#include "multi_queue.h"

/*  Init multi-level queue.  */
void mq_init(struct multi_queue* mq) {
    for(int i = PRI_MIN; i <= PRI_MAX; i++)
        list_init(mq->queue + i);
}

/*  Is the multi-level queue empty?  */  
bool mq_empty(struct multi_queue* mq) {
    for(int i = PRI_MAX; i >= PRI_MIN; i--)
        if(!list_empty(mq->queue + i))
            return false;
    return true;
}

/*  Pop the frontmost elem with highest priority. 
    If mq is empty, this returns NULL.
    If priority is not NULL, save the priority of the queue which returning elem is from.
*/
struct list_elem* mq_pop_high_front(struct multi_queue* mq, int* priority) {
    for(int i = PRI_MAX; i >= PRI_MIN; i--) {
        if(!list_empty(mq->queue + i)) {
            if(priority)
                *priority = i;
            return list_pop_front(mq->queue + i);
        }
    }
    return NULL;
}

/*  Push the elem into the back of list of corresponding priority.  */
void mq_push_back(struct multi_queue* mq, struct list_elem* e, int priority) {
    list_push_back(mq->queue + priority, e);
}
