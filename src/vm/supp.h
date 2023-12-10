#ifndef VM_SUPP_H
#define VM_SUPP_H

#include <stdint.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"
#include "devices/block.h"
#include "vm/frame.h"

struct file_supp_data {
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};

struct swap_supp_data {
    block_sector_t sec;
};

union supp_data {
    struct file_supp_data file;
    struct swap_supp_data swap;
};

enum supp_type {
    CODE_FILE_SUPP,
    SWAP_SUPP
};

struct page {
    struct hash_elem hash_elem;
    void* addr;
    enum supp_type type;
    union supp_data data;
    bool writable;
};

void supp_pages_init (struct hash* supp_table);

/* Returns a hash value for page p. Reference: pintos manual */
unsigned page_hash (const struct hash_elem *p_, void *aux);

/* Returns true if page a precedes page b. Reference: pintos manual */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);

/* Returns the page containing the given virtual address,
or a null pointer if no such page exists. Reference: pintos manual */
struct page *page_lookup (struct hash* supp_table, const void *address);

bool save_file_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool save_swap_segment (block_sector_t sec, struct hash* supp_table, uint8_t *upage, bool writable);
bool install_page (struct thread* t, void *upage, void *kpage, bool writable);

void page_free (struct hash* supp_table, struct hash_elem* e);
void page_all_free (struct hash* supp_table);
bool lazy_load_segment (struct hash* supp_table, uint8_t *upage);

#endif