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

    bool success = true;
    uint8_t* upage_src = upage;
    uint32_t cnt = 0;

    while (read_bytes > 0 || zero_bytes > 0) 
        {
        struct page* pg = malloc(sizeof(struct page));

        if(!pg) {
            success = false;
            break;
        }

        /* Calculate how to fill this page.
            We will read PAGE_READ_BYTES bytes from FILE
            and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        pg->addr = upage;
        pg->type = CODE_FILE_SUPP;
        pg->data.file.file = file;
        pg->data.file.ofs = page_read_bytes ? ofs : 0;
        pg->data.file.read_bytes = page_read_bytes;
        pg->data.file.zero_bytes = page_zero_bytes;
        pg->writable = writable;

        hash_insert(supp_table, &pg->hash_elem);

        /* Advance. */
        ofs += page_read_bytes;
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        cnt++;
        }

    if(!success) {
        for(uint32_t i = 0; i < cnt; i++) {
            struct page* pg = page_lookup(supp_table, upage_src + i * PGSIZE);
            page_free(&pg->hash_elem, NULL);
        }
    }

    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (struct thread* t, void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  bool success = pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable);
  if(success) {
    push_frame(t->pagedir, upage, kpage);
  }
  return success;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
bool
load_lazy_segment (struct hash* supp_table, uint8_t *upage, bool* alloc_fail) {
    ASSERT(upage >= 0x8048000);

    struct page* pg = page_lookup(supp_table, upage);
    if(!pg) return false;

    struct thread* t = thread_current();

    ASSERT(pg->type == CODE_FILE_SUPP);
    upage = pg->addr;
    uint32_t read_bytes = pg->data.file.read_bytes;
    uint32_t zero_bytes = pg->data.file.zero_bytes;
    off_t ofs = pg->data.file.ofs;
    struct file* file = pg->data.file.file;
    bool writable = pg->writable;

    ASSERT ((read_bytes + zero_bytes) == PGSIZE);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0);

    file_seek (file, ofs);

    /* Get a page of memory. */
    uint8_t *kpage = palloc_get_page (PAL_USER);
    if (kpage == NULL) {
        if(alloc_fail)
            *alloc_fail = true;
        return false;
    }

    bool success = true;

    /* Load this page. */
    if (file_read (file, kpage, read_bytes) != (int) read_bytes) {
        if(alloc_fail)
            *alloc_fail = false;
        success = false;
    }
    memset (kpage + read_bytes, 0, zero_bytes);

    /* Add the page to the process's address space. */
    if (success && !install_page (t, upage, kpage, writable)) {
        if(alloc_fail)
            *alloc_fail = false;
        success = false;
    }

    if(!success) {
        palloc_free_page (kpage);
    } else {
        page_free (supp_table, &pg->hash_elem);
    }
    return success;
}

static void page_free_ (struct hash_elem *e, void* aux UNUSED) {
    struct page* pg = hash_entry(e, struct page, hash_elem);
    free(pg);
}

void page_free (struct hash* supp_table, struct hash_elem* e) {
    struct page* pg = hash_entry(hash_delete(supp_table, e), struct page, hash_elem));
    free(pg);
}

void page_all_free (struct hash* supp_table) {
    hash_destroy(supp_table, page_free_);
}



