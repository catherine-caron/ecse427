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
/**
* @brief 
* 1. Allocate and initialize an i-Node. You need to somehow remember the state of the i-Node table to know which i-Node could be allocated for the newly created file. Simply remembering the last i- Node used is not correct because as you delete files, some i-Nodes in the middle of the table will become unused and available for reuse.
* 2. Write the mapping between the i-Node and file name in the root directory. You could simply update the memory and disk copies.
* 3. No disk data block allocated. File size is set to 0.
* 4. This can also “open” the file for transactions (read and write). Note that the SFS API does not have a separate create() call. So you can do this activity as part of the open() call
 */
    
}


int sfs_fopen(char *name){
     // opens the given file
    // return index
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


int sfs_fwrite(int fileID, char *buf, int length) {
    // write buf characters into disk
    // inc RW pointer
    // from mem
/*
1. Allocate disk blocks (mark them as allocated in your free block list).
2. Modify the file's i-Node to point to these blocks.
3. Write the data the user gives to these blocks.
4. Flush all modifications to disk.
5. Note that all writes to disk are at block sizes. If you are writing few bytes into a file, this might actually end up writing a block to next. So if you are writing to an existing file, it is important you read the last block and set the write pointer to the end of file. The bytes you want to write goes to the end of the previous bytes that are already part of the file. After you have written the bytes, you flush the block to the disk.
*/
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
    // 1. Modify the read and write pointers in memory. There is nothing to be done on disk!
} 

int sfs_fclose(int fileID) {
    // closes the given file
} 


int sfs_remove(char *file) {
    // removes a file from the filesystem
    // like delete
}

