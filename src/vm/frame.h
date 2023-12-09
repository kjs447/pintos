#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/pte.h"
#include "threads/palloc.h"
#include <stdbool.h>
#include <list.h>
#include "threads/malloc.h"

#define ENTRYNUM (1 << PTBITS)

struct frame {
    struct list_elem elem;
    uint32_t* pd;
    void* kaddr;
};

void init_frame_list(void);
void push_frame(uint32_t* pd, void* kaddr);
void* pop_frame(struct frame* f);
void pop_frame_free(struct frame* f);
void pop_frame_pd(uint32_t* pd);
struct frame* clock_advance(void);

#endif