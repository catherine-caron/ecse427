#ifndef COMP310_A2_Q
#define COMP310_A2_Q

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

/* queue node/entry object */
struct queue_entry {
    void *data;
    STAILQ_ENTRY(queue_entry) entries; /* add to struct of all entries */
};

STAILQ_HEAD(queue, queue_entry); 

/* create a queue */
struct queue queue_create() { 
    struct queue q = STAILQ_HEAD_INITIALIZER(q);
    return q;
}

/* initialize the queue */
void queue_init(struct queue *q) {
    STAILQ_INIT(q);
}

/* deal with errors */
void queue_error() {
    fprintf(stderr, "Fatal error in queue operations\n");
    exit(1);
}

/* make new node/entry */
struct queue_entry *queue_new_node(void *data) {
    struct queue_entry *entry = (struct queue_entry*) malloc(sizeof(struct queue_entry));
    if(!entry) {
        queue_error();
    }
    entry->data = data;
    return entry;
}

/* insert at head: not used for FIFO */
void queue_insert_head(struct queue *q, struct queue_entry *e) { 
    STAILQ_INSERT_HEAD(q, e, entries);
}

/* insert at tail whenever we get a new node/entry */
void queue_insert_tail(struct queue *q, struct queue_entry *e) {
    STAILQ_INSERT_TAIL(q, e, entries); 
}

/* check what's next in the queue */
struct queue_entry *queue_peek_front(struct queue *q) { 
    return STAILQ_FIRST(q);
}

/* check and pop what's next in the queue
*  return null if queue is empty
*/
struct queue_entry *queue_pop_head(struct queue *q) { 
    struct queue_entry *elem = queue_peek_front(q);
    if(elem) {
        STAILQ_REMOVE_HEAD(q, entries);
    }
    return elem;
}

#endif
