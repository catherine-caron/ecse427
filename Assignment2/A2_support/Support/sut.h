#ifndef __SUT_H__
#define __SUT_H__
#include <stdbool.h>
#include <ucontext.h>

#define MAX_THREADS 33 // 30 tasks + 3 kernel threads (2 x C-EXEC and 1 x I-EXEC)
#define THREAD_STACK_SIZE 1024*64
#define SIZE 200

typedef struct __threaddesc
{
	int threadid;
	char *threadstack;
	void *threadfunc;
	ucontext_t threadcontext;
} threaddesc;

extern threaddesc thread_array[MAX_THREADS];

typedef struct __iodesc
{
	char *iofunction;		// "open" || "write" || "read" || "close"

	int ioid;
	char *iostack;

	char *fname;
	int fdnum;

	char *buffer;
	int size;
} iodesc;

/* API functions supported by SUT library */
typedef void (*sut_task_f)();

void sut_init();
bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
int sut_open(char *dest); // different, has int port
void sut_write(int fd, char *buf, int size); // different, has char 8dest, int port
void sut_close(int fd); // different, doesn't have fd
char *sut_read(int fd, char *buf, int size); //different, has no args
void sut_shutdown();

#endif
