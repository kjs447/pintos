#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/pte.h"
#include <stdbool.h>
#include <list.h>
#include "threads/palloc.h"

#define ENTRYNUM (1 << PTBITS)

struct frame {
    struct list_elem elem;
    struct page* page;
    void* kaddr;
};

void init_frame_list(void);
struct frame* push_frame(struct page* page, void* kaddr);
struct frame* pop_front_frame_elem(void);

#endif