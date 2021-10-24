#include "queue.h" // use homemade queue library
#include <stdio.h>
#include <unistd.h>

#include <pthread.h> 

int main() {
    
    int x = 1;
    int y = 2;
    int z = 3;

    /* Note: it is important that you do NOT move/copy this value
     * after the list has been initialized. Always refer to it by
     * reference/through a pointer indirection */
    struct queue q = queue_create();
    queue_init(&q); // use pointer
    
    struct queue_entry *node = queue_new_node(&x); // queue: x
    queue_insert_tail(&q, node); // insert new stuff at the tail
    
    struct queue_entry *node2 = queue_new_node(&y); // queue: y -> x
    queue_insert_tail(&q, node2);
    
    struct queue_entry *node3 = queue_new_node(&z); // queue z -> y -> x
    queue_insert_tail(&q, node3);

    struct queue_entry *ptr = queue_pop_head(&q); // pop next in line: x | so queue: z -> y
    while(ptr) { // iterate and pop everything
        printf("popped %d\n", *(int*)ptr->data);  // info
        
        queue_insert_tail(&q, ptr); // queue: x -> z -> y
        usleep(1000 * 1000);
        
        ptr = queue_pop_head(&q); // pop head | queue: x -> z 
    } // infinite loop to push and pop the queue. should never end!
    
    return 0;
}
