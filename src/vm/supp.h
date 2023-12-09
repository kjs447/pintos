#ifndef VM_SUPP_H
#define VM_SUPP_H

#include <stdint.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"
#include "frame.h"

struct file_supp_data {
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};

union supp_data {
    struct file_supp_data file;
};

enum supp_type {
    FILE_SUPP,
    SWAP_SUPP
};

struct page_content {
    uint32_t cnt;
    enum supp_type type;
    union supp_data data;
    bool writable;
};

struct page {
    struct hash_elem hash_elem;
    void* addr;
    struct page_content* content;
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
bool load_lazy_segment (struct hash* supp_table, uint8_t *upage);
void page_free (struct hash_elem *e, void* aux);
void page_all_free (struct hash* supp_table);

#endif