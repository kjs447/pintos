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
    int highest_priority;               /* Highest priority among threads. */
    struct list queue[64];              /* Multi-Level Queue. */
};

void mq_init(struct multi_queue*);
bool mq_empty(struct multi_queue*);

struct list_elem* mq_pop_high_front(struct multi_queue*);
void mq_push_back(struct multi_queue*, struct list_elem*, int priority);

#endif /* lib/kernel/multi_queue.h */