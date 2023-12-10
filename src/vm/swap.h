#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/vaddr.h"
#include <kernel/list.h>
#include "devices/block.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)
#define FLAG_BITS       (SECTOR_PER_PAGE - 1)
#define SECTOR_BITS     (~FLAG_BITS)

struct swap_elem {
    struct list_elem elem;
    block_sector_t off;
};

struct list swap_table;
struct block* swap_disk;

void swap_table_init(void);
block_sector_t get_swap_slot(void);
void return_swap_slot(block_sector_t slot);

#endif