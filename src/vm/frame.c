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

struct frame* push_frame(struct page* page, void* kaddr) {
    struct frame* fptr = (struct frame*)malloc(sizeof(struct frame));
    fptr->page = page;
    fptr->kaddr = kaddr;
    enum intr_level old = intr_disable();
    list_push_back(&frame_list, &fptr->elem);
    intr_set_level(old);
}

struct frame* pop_front_frame_elem() {
    enum intr_level old = intr_disable();
    struct frame* f = list_entry(list_pop_front(&frame_list), struct frame, elem);
    intr_set_level(old);
    return f;
}
