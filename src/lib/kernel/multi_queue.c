#include "multi_queue.h"

/*  Init multi-level queue.  */
void mq_init(struct multi_queue* mq) {
    mq->num = 0;
    list_init(&mq->queue);
}

/*  Is the multi-level queue empty?  */  
bool mq_empty(struct multi_queue* mq) {
    return !mq->num;
}

/*  How many objects in multi-level queue?  */
int mq_num(struct multi_queue* mq) {
    return mq->num;
}

/*  Pop the frontmost elem with highest priority. 
    If mq is empty or not a list, undefined behavior.
*/
struct list_elem* mq_pop_high_front(struct multi_queue* mq) {
    mq->num--;
    return list_pop_front(&mq->queue);
}

/*  Push the elem into the back of list of corresponding priority.  */
void mq_push_back(struct multi_queue* mq, struct list_elem* e
    , bool (*priority_gt) (const struct list_elem *a
    , const struct list_elem *b, void *aux)) {
    mq->num++;
    list_insert_ordered(&mq->queue, e, priority_gt, NULL);
}
