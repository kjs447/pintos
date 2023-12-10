#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/pte.h"
#include <stdbool.h>
#include <list.h>

#define ENTRYNUM (1 << PTBITS)

struct frame {
    struct list_elem elem;
    uint32_t* pd;
    void* uaddr;
    void* kaddr;
    struct hash* supp_table;
    bool writable;
};

void init_frame_list(void);
void push_frame(struct hash* supp_table, uint32_t* pd, void* uaddr, void* kaddr, bool writable);
void modify_frame(struct frame* fptr, struct hash* supp_table, uint32_t* pd, void*uaddr, void* kaddr, bool writable);
void pop_frame_pd(uint32_t* pd);
struct frame* clock_advance(void);
struct frame* evict(void);

#endif