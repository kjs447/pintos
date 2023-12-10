#include "swap.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"

void swap_table_init(void) {
    list_init(&swap_table);
    swap_disk = block_get_role(BLOCK_SWAP);
}

static bool 
off_less (const struct list_elem *a,
                    const struct list_elem *b,
                    void *aux UNUSED) {
    return list_entry(a, struct swap_elem, elem)->off 
        < list_entry(b, struct swap_elem, elem)->off;
}

block_sector_t get_swap_slot(void) {
    enum intr_level old = intr_disable();
    block_sector_t i;
    struct list_elem* p = list_begin(&swap_table);
    for(i = 0; i < block_size(swap_disk); i += SECTOR_PER_PAGE) {
        if(i == list_entry(p, struct swap_elem, elem)->off) {
            p = list_next(p);
            continue;
        }
        struct swap_elem* e = malloc(sizeof(struct swap_elem));
        e->off = i;
        list_insert_ordered(&swap_table, &e->elem, off_less, NULL);
        intr_set_level (old);
        return i;
    }
    ASSERT(0 && "No more swap slot!");
}

void return_swap_slot(block_sector_t slot) {
    for(struct list_elem* e = list_begin(&swap_table);
        e != list_end(&swap_table);
        e = list_next(e)) {
        if(list_entry(e, struct swap_elem, elem)->off == slot) {
            list_remove(e);
            free(list_entry(e, struct swap_elem, elem));
            return;
        }
    }
}