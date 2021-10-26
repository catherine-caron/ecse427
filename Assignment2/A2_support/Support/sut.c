#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "sut.h"
#include "queue.h"

/* 
*   The Simple User-Level Thread Library (SUT)
*   By Catherine Caron (ID 260762540)
*/

int c_exec_number = 1;                  /* manually set this value to 2 to use two C-EXEC threads (for part 2)          */

pthread_t C_EXEC, I_EXEC, C2_EXEC;      /* kernel threads                                                               */
ucontext_t c_exec_context;              /* userlevel context for context switching                                      */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;      /* semaphore lock for enqueing and dequeing                     */
struct queue c_exec_queue, i_exec_queue, wait_queue;    /* ready queue for each kernel thread and wait queue for I-EXEC */
struct queue_entry *ptr;                /* queue node/entry                                                             */
int sleeping_time = 1000;               /* wait time before checking again when queue is empty                          */

/* C-EXEC global variables */
int thread_number;                      /* counter                          */
threaddesc thread_array[MAX_THREADS], *tdescptr; /* ????? */

/* I-EXEC global variables */
iodesc *iodescptr;                      /* I-EXEC object                    */
int fd;                                 /* file descriptor number           */
int bytes;                              /* number of bytes written/read     */
int sockfd;  // this may be provided by user now. 
char received_from_server[SIZE]; 

/* Shutdown flags */
bool create_flag = false;               /* creation of task     */
bool open_flag = false;                 /* opening file         */
bool close_flag = false;                /* closing file         */
bool c_exec_shutdown_flag = false;      /* shutting down C-EXEC */
bool connection_failed_flag = false;    /* connection failed    */

/* Creation of the C-EXEC thread */
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
            ptr = queue_pop_head(&c_exec_queue);
            pthread_mutex_unlock(&mutex);

            /* Get the TBC data for the current task to run */
            threaddesc *current_task = (threaddesc *)ptr->data;

            /* Swapcontext and launch the first entry from the queue */
            swapcontext(&c_exec_context, &current_task->threadcontext);
        }

        /* If the queue is empty, check flags for shutdown */
        else if ((!open_flag && create_flag) || (close_flag && create_flag) || connection_failed_flag)
        {
            c_exec_shutdown_flag = true;
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
                    connection_failed_flag = true;
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
                    connection_failed_flag = true;
                    fprintf(stderr, "Could not write to file\n");
                }
                else if (bytes == 0)
                {
                    connection_failed_flag = true;
                    fprintf(stderr, "Reached end of file\n");
                }
                else 
                {
                    // This was not done, I added this. not sure if its good! ????????????????????

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
                    connection_failed_flag = true;
                    fprintf(stderr, "Could not read from file\n");
                }
                else if (bytes == 0)
                {
                    connection_failed_flag = true;
                    fprintf(stderr, "Reached end of file\n");
                }
                else 
                {
                    // This was not done, I added this. not sure if its good! ????????????????????

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
                    connection_failed_flag = true;
                    fprintf(stderr, "Closing file failed\n");
                }
            }
        }
        /* Check flags and exit if shutting down or failing  */
        else if ((close_flag && c_exec_shutdown_flag) || (!open_flag && c_exec_shutdown_flag) || connection_failed_flag)
        {
            break;
        }
        usleep(sleeping_time); /* sleep and loop */
    }
    pthread_exit(0);
}

/* Initialization of C-EXEC and I-EXEC, queues, and variables */
void sut_init()
{
    /* Initialize the threads of C-EXEC and I-EXEC */
    pthread_create(&C_EXEC, NULL, C_Exec, &mutex);
    pthread_create(&I_EXEC, NULL, I_Exec, &mutex);
    if (c_exec_number == 2) {                               /* Initialize second C-EXEC */
        pthread_create(&C2_EXEC, NULL, C_Exec, &mutex);
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

    /* Initialize important variables */
    thread_number = 0;
}

/* Create a task and add it to C-EXEC queue */
bool sut_create(sut_task_f fn)
{
    /* raise create flag for shutdown later */
    create_flag = true;

    /* Check number of threads is within bounds (32 or 33) */
    if (thread_number >= (MAX_THREADS)
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
    /* ptr set from the C-EXEC method */
    /* Get the current task and swap context */
    threaddesc *current_task = (threaddesc *)ptr->data;

    /* Add entry to end of queue */
    pthread_mutex_lock(&mutex);         /* lock to use queue */
    queue_insert_tail(&c_exec_queue, ptr);
    pthread_mutex_unlock(&mutex);       /* unlock to use queue */

    /* Swap context and execute */
    swapcontext(&current_task->threadcontext, &c_exec_context);
}

/* Kill current running task */
void sut_exit()
{
    /* Get the current task and free it */
    threaddesc *current_task = (threaddesc *)ptr->data;
    free(current_task->threadstack);

    /* Swap context for next task and execute */
    swapcontext(&current_task->threadcontext, &c_exec_context);
}

// NEEDS TO BE REWRITTEN TO FIT OUR API
// =========================================
/* Open the file with specified name. Return negative int on fail, positive int on success */
int sut_open(char *fname)
{
    /* raise open flag for shutdown later */
    open_flag = true;

    // Generation of the structure
    fd = open(fname, O_RDWR); // open, return -1 if fails

    iodescptr = (iodesc *)malloc(sizeof(iodesc));

    iodescptr->fdnum = port;
    iodescptr->filname = dest;
    iodescptr->iofunction = "open";

    // Enqueue the structure
    struct queue_entry *node = queue_new_node(iodescptr);
    // The structure is added to the i_exec_queue and consequently the current_task is put in the waiting queue
    pthread_mutex_lock(&mutex);
    queue_insert_tail(&i_exec_queue, node);
    queue_insert_tail(&wait_queue, ptr);
    pthread_mutex_unlock(&mutex);

    // Go to the c_exec_context i.e. C_EXEC function in order to avoid blocking it
    threaddesc *current_io_task = (threaddesc *)ptr->data;
    swapcontext(&current_io_task->threadcontext, &c_exec_context);
}

// NEEDS TO BE REWRITTEN TO FIT OUR API
// =========================================
/* This function will write size bytes from buf to the socket associated with the current task */
void sut_write(char *buf, int size)
{
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called before sut_write()\n\n");
        return;
    }

    // Generation of the structure
    iodescptr = (iodesc *)malloc(sizeof(iodesc));

    iodescptr->buffer = buf;
    iodescptr->size = size;
    iodescptr->iofunction = "write";

    // Enqueue the structure
    struct queue_entry *node = queue_new_node(iodescptr);
    // The structure is added to the i_exec_queue
    pthread_mutex_lock(&mutex);
    queue_insert_tail(&i_exec_queue, node);
    pthread_mutex_unlock(&mutex);
}

// NEEDS TO BE REWRITTEN TO FIT OUR API
// =========================================
/* This function will close the socket associated with the current task */
void sut_close()
{
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called before sut_close()\n\n");
        return;
    }
    
    // Neccessary in order to shutdown the threads later on
    close_flag = true;

    // Generation of the structure
    iodescptr = (iodesc *)malloc(sizeof(iodesc));

    iodescptr->iofunction = "close";

    // Enqueue the structure
    struct queue_entry *node = queue_new_node(iodescptr);
    // The structure is added to the i_exec_queue
    pthread_mutex_lock(&mutex);
    queue_insert_tail(&i_exec_queue, node);
    pthread_mutex_unlock(&mutex);

    // Go to the c_exec_context i.e. C_EXEC function in order to avoid blocking it
    threaddesc *current_io_task = (threaddesc *)ptr->data;
    swapcontext(&current_io_task->threadcontext, &c_exec_context);
}

// NEEDS TO BE REWRITTEN TO FIT OUR API
// =========================================
/* This function will read from the task's associated socket until there is no more data */
char *sut_read()
{
    if (!open_flag)
    {
        printf("ERROR: sut_open() must be called before sut_read()\n\n");
        // received_from_server should be empty
        return received_from_server;
    }

    // Generation of the task/context
    iodescptr = (iodesc *)malloc(sizeof(iodesc));

    iodescptr->iofunction = "read";

    // Insert the struct in the C_exec queue.
    struct queue_entry *node = queue_new_node(iodescptr);
    pthread_mutex_lock(&mutex);
    queue_insert_tail(&i_exec_queue, node);
    queue_insert_tail(&wait_queue, ptr);
    pthread_mutex_unlock(&mutex);

    // Go to the c_exec_context i.e. C_EXEC function in order to avoid blocking it
    threaddesc *current_task = (threaddesc *)ptr->data;
    swapcontext(&current_task->threadcontext, &c_exec_context);

    return received_from_server;
}

/* Clean and shut down once the current tasks complete */
void sut_shutdown()
{
    /* Join C-EXEC and I-EXEC to clean properly */
    pthread_join(C_EXEC, NULL);
    pthread_join(I_EXEC, NULL);
}
