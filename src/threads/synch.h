#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>
#include <multi_queue.h> // prj3: multi-level queue

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct multi_queue waiters;        /* List of waiting threads. prj3: multi-level queue */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition 
  {
    struct multi_queue waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* TODO: Single Variable Consumer-Producer 
   this variable regards 0 as empty.   
*/
struct single_pc
  {
    int value;
    struct semaphore sem;
    struct semaphore full;
    struct semaphore empty;
  };

void single_pc_init(struct single_pc*);
void single_pc_producer(struct single_pc*, int);
int single_pc_consumer(struct single_pc*);

/* TODO prj2: Reader-Writer Problem */
struct rw_sem 
  {
    int rcount;
    struct lock queue_lock;
    struct lock rcount_lock;
    struct semaphore access_sem;
  };

void rw_sem_init(struct rw_sem*);
void rw_reader_begin(struct rw_sem*);
void rw_reader_end(struct rw_sem*);
void rw_writer_begin(struct rw_sem*);
void rw_writer_end(struct rw_sem*);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
