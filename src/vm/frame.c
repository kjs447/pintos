#include "frame.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"
#include "swap.h"
#include "supp.h"

struct list frame_list;
int frame_num;
struct frame* clock_ptr;

void init_frame_list(void) {
    list_init(&frame_list);
    frame_num = 0;
    clock_ptr = (struct frame*)NULL;
}

void push_frame(struct hash* supp_table, uint32_t* pd, void* uaddr, void* kaddr, bool writable) {
    struct frame* fptr = (struct frame*)malloc(sizeof(struct frame));
    modify_frame(fptr, supp_table, pd, uaddr, kaddr, writable);
    enum intr_level old = intr_disable();
    list_push_back(&frame_list, &fptr->elem);
    intr_set_level(old);
    if(!(frame_num++)) {
        clock_ptr = fptr;
    }
}

void modify_frame(struct frame* fptr, struct hash* supp_table, uint32_t* pd, void* uaddr, void* kaddr, bool writable) {
    fptr->supp_table = supp_table;
    fptr->pd = pd;
    fptr->uaddr = uaddr;
    fptr->kaddr = kaddr;
    fptr->writable = writable;
}

static struct list_elem* pop_frame_elem(struct frame* f) {
    if(!(--frame_num))
        clock_ptr = (struct frame*)NULL;
    else if(f == clock_ptr)
        clock_advance();
    enum intr_level old = intr_disable();
    struct list_elem* e = list_remove(&f->elem);
    intr_set_level(old);
    return e;
}

void pop_frame_pd(uint32_t* pd) {
    enum intr_level old = intr_disable();
    struct list_elem* e;
    for(e = list_begin(&frame_list);
        e != list_end(&frame_list);) {
            if(!frame_num)
                return;
            struct frame* f = list_entry(e, struct frame, elem);
            if(f->pd == pd) {
                e = pop_frame_elem(f);
                free(f);
            } else {
                e = list_next(e);
            }
            if(!e->next) break;
        }
    intr_set_level(old);
}

struct frame* clock_advance() {
    ASSERT(clock_ptr);

    enum intr_level old = intr_disable();
    if(list_rbegin(&frame_list) == &clock_ptr->elem)
        clock_ptr = list_entry(list_begin(&frame_list), struct frame, elem);
    else {
        clock_ptr = list_entry(list_next(&clock_ptr->elem), struct frame, elem);
    }
    intr_set_level(old);
    return clock_ptr;
}

static struct frame*
second_chance (void) {
    while (true) {
        if(pagedir_is_accessed(clock_ptr->pd, clock_ptr->uaddr)) {
            pagedir_set_accessed(clock_ptr->pd, clock_ptr->uaddr, false);
            clock_advance();
        } else {
            struct frame* cptr = clock_ptr;
            clock_advance();
            return cptr;
        }

    }
}

struct frame* evict(void) {
    enum intr_level old = intr_disable();
    struct frame* victim = second_chance();
    block_sector_t off = get_swap_slot();
    struct block* block = swap_disk;
    char* buf = victim->kaddr;
    for(int i = 0; i < 8; i++) {
        block_write(block, off, buf);

        /* advance */
        off++;
        buf += 512;
    }
    buf = victim->kaddr;

    pagedir_clear_page(victim->pd, victim->uaddr);
    save_swap_segment(off, victim->supp_table, victim->uaddr, victim->writable);
    intr_set_level(old);

    return victim;
}