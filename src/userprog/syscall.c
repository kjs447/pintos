#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "lib/kernel/stdio.h"
#include "pagedir.h"
#include "userprog/syscall.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* TODO: check for lowest and highest address of variable list is in user (Modified in prj2) */
#define CHK_ARG_NUM(f, num) do { \
  if(is_kernel_vaddr((unsigned char*)(f->esp) + 3)) \
    exit(-1); \
  if(is_kernel_vaddr((unsigned char*)(f->esp) + (((num) + 1) * 4) - 1)) \
    exit(-1); \
} while(0)

#define ARG(f, type, idx) \
*(type*)((unsigned char*)(f->esp) + ((idx) + 1) * 4)

#define CHK_STR(f, idx) do {    \
  const char* cptr = ARG(f, const char*, idx);   \
  for(; is_user_vaddr(cptr) && *cptr; cptr++);   \
  if(is_kernel_vaddr(cptr))                       \
    exit(-1);                                 \
} while(0)

#define CHK_BUF_SZ(f, idx, sz) do {    \
  const char* cptr = ARG(f, const char*, idx);           \
  unsigned i;                                         \
  for(i = 0; is_user_vaddr(cptr + i) && i < sz; i++);    \
  if(is_kernel_vaddr(cptr))                               \
    exit(-1);                                         \
} while(0)

/* TODOs: system call implementation */
static void
halt(void) {
  shutdown_power_off();
}

void
exit(int status) {
  struct thread* th = thread_current();
  char name[16];
  int i;
  for(i = 0; i < sizeof th->name; i++)
    if(!th->name || th->name[i] == ' ')
      break;
    else
      name[i] = th->name[i];
  name[i] = '\0';
  printf("%s: exit(%d)\n", name, status);

  struct list_elem* childp;
  for(childp = list_begin(&th->child_list);
    childp != list_end(&th->child_list);
    childp = list_next(childp))
    get_thread_from_childelem(childp)->parent = NULL;

  th->terminated_by_exit = true;
  th->exit_status = status;
  sema_up(&th->to_wait);
  sema_down(&th->to_th_exit);
  thread_exit();
  NOT_REACHED ();
}



/* TODO: Project 1 additional syscall */
static int
fibonacci(int n) {
  int a0 = 1, a1 = 1, tmp, i;
  for(i = 0; i < n - 2; i++) {
    tmp = a0 + a1;
    a0 = a1;
    a1 = tmp;
  }
  return a1;
}

static int
max_of_four_int(int a, int b, int c, int d) {
  int max = a >= b ? a : b;
  max = max >= c ? max : c;
  return max = max >= d ? max : d;
}

/* TODO prj2: syscalls for files */
static bool
create (const char *file, unsigned initial_size) {
  return filesys_create(file, (off_t)initial_size);
}

static bool
remove (const char *file) {
  return filesys_remove(file);
}

static int
open (const char *file) {
  int fd = get_new_fd(&thread_current()->fd_list);
  if(fd == -1)
    return -1;
  struct file* fp = filesys_open(file);
  if(!fp)
    return -1;
  fd_create(fd, fp, &thread_current()->fd_list);
  return fd;
}

static int
filesize (int fd) {
  struct file* f = get_file_from_fd(fd);
  if(!f) return -1;
  return file_length(f);
}

static void
seek (int fd, unsigned position) {
  struct file* f = get_file_from_fd(fd);
  if(!f) return;
  file_seek(f, (off_t)position);
}

static unsigned
tell (int fd) {
  struct file* f = get_file_from_fd(fd);
  if(!f) return 0;
  return (unsigned)file_tell(f);
}

static void
close (int fd) {
  if(fd == 0 || fd == 1)
    return;
  struct file* f = remove_fd_elem(fd);
  if(!f) return;
  file_close(f);
}

static int
read (int fd, void *buffer, unsigned size) {
  if(fd != 0) {
    struct file* f = get_file_from_fd(fd);
    if(!f) return -1;
    return file_read(f, buffer, (off_t)size);
  }
  char* buf = (char*)buffer;
  unsigned i;
  for(i = 0; i < size; i++) {
    if((buf[i] = (char)input_getc()) == 0x04) {
      break;
    }
  }
  return i;
}

static int
write (int fd, const void *buffer, unsigned size) {
  if(fd != 1) {
    struct file* f = get_file_from_fd(fd);
    if(!f) return -1;
    return file_write(f, buffer, (off_t)size);
  }
  putbuf(buffer, size);
  return size;
}

static void
syscall_handler (struct intr_frame *f) 
{
  if(is_kernel_vaddr(f->esp) || is_kernel_vaddr((char *)f->esp + 3)) 
    exit(-1);
  int num = *(int*)f->esp;

  switch (num) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      CHK_ARG_NUM(f, 1);
      exit(ARG(f, int, 0));
      break;
    case SYS_EXEC:
      CHK_ARG_NUM(f, 1);
      CHK_STR(f, 0);
      f->eax = process_execute(ARG(f, const char*, 0));
      break;
    case SYS_WAIT:
      CHK_ARG_NUM(f, 1);
      f->eax = process_wait(ARG(f, pid_t, 0));
      break;
    case SYS_FIBO:
      CHK_ARG_NUM(f, 1);
      f->eax = fibonacci(ARG(f, int, 0));
      break;
    case SYS_MAX_FOUR: 
      CHK_ARG_NUM(f, 4);
      f->eax = max_of_four_int(ARG(f, int, 0), ARG(f, int, 1), ARG(f, int, 2), ARG(f, int, 3));
      break;
    /* TODO prj2 */
    case SYS_CREATE:
      CHK_ARG_NUM(f, 2);
      CHK_STR(f, 0);
      f->eax = create(ARG(f, const char*, 0), ARG(f, unsigned, 1));
      break;
    case SYS_REMOVE:
      CHK_ARG_NUM(f, 1);
      CHK_STR(f, 0);
      f->eax = remove(ARG(f, const char*, 0));
      break;
    case SYS_OPEN:
      CHK_ARG_NUM(f, 1);
      CHK_STR(f, 0);
      f->eax = open(ARG(f, const char*, 0));
      break;
    case SYS_CLOSE:
      CHK_ARG_NUM(f, 1);
      close(ARG(f, int, 0));
      break;
    case SYS_FILESIZE:
      CHK_ARG_NUM(f, 1);
      f->eax = filesize(ARG(f, int, 0));
      break;
    case SYS_READ:
      CHK_ARG_NUM(f, 3);
      CHK_BUF_SZ(f, 1, ARG(f, unsigned, 2));
      f->eax = read(ARG(f, int, 0), ARG(f, void*, 1), ARG(f, unsigned, 2));
      break;
    case SYS_WRITE:
      CHK_ARG_NUM(f, 3);
      CHK_BUF_SZ(f, 1, ARG(f, unsigned, 2));
      f->eax = write(ARG(f, int, 0), ARG(f, const void*, 1), ARG(f, unsigned, 2));
      break;
    case SYS_SEEK:
      CHK_ARG_NUM(f, 2);
      seek(ARG(f, int, 0), ARG(f, unsigned, 1));
      break;
    case SYS_TELL:
      CHK_ARG_NUM(f, 1);
      f->eax = tell(ARG(f, int, 0));
      break;
  }
}

