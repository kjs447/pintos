#include "multi_queue.h"

/*  Init multi-level queue.  */
void mq_init(struct multi_queue* mq) {
    mq->highest_priority = -1;
    for(int i = PRI_MIN; i <= PRI_MAX; i++)
        list_init(mq->queue + i);
}

/*  Is the multi-level queue empty?  */  
bool mq_empty(struct multi_queue* mq) {
    return mq->highest_priority == -1;
}

/*  Find highest priority of threads by binary search in [begin, end].  */
static int mq_highest_priority(struct multi_queue* mq, int begin, int end) {
    for(int i = end; i >= begin; i--) {
        if(list_empty(mq->queue + i))
            continue;
        return i;
    }
    return -1;
}

/*  Pop the frontmost elem with highest priority. 
    If mq is empty, this returns NULL.
*/
struct list_elem* mq_pop_high_front(struct multi_queue* mq) {
    if(mq_empty(mq))
        return NULL;
    struct list_elem* res = list_pop_front(mq->queue + mq->highest_priority);
    if(list_empty(mq->queue + mq->highest_priority))
        mq->highest_priority = mq->highest_priority == PRI_MIN ?
            -1 : mq_highest_priority(mq, PRI_MIN, mq->highest_priority - 1);
    return res;
}

/*  Push the elem into the back of list of corresponding priority.  */
void mq_push_back(struct multi_queue* mq, struct list_elem* e, int priority) {
    list_push_back(mq->queue + priority, e);
    if(mq->highest_priority < priority)
        mq->highest_priority = priority;
}
