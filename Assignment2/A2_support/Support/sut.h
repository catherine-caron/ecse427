#ifndef __SUT_H__
#define __SUT_H__
#include <stdbool.h>

/* 
*   The Simple User-Level Thread Library (SUT)
*   By Catherine Caron (ID 260762540)
*/

typedef void (*sut_task_f)();

void sut_init();
bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
int sut_open(char *dest);
void sut_write(int fd, char *buf, int size);
void sut_close(int fd);
char *sut_read(int fd, char *buf, int size);
void sut_shutdown();

// define each API function listed above. See instructions for details on what they should do

/*  sut_init initializes the SUT library
*   It must be called first in order to use the SUT library
*   It will initialize two kernel level threads: the C-EXEC and I-EXEC threads
*   
*/
void sut_init() {
    
}



#endif
