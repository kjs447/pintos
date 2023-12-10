#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h" // prj4 : frame table
#include "vm/supp.h"  // prj4 : supplemental page table

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
    
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  /* TODO : get child and put into child list */
  struct thread* par = thread_current();
  struct list_elem* listp;
  struct thread* new_th;
  for(listp = thread_list_rbegin();
    listp != thread_list_rend(); listp = list_prev(listp)) {
    if(list_entry(listp, struct thread, allelem)->tid == tid) {
      new_th = list_entry(listp, struct thread, allelem);
      break;
    }
  }

  int res = single_pc_consumer(&new_th->exec_status);
  if(res == -1)
    return -1;

  list_push_back(&par->child_list, &new_th->child_elem);
  new_th->parent = par;
  
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  
  palloc_free_page (file_name);

  struct thread* cur = thread_current();
  single_pc_producer(&cur->exec_status, success ? 1 : -1);

  /* If load failed, quit. */
  if (!success) 
    exit(-1);
  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread* this = thread_current();
  
  if(child_tid <= 0)
    return -1;
  
  struct list_elem* childp;
  for(childp = list_begin(&this->child_list);
    childp != list_end(&this->child_list);
    childp = list_next(childp)) {
    if(list_entry(childp, struct thread, child_elem)->tid == child_tid)
      break;
  }

  if(childp == list_end(&this->child_list))
    return -1;

  struct thread* child = list_entry(childp, struct thread, child_elem);
  sema_down(&child->to_wait);
  
  list_remove(childp);

  int res = child->terminated_by_exit ? child->exit_status : -1;

  /* if child process be reaped, move up allowing write and closing file*/
  file_close(child->file);       /* TODO prj2: delay file closing, and allow file write*/
  child->file = NULL;

  sema_up(&child->to_th_exit);

  return res;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* if cur->file is not NULL, this is not reaped. */
  if(cur->file)
    file_close(cur->file);       /* TODO prj2: delay file closing, and allow file write*/

  /* TODO prj2: free fd list */
  while(!list_empty(&cur->fd_list)) {
    struct list_elem* e = list_pop_front(&cur->fd_list);
    free_fd_elem(e);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pop_frame_pd (pd); // prj4: frame table
      page_all_free(&cur->supp_table); // prj4: supplemental page table
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  t->file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  /* defined variables for TODO1*/
  char str[128], *tk[64] = {str}, *argv_head;
  int len = 1, tk_total_len = 0;
  bool end_by_space = false;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  supp_pages_init(&t->supp_table);
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* TODO1 : parse file name*/
  for(i = 0; file_name[i] && file_name[i] == ' ' && i < 128; ++i);
  for(; file_name[i] && i < 128;) {
    if(file_name[i] == ' ') {
      str[tk_total_len++] = '\0';
      for(; file_name[i] == ' ' && i < 128; ++i);
      tk[len++] = str + tk_total_len;
      end_by_space = true;
    } else {
      str[tk_total_len++] = file_name[i++]; 
      end_by_space = false;
    }
  }
  if(!end_by_space) {
    str[tk_total_len] = '\0';
    ++tk_total_len;
  } else {
    len--;
  }

  /* Open executable file. */
  t->file = filesys_open (tk[0]); /* modified */
  if (t->file == NULL) 
    {
      printf ("load: %s: open failed\n", tk[0]); /* modified */
      goto done; 
    }
  
  file_deny_write(t->file); /* TODO prj2: deny write of executing file */

  /* Read and verify executable header. */
  if (file_read (t->file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", tk[0]); /* modified */
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (t->file))
        goto done;
      file_seek (t->file, file_ofs);

      if (file_read (t->file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, t->file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!save_file_segment (t->file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  // prj4: save esp
  t->old_stack_pg = ((uint8_t *) PHYS_BASE) - PGSIZE;

  /* TODO2: construct stack */
  /* argv's copy*/
  memcpy(argv_head = (*(char**)esp -= tk_total_len), str, tk_total_len);

  /* word-align */
  tk_total_len %= 4;
  if(tk_total_len) {
    tk_total_len = 4 - tk_total_len;
    memset(*(char**)esp -= tk_total_len, 0x00, tk_total_len);
  }

  /* argv array */
  *(--*(char***)esp) = NULL;
  for(i = len - 1; i >= 0; i--) {
    *(--*(char***)esp) = argv_head + (tk[i] - str);
  }
  
  /* ptr of argv array, argc, return addr */
  *(*(char****)esp - 1) = *esp;
  *(*(int**)esp -= 2) = len;
  *(--*(void (***) (void))esp) = NULL;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true; 

 done:
  /* We arrive here whether the load is successful or not. */
  if(!success) {
    file_close (t->file); /* TODO prj2: associate file and process. thus, don't close if succeed, and allow file write */
  }
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  struct frame* f;
  kpage = palloc_get_page_evict (PAL_USER | PAL_ZERO, &f);
  if (kpage != NULL) 
    {
      success = install_page (thread_current(), ((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true, f);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}