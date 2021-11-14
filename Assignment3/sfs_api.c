#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "sfs_api.h"
#include "disk_emu.h"




// to implement:
void mksfs(int); // creates the file system

int sfs_getnextfilename(char*); // get the name of the next file in directory 

int sfs_getfilesize(const char*); // get the size of the given file

int sfs_fopen(char*); // opens the given file

int sfs_fclose(int); // closes the given file

int sfs_fwrite(int, const char*, int); // write buf characters into disk

int sfs_fread(int, char*, int); // read characters from disk into buf

int sfs_fseek(int, int); // seek to the location from beginning

int sfs_remove(char*); // removes a file from the filesystem