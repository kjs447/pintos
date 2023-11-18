#ifndef __LIB_KERNEL_MULTI_QUEUE_H
#define __LIB_KERNEL_MULTI_QUEUE_H

/*  Queue implementation with priority multi-level queue
 
    This implements a multi level list with 64 lists by their priorities.
*/
#include "list.h"

/* Flag: is the scheduler of BSD? */
extern bool thread_mlfqs;

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Multi-level Queue. */
struct multi_queue {
    int num;               /* How many objects in multi-level queue? */
    struct list queue;              /* Multi-Level Queue. */
};

void mq_init(struct multi_queue*);
bool mq_empty(struct multi_queue*);
int mq_num(struct multi_queue*);

struct list_elem* mq_pop_high_front(struct multi_queue*);
void mq_push_back(struct multi_queue* mq, struct list_elem* e
    , bool (*priority_gt) (const struct list_elem *a
    , const struct list_elem *b, void *aux));

#endif /* lib/kernel/multi_queue.h */