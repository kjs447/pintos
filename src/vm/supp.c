#include "supp.h"
#include "threads/init.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include <string.h>
#include <stdio.h>

void
supp_pages_init (struct hash* supp_table) {
    hash_init(supp_table, page_hash, page_less, NULL);
}

/* Returns a hash value for page p. Reference: pintos manual */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
    const struct page *p = hash_entry (p_, struct page, hash_elem);
    return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. Reference: pintos manual */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
            void *aux UNUSED) {
    const struct page *a = hash_entry (a_, struct page, hash_elem);
    const struct page *b = hash_entry (b_, struct page, hash_elem);
    return a->addr < b->addr;
}

/* Returns the page containing the given virtual address,
or a null pointer if no such page exists. Reference: pintos manual */
struct page *
page_lookup (struct hash* supp_table, const void *address)
{
    struct page p;
    struct hash_elem *e;
    p.addr = address;
    e = hash_find (supp_table, &p.hash_elem);
    return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

bool
save_file_segment(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0);

    struct hash* supp_table = &thread_current()->supp_table;

    struct page_content* pg_cont = malloc(sizeof(struct page_content));
    if(!pg_cont) return false;

    pg_cont->type = FILE_SUPP;
    pg_cont->data.file.file = file;
    pg_cont->data.file.ofs = ofs;
    pg_cont->data.file.read_bytes = read_bytes;
    pg_cont->data.file.zero_bytes = zero_bytes;
    pg_cont->writable = writable;
    pg_cont->cnt = 0;

    bool success = true;

    uint32_t pg_cnt = (read_bytes + zero_bytes) / PGSIZE;
    for(uint32_t i = 0; i < pg_cnt; i++) {
        struct page* pg = malloc(sizeof(struct page));
        if(!pg) {
            success = false;
            break;
        }

        pg_cont->cnt++;
        pg->addr = upage + i * PGSIZE;
        pg->content = pg_cont;
        hash_insert(supp_table, &pg->hash_elem);
    }

    if(!success) {
        for(uint32_t i = 0; i < pg_cont->cnt; i++) {
            struct page* pg = page_lookup(supp_table, upage + i * PGSIZE);
            page_free(&pg->hash_elem, NULL);
        }
        free(pg_cont);
    }

    return success;
}

static bool
install_page (struct thread* t, void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  bool success = pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable);
  if(success) {
    push_frame(t->pagedir, kpage);
  }
  return success;
}

bool
load_lazy_segment (struct hash* supp_table, uint8_t *upage) {
    ASSERT(upage >= 0x8048000);

    struct page* pg = page_lookup(supp_table, upage);
    if(!pg) return false;

    struct thread* t = thread_current();

    ASSERT(pg->content->type == FILE_SUPP);
    uint32_t read_bytes = pg->content->data.file.read_bytes;
    uint32_t zero_bytes = pg->content->data.file.zero_bytes;
    off_t ofs = pg->content->data.file.ofs;
    struct file* file = pg->content->data.file.file;
    bool writable = pg->content->writable;

    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0);

    file_seek (file, ofs);
    
    while (read_bytes > 0 || zero_bytes > 0) 
        {
        /* Calculate how to fill this page.
            We will read PAGE_READ_BYTES bytes from FILE
            and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Get a page of memory. */
        uint8_t *kpage = palloc_get_page (PAL_USER);
        if (kpage == NULL)
            return false;

        /* Load this page. */
        if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
            {
            palloc_free_page (kpage);
            return false; 
            }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page (t, upage, kpage, writable)) 
            {
            palloc_free_page (kpage);
            return false; 
            }

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        }
    return true;
}

void page_free (struct hash_elem *e, void* aux UNUSED) {
    struct page* pg = hash_entry(e, struct page, hash_elem);
    struct page_content* pg_cont = pg->content;
    free(pg);
    if(!(--pg_cont->cnt))
        free(pg_cont);
}

void page_all_free (struct hash* supp_table) {
    hash_destroy(supp_table, page_free);
}



