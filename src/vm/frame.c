#include "frame.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"
#include "swap.h"
#include "supp.h"
#include <debug.h>

struct list frame_list;

void init_frame_list(void) {
    list_init(&frame_list);
}

void push_frame(struct hash* supp_table, uint32_t* pd, void* uaddr, void* kaddr, bool writable) {
    struct frame* fptr = (struct frame*)malloc(sizeof(struct frame));
    modify_frame(fptr, supp_table, pd, uaddr, kaddr, writable);
    enum intr_level old = intr_disable();
    list_push_back(&frame_list, &fptr->elem);
    intr_set_level(old);
}

void modify_frame(struct frame* fptr, struct hash* supp_table, uint32_t* pd, void* uaddr, void* kaddr, bool writable) {
    fptr->supp_table = supp_table;
    fptr->pd = pd;
    fptr->uaddr = uaddr;
    fptr->kaddr = kaddr;
    fptr->writable = writable;
}

struct list_elem* pop_frame_elem(struct frame* f) {
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

void evict(void) {
    enum intr_level old = intr_disable();
    struct frame* victim = list_entry(list_pop_front(&frame_list), struct frame, elem);
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
    free(victim);
    intr_set_level(old);

    return victim;
}