#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "sfs_api.h"
#include "disk_emu.h"

void mksfs(int fresh) {
    // creates the file system
    // set i-node for file
    // R/W pointer to end of file
    // use on disk to set up memory

}

int sfs_getnextfilename(char *fname) {
    // get the name of the next file in directory 
    // from memory  
}

int sfs_getfilesize(const char* path) {
    // get the size of the given file
    // calc with inode maybe? use pointer?
    // from memory probs
} 

int sfs_fopen(char *name){
     // opens the given file
    // return index
    // use on disk to set up memory
}

int sfs_fclose(int fileID) {
    // closes the given file
} 


int sfs_fwrite(int fileID, char *buf, int length) {
    // write buf characters into disk
    // inc RW pointer
    // from mem
} 

int sfs_fread(int fileID, char *buf, int length) {
    // read characters from disk into buf
    // inc RW pointer
    // from mem
}

int sfs_fseek(int fileID, int loc) {
    // seek to the location from beginning
    // move RW pointer
    // from mem
} 

int sfs_remove(char *file) {
    // removes a file from the filesystem
    // like delete
}

