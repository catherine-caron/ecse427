#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include "sut.h"
#include "queue.h"

/* 
*   The Simple User-Level Thread Library (SUT)
*   By Catherine Caron (ID 260762540)
*/

int c_exec_number = 1;                  /* manually set this value to 2 to use two C-EXEC threads (for part 2)          */

pthread_t C_EXEC, I_EXEC, C2_EXEC;      /* kernel threads                                                               */
ucontext_t c_exec_context1;             /* userlevel context for context switching  - first C-EXEC thread               */
ucontext_t c_exec_context2;             /* userlevel context for context switching  - second C-EXEC thread              */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;      /* semaphore lock for queue                                     */
struct queue c_exec_queue, i_exec_queue, wait_queue;    /* ready queue for each kernel thread and wait queue for I-EXEC */
struct queue_entry *ptr1;                /* queue node/entry                                                             */
struct queue_entry *ptr2;                /* queue node/entry                                                             */
int sleeping_time = 1000;               /* wait time before checking again when queue is empty                          */

/* C-EXEC global variables */
int thread_number;                      /* counter                          */
threaddesc thread_array[MAX_THREADS], *tdescptr; 

/* I-EXEC global variables */
iodesc *iodescptr;                      /* I-EXEC object                    */
int fd;                                 /* file descriptor number           */
int bytes;                              /* number of bytes written/read     */

/* Shutdown flags */
bool create_flag = false;               /* creation of task     */
bool open_flag = false;                 /* opening file         */
bool close_flag = false;                 /* closing file         */
bool c1_exec_shutdown_flag = false;     /* shutting down C-EXEC */
bool c2_exec_shutdown_flag = false;     /* shutting down C2-EXEC */
bool error_occured_flag = false;    /* connection failed    */

/* Creation of the  first C-EXEC thread */
void *C_Exec(void *arg)
{
    while (true)
    {
        /* Unlock and get the first entry from the queue */
        pthread_mutex_lock(&mutex);
        struct queue_entry *ptr_local = queue_peek_front(&c_exec_queue); // check who's next
        pthread_mutex_unlock(&mutex);

        /* Check if the queue is empty */
        if (ptr_local != NULL)
        {
            /* actually get next entry */
            pthread_mutex_lock(&mutex);
            ptr1 = queue_pop_head(&c_exec_queue);
            pthread_mutex_unlock(&mutex);

            /* Get the TBC data for the current task to run */
            threaddesc *current_task1 = (threaddesc *)ptr1->data;

            /* Swapcontext and launch the first entry from the queue */
            swapcontext(&c_exec_context1, &current_task1->threadcontext);
        }

        /* If the queue is empty, check flags for shutdown */
        else if ((!open_flag && create_flag) || (close_flag && create_flag) || error_occured_flag)
        {
            c1_exec_shutdown_flag = true;
            break;
        }
        usleep(sleeping_time); /* sleep and loop */
    }
    pthread_exit(0); /* error */
}

/* Creation of the second C-EXEC thread */
void *C2_Exec(void *arg)
{
    while (true)
    {
        /* Unlock and get the first entry from the queue */
        pthread_mutex_lock(&mutex);
        struct queue_entry *ptr_local = queue_peek_front(&c_exec_queue); // check who's next
        pthread_mutex_unlock(&mutex);

        /* Check if the queue is empty */
        if (ptr_local != NULL)
        {
            /* actually get next entry */
            pthread_mutex_lock(&mutex);
            ptr2 = queue_pop_head(&c_exec_queue);
            pthread_mutex_unlock(&mutex);

            /* Get the TBC data for the current task to run */
            threaddesc *current_task2 = (threaddesc *)ptr2->data;

            /* Swapcontext and launch the first entry from the queue */
            swapcontext(&c_exec_context2, &current_task2->threadcontext);
        }

        /* If the queue is empty, check flags for shutdown */
        else if ((!open_flag && create_flag) || (close_flag && create_flag) || error_occured_flag)
        {
            c2_exec_shutdown_flag = true;
            break;
        }
        usleep(sleeping_time); /* sleep and loop */
    }
    pthread_exit(0); /* error */
}

/* Creation of the I-EXEC thread*/
void *I_Exec(void *arg)
{
    /* local variables */
    int fd_number, size_value;
    char *function_name, *file_name, *message;

    while (true)
    {
        /* Unlock and get the first entry from the queue */
        pthread_mutex_lock(&mutex);
        struct queue_entry *ptr_local = queue_peek_front(&i_exec_queue);
        pthread_mutex_unlock(&mutex);
        
        /* Check if the queue is empty */
        if (ptr_local != NULL)
        {
            /* actually get next entry */
            pthread_mutex_lock(&mutex);
            struct queue_entry *ptr_local_1 = queue_pop_head(&i_exec_queue);
            pthread_mutex_unlock(&mutex);

            function_name = ((iodesc *)ptr_local_1->data)->iofunction;  /* open || write || read || close   */
            fd_number = ((iodesc *)ptr_local_1->data)->fdnum;           /* file descriptor number           */
            file_name = ((iodesc *)ptr_local_1->data)->filname;         /* file name for open               */
            message = ((iodesc *)ptr_local_1->data)->buffer;            /* message for write and read       */
            size_value = ((iodesc *)ptr_local_1->data)->size;           /* size for write and read          */

            /* Get function to execute */
            if (strcmp(function_name, "open") == 0)         /* Open */
            {
                /* open fd and get int */
                if ((fd = open(file_name, O_RDWR)) < 0)
                {
                    error_occured_flag = true;
                    fprintf(stderr, "File could not be opened\n");
                }
                else
                {
                    /* Pop task from wait queue and add it to C-EXEC queue */
                    struct queue_entry *node = queue_pop_head(&wait_queue);
                    pthread_mutex_lock(&mutex);         /* lock to use queue */
                    queue_insert_tail(&c_exec_queue, node);
                    pthread_mutex_unlock(&mutex);       /* unlock to use queue */
                }
            }
            else if (strcmp(function_name, "write") == 0)   /* Write */
            {
                /* write to fd */
                if ((bytes = write(fd, message, size_value)) < 0)
                {
                    error_occured_flag = true;
                    fprintf(stderr, "Could not write to file\n");
                }
                else if (bytes == 0)
                {
                    error_occured_flag = true;
                    fprintf(stderr, "Reached end of file\n");
                }
                else 
                {
                    // fails with seg fault

                    /* Pop task from wait queue and add it to C-EXEC queue */
                    struct queue_entry *node = queue_pop_head(&wait_queue);
                    pthread_mutex_lock(&mutex);         /* lock to use queue */
                    queue_insert_tail(&c_exec_queue, node);
                    pthread_mutex_unlock(&mutex);       /* unlock to use queue */
                }
            }
            else if (strcmp(function_name, "read") == 0)    /* Read */
            {
                /* read from fd */
                if ((bytes = read(fd, message, size_value)) < 0)
                {
                    error_occured_flag = true;
                    fprintf(stderr, "Could not read from file\n");
                }
                else if (bytes == 0)
                {
                    error_occured_flag = true;
                    fprintf(stderr, "Reached end of file\n");
                }
                else{
                    // pretty sure this is failing with seg fault. I think it has to do with enqueueing to c-queue
                    
                    /* Pop task from wait queue and add it to C-EXEC queue */
                    struct queue_entry *node = queue_pop_head(&wait_queue);
                    pthread_mutex_lock(&mutex);         /* lock to use queue */
                    queue_insert_tail(&c_exec_queue, node);
                    pthread_mutex_unlock(&mutex);       /* unlock to use queue */
                }

            }
            else if (strcmp(function_name, "close") == 0)   /* Close */
            {
                if (close(fd) < 0 )
                {
                    error_occured_flag = true;
                    fprintf(stderr, "Closing file failed\n");
                }
                else 
                {
                    printf("Successfully closed file\n");
                }
            }
        }
        /* Check flags and exit if shutting down or failing  */
        else if (c_exec_number == 2){   /* two C-EXEC threads */
            if ((close_flag && c1_exec_shutdown_flag && c2_exec_shutdown_flag) || (!open_flag && c1_exec_shutdown_flag && c2_exec_shutdown_flag) || error_occured_flag)
            {
                break;
            }
        }
        else if (c_exec_number == 1)    /* one C-EXEC thread */
        {
            if ((close_flag && c1_exec_shutdown_flag) || (!open_flag && c1_exec_shutdown_flag) || error_occured_flag)
            {
                break;
            }
        }
        
        usleep(sleeping_time); /* sleep and loop */
    }
    pthread_exit(0);
}

/* Initialization of C-EXEC and I-EXEC, queues, and variables */
void sut_init()
{
    /* Initialize the threads of C-EXEC and I-EXEC */
    pthread_create(&C_EXEC, NULL, C_Exec, &mutex);          /* C-EXEC will contain first C-EXEC ID      */
    pthread_create(&I_EXEC, NULL, I_Exec, &mutex);
    if (c_exec_number == 2) {                               /* Initialize second C-EXEC                 */
        pthread_create(&C2_EXEC, NULL, C2_Exec, &mutex);    /* C2_EXEC will contain second C-EXEC ID    */
    }

    /* Initialize the queues */
    /* C-EXEC ready queue */
    c_exec_queue = queue_create();
    queue_init(&c_exec_queue);
    /* I-EXEC ready queue */
    i_exec_queue = queue_create();
    queue_init(&i_exec_queue);
    /* Wait queue */
    wait_queue = queue_create();
    queue_init(&wait_queue);

    /* Initialize variables */
    thread_number = 0;
}

/* Create a task and add it to C-EXEC queue */
bool sut_create(sut_task_f fn)
{
    create_flag = true;

    /* Check number of threads is within bounds (32 or 33) */
    if (thread_number >= (MAX_THREADS))
    {
        printf("FATAL: Maximum thread limit reached... creation failed! \n");
        return false; /* return false on errors */
    }

    /* Generation of the task/context */
    tdescptr = &(thread_array[thread_number]);
    getcontext(&(tdescptr->threadcontext));
    tdescptr->threadid = thread_number;
    tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
    tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    tdescptr->threadcontext.uc_link = 0;
    tdescptr->threadcontext.uc_stack.ss_flags = 0;
    tdescptr->threadfunc = fn;

    makecontext(&tdescptr->threadcontext, fn, 0);

    /* Insert into C-EXEC queue */
    struct queue_entry *node = queue_new_node(tdescptr);
    pthread_mutex_lock(&mutex);         /* lock to use queue */
    queue_insert_tail(&c_exec_queue, node);
    thread_number++; /* increase of the number of threads */
    pthread_mutex_unlock(&mutex);       /* unlock to use queue */

    return true;
}

/* Pause task, save state, and reschedule by adding it to the back of the queue */
void sut_yield()
{
    pthread_t test =  pthread_self(); /* get current thread ID */
<<<<<<< HEAD
=======
    
    /* ptr set from the C-EXEC method */
    // /* IO task yield */
    // threaddesc *current_io_task = (threaddesc *)ptr->data;

    // /* Add entry to end of queue */
    // pthread_mutex_lock(&mutex);         /* lock to use queue */
    // queue_insert_tail(&c_exec_queue, ptr);
    // pthread_mutex_unlock(&mutex);       /* unlock to use queue */

    // /* Swap context and execute */
    // swapcontext(&current_io_task->threadcontext, &c_exec_context1);
    

    /* CPU task yield */
    threaddesc *current_task = (threaddesc *)ptr->data;

    /* Add entry to end of queue */
    pthread_mutex_lock(&mutex);         /* lock to use queue */
    queue_insert_tail(&c_exec_queue, ptr);
    pthread_mutex_unlock(&mutex);       /* unlock to use queue */
>>>>>>> parent of 27853f1 (cleanup comments)

    if (test == C_EXEC) {         /* first thread     */
        /* CPU task yield */
        threaddesc *current_task1 = (threaddesc *)ptr1->data;

        /* Add entry to end of queue */
        pthread_mutex_lock(&mutex);         /* lock to use queue */
        queue_insert_tail(&c_exec_queue, ptr1);
        pthread_mutex_unlock(&mutex);       /* unlock to use queue */

        /* Swap context and execute */
        swapcontext(&current_task1->threadcontext, &c_exec_context1);
    }
    else if (test == C2_EXEC) {   /* second thread    */
        /* CPU task yield */
        threaddesc *current_task2 = (threaddesc *)ptr2->data;

        /* Add entry to end of queue */
        pthread_mutex_lock(&mutex);         /* lock to use queue */
        queue_insert_tail(&c_exec_queue, ptr2);
        pthread_mutex_unlock(&mutex);       /* unlock to use queue */

        /* Swap context and execute */
        swapcontext(&current_task2->threadcontext, &c_exec_context2);
    }
    else {
        printf("FATAL: C-EXEC Thread not found \n");
        return;
    }   
    
    
}

/* Kill current running task */
void sut_exit()
{
    pthread_t test =  pthread_self(); /* get current thread ID */

    if (test == C_EXEC) {         /* first thread     */
        /* Get the current task and free it */
        threaddesc *current_task1 = (threaddesc *)ptr1->data;
        free(current_task1->threadstack);
        /* Swap context and execute */
        swapcontext(&current_task1->threadcontext, &c_exec_context1);
    }
    else if (test == C2_EXEC) {   /* second thread    */
        /* Get the current task and free it */
        threaddesc *current_task2 = (threaddesc *)ptr2->data;
        free(current_task2->threadstack);
        /* Swap context and execute */
        swapcontext(&current_task2->threadcontext, &c_exec_context1);   
    }
    else {
        printf("FATAL: C-EXEC Thread not found \n");
        return;
    }
}

/* Open the file with specified name. Return negative int on fail, positive int on success */
int sut_open(char *fname)
{
    open_flag = true;
    pthread_t test =  pthread_self(); /* get current thread ID */

    iodescptr = (iodesc *)malloc(sizeof(iodesc)); /* generate I-EXEC object */
    iodescptr->filname = fname;
    iodescptr->iofunction = "open";

    struct queue_entry *node = queue_new_node(iodescptr);
    
    if (test == C_EXEC) {         /* first thread     */

        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr1);
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_io_task = (threaddesc *)ptr1->data;
        /* Swap context and execute */
        swapcontext(&current_io_task->threadcontext, &c_exec_context1);
    }
    else if (test == C2_EXEC) {   /* second thread    */
        
        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr2);
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_io_task = (threaddesc *)ptr2->data;
        /* Swap context and execute */
        swapcontext(&current_io_task->threadcontext, &c_exec_context2);   
    }
    else {
        printf("FATAL: C-EXEC Thread not found \n");
        return -1;
    }
}

/* Write the bytes in buf to the disk file that is already open. Ignore write errors */
void sut_write(int fd, char *buf, int size)
{
    /* Cannot write if no file opened yet */
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called first\n");
        return;
    }

    pthread_t test =  pthread_self(); /* get current thread ID */

    iodescptr = (iodesc *)malloc(sizeof(iodesc)); /* generate I-EXEC object */
    iodescptr->fdnum = fd;
    iodescptr->buffer = buf;
    iodescptr->size = size;
    iodescptr->iofunction = "write";

    struct queue_entry *node = queue_new_node(iodescptr);

    if (test == C_EXEC) {         /* first thread     */
        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr1); //added
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_task1 = (threaddesc *)ptr1->data;
        /* Swap context and execute */
        swapcontext(&current_task1->threadcontext, &c_exec_context1);
    }
    else if (test == C2_EXEC) {   /* second thread    */
        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr2); //added
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_task2 = (threaddesc *)ptr2->data;
        /* Swap context and execute */
        swapcontext(&current_task2->threadcontext, &c_exec_context2);  
    }
    else {
        printf("FATAL: C-EXEC Thread not found \n");
        return;
    }

    printf("Done writing! \n");
}

/* Close the file that is pointed to by the file descriptor */
void sut_close(int fd)
{
    /* Cannot close if no file opened yet */
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called first\n");
        return;
    }
    
    close_flag = true;

    iodescptr = (iodesc *)malloc(sizeof(iodesc)); /* generate I-EXEC object */
    iodescptr->fdnum = fd;
    iodescptr->iofunction = "close";

    struct queue_entry *node = queue_new_node(iodescptr);
    /* add to i_exec_queue */
    pthread_mutex_lock(&mutex);
    queue_insert_tail(&i_exec_queue, node);
    pthread_mutex_unlock(&mutex);

}

/* Read using a pre-allocated memory buffer. Return non-NULL on success, NULL on error */
char *sut_read(int fd, char *buf, int size)
{
    /* Cannot read if no file opened yet */
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called first\n\n");
        return NULL;
    }
    
    pthread_t test =  pthread_self(); /* get current thread ID */

    iodescptr = (iodesc *)malloc(sizeof(iodesc)); /* generate I-EXEC object */
    iodescptr->fdnum = fd;
    iodescptr->buffer = buf;
    iodescptr->size = size;
    iodescptr->iofunction = "read";

    struct queue_entry *node = queue_new_node(iodescptr);

    if (test == C_EXEC) {         /* first thread     */
        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr1);
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_task1 = (threaddesc *)ptr1->data;
        /* Swap context and execute */
        swapcontext(&current_task1->threadcontext, &c_exec_context1);
    }
    else if (test == C2_EXEC) {   /* second thread    */
        /* new task put into i_exec_queue and current task put into wait_queue */
        pthread_mutex_lock(&mutex);
        queue_insert_tail(&i_exec_queue, node);
        queue_insert_tail(&wait_queue, ptr2);
        pthread_mutex_unlock(&mutex);

        /* Go back to C_EXEC function */
        threaddesc *current_task2 = (threaddesc *)ptr2->data;
        /* Swap context and execute */
        swapcontext(&current_task2->threadcontext, &c_exec_context2);   
    }
    else {
        printf("FATAL: C-EXEC Thread not found \n");
        return NULL;
    }
    return "a"; /* generic return value not NULL */
}

/* Clean and shut down once the current tasks complete */
void sut_shutdown()
{
    /* Join C-EXEC and I-EXEC to clean properly */
    pthread_join(C_EXEC, NULL);
    pthread_join(I_EXEC, NULL);
    if (c_exec_number == 2 ){ 
        pthread_join(C2_EXEC, NULL);
    }
}
