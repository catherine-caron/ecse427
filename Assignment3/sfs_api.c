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
    // initilaize everything necessary to run this thang!
    
}

/**
 * @brief Opens the given file and returns the fd
 * If the file doesn't exist, it creates it
 * 
 * @param name file name
 * @return fd 
 */
int sfs_fopen(char *name){
// Sudo code:
    // check cached root directory to see if file is listed there
        // if not, then create it 
            // check the number of free blocks in the superblock
                // if it's 1024, throw an error (no free space)
            // initialize an inode
                // check free block map and choose first available block (mark as used)
                    // if there's none, return an error (no free space)
                // assign block number as inode number
                // set file size = 0 for now
                // set mode/file permissions ??????????
                // set link count to 0 for now
                // set 1 direct pointer for now
                    // select a free block from the free block map (mark as used)
                        // if there's none, return error (no free space)
                    // set the 1st direct pointer to equal the block number
                    // possibly initialize the block to all 0s or -1 or something ? (need this, but what to fill?)
                    // save the block number temporarily (will be RW pointer)
                // set other direct pointers to -1 for now
                // set indirect pointer to -1 for now   
            // add the inode number to the root directory
                // update it on the disk
                // update it in cache
                // change the link count to 1 in the inode
            // add the inode number to the fd table
                // set the RW pointer to the temporarily saved block number and 0 byte number
            // return the fd number

        // if yes its in the root directory, then check if its inode number is in the fd table
            // if yes, return the fd number
            // if not
                // get the inode number from the root directory (should have gotten it just before actually)
                // read the inode from the disk into something we can work with
                    // check if link count is not 0 (unused inode error)
                // find where the file ends
                    // use file size
                    // do file size % 1024 to get overflow bytes 
                        // if == 0, then 
                            // file size / 1024 = number of blocks it occupies (int division truncates)
                        // else
                            // (file size / 1024 ) + 1 = number of blocks it occupies
                    // if number of blocks <= 12 then it fits in the direct pointers
                        // direct pointer number = number of blocks
                        // set RW pointer to block number in direct pointer and the overflow bytes 
                            // RWBlockPointer = block number/address
                            // if overflow == 0 
                                // RWBytePointer = 0
                            // else 
                                // RWBytePointer = overflow bytes - 1 (since indexing starts at 0)
                        // create a new entry in the fd table
                        // set inode number, RWBlockPointer, and RWBytePointer 
                        // return fd number
                    // else check the indirect pointer
                        // get the indirect pointer and read that block from the disk
                        // loop through the array (size 1024) until you find -1 pointer
                            // if not found, then take the last entry
                            // if found, then take the entry just before
                        // get the block number in the entry
                        // set RW pointer to block number in the entry and the overflow bytes 
                            // RWBlockPointer = block number/address
                            // if overflow == 0 
                                // RWBytePointer = 0
                            // else 
                                // RWBytePointer = overflow bytes - 1 (since indexing starts at 0)
                        // create a new entry in the fd table
                        // set inode number, RWBlockPointer, and RWBytePointer 
                        // return fd number
}


int sfs_getnextfilename(char *fname) {
    // get the name of the next file in directory 
    // set it in fname
    // needs to remember last location in directory
    // otherwise start from top?

    //return 0 if no new file
    // from memory  -> shouldnt it be from the cache?
}

int sfs_getfilesize(const char* path) {
    // get the size of the given file
    // from memory probs

// Sudo code:
    // I'm assuming the path is the f
} 


int sfs_fwrite(int fileID, char *buf, int length) {
    // write buf characters into disk
    // inc RW pointer
    // possibly increase file size in inode
    // return number of bytes written
    // from mem
/*
1. Allocate disk blocks (mark them as allocated in your free block list).
2. Modify the file's i-Node to point to these blocks.
3. Write the data the user gives to these blocks.
4. Flush all modifications to disk.
5. Note that all writes to disk are at block sizes. If you are writing few bytes into a file, this might actually end up writing a block to next. So if you are writing to an existing file, it is important you read the last block and set the write pointer to the end of file. The bytes you want to write goes to the end of the previous bytes that are already part of the file. After you have written the bytes, you flush the block to the disk.
*/

// Sudo code:
    // I'm going to assume the fileID is the fd number in the fd table, buf is the buffer of what to write, and length is the number of bytes to write

    // search for fd number in fd table and get inode number, RWBlockPointer, and RWBytePointer
        // if not found, return error (file not opened)
    // read inode from disk and save it into something we can work with
    // check if RW points to end of file or not
        // find where the file ends
        // use file size
        // do file size % 1024 to get overflow bytes 
            // if == 0, then 
                // file size / 1024 = number of blocks it occupies (int division truncates)
            // else
                // (file size / 1024 ) + 1 = number of blocks it occupies
        // check if number of blocks == RWBlockPointer
            // if no, then flag that we're overwriting the file
            // if yes, then check if overflow bytes < RWBytePointer
                // if yes, then flag that we're overwriting 
                    // we'll need to calculate if the file size grows after to save in the inode  !!!!!!!!!!!!
                    // calculate by how much
                        // do length % 1024 to get overflow of writing bytes
                        // number of overwriting bytes = number of overflow bytes in last block - number of overflow new writing bytes
                        // save this number to use in the inode file size later !!!!!!!!!
    // if we're not overwriting, then we're at the end of the file 
    // either way, use the RWBlockPointer and RWBytePointer to do the next calculations
        // check if we're going to need another block 
            // if 1024 - RWBytePointer >= 0 then we're good
            // if not, then we need another block
                // if overwriting, then get the next block from the inode pointers
                    // loop to find RWBlockPointer in the direct list
                        // if you find it, return the next block number in the next pointer
                            // if its the last pointer
                                // read the block in the indirect pointer
                                // return the block number in the first entry of the array
                        // if you dont find it
                            // read the block in the indirect pointer
                            // loop through the array to find the RWBlockPointer and return the next block number
                                // if its the last entry, return error (no more space in the inode)
                    // if not overwriting, then allocate a new block
                        // check free block map and choose first available block (mark as used)
                            // if there's none, return an error (no free space)
                        // loop to find RWBlockPointer in the direct list
                            // if you find it, set the pointer to the new block number
                            // if you dont find it
                                // read the block in the indirect pointer
                                // loop through the array to find the RWBlockPointer and set the pointer to the new block number
                                    // if its the last entry, return error (no more space in the inode)
                // write the (RWBytePointer - 1024) bytes into the original block
                // write the (RWBytePointer - (RWBytePointer - 1024)) into the next block
                // if not overwriting, 
                    // set inode file size = file size + length
                // if overwriting, 
                    // set inode file size = file size + (length % 1024)
                // set RWBlockPointer to the last block written to
                // set the RWBytePointer to the last byte written to
                // return length ???????

} 

int sfs_fread(int fileID, char *buf, int length) {
    // read characters from disk into buf
    // inc RW pointer
    // from mem

// use inspo from write


}

int sfs_fseek(int fileID, int loc) {
    // seek to the location from beginning
    // move RW pointer
    // from mem
    // 1. Modify the read and write pointers in memory. There is nothing to be done on disk!
} 

int sfs_fclose(int fileID) {
    // closes the given file
    // remove from fd table
} 


int sfs_remove(char *file) {
    // removes a file from the filesystem
    // like delete
    // delete all the data blocks (set to -1, 0, null, something)
    // delete inode
    // free all the blocks int the free block map
    // remove inode and file name from root directory (disk and cache)
    // remove from fd table if its on there
}

