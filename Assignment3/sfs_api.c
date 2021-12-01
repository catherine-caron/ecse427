#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "sfs_api.h"
#include "disk_emu.h"

// common values
int block_size = 1024;              // block size in bytes
char disk[4] = "disk";              // the disk filename
int number_of_blocks = 1024;        // number of blocks on the disk 

// cached structs
directoryEntry_t* startingPointer;  // points to the first entry in the root, used by sfs_getnextfilename
directoryEntry_t* walkingPointer;   // moved by sfs_getnextfilename to point to next file in root directory
rootDirectory_t rootDirectory;      // cached root directory
freeBlockMap_t freeBlockMap;        // initialize a freeBlockMap struct
superblock_t superblock;            // initialize a superblock struct
inode_t root;                       // root Directory inode

// root is only one block, so max number of files that can be open is 42
fileDescriptor_t fileDescriptorTable[42]; // fd table
int fdTableSize;

/**
 * Initializes the Simple File System
 * If fresh is true, create a new file system
 * If fresh is false, initialize an existing file system with disk called disk
 * 
 * @param fresh 
 */
void mksfs(int fresh) {

        // initialize disk
        // check fresh flag
        if (fresh == 0){
            // if flag is false, then it already exists, call init_disk
            if (init_disk(disk, block_size, number_of_blocks) < 0){
                printf("An error occured when initializing the disk\n");
                exit(-1); 
            }
        }
        else{
            // if flag is true, then it doesn't exist, call init_fresh_disk
            if (init_fresh_disk(disk, block_size, number_of_blocks) < 0){
                printf("An error occured when initializing the disk\n");
                exit(-1); 
            }
            // designate block 1023 as the free block map
            memset(freeBlockMap.freeBlockMap, 0, sizeof freeBlockMap.freeBlockMap); // fill the array with 1024 zeros (all free blocks)
            freeBlockMap.freeBlockMap[0] = 1;       // set superblock to used
            freeBlockMap.freeBlockMap[1] = 1;       // set root directory to used
            freeBlockMap.freeBlockMap[1023] = 1;    // set free block map to used

            // initialize the superblock and fill it with important info (block 0)
            superblock.magic = 2898067461; 
            superblock.block_size = block_size;
            superblock.inode_table_size = 0;    // set to zero because root inode is inside superblock
            superblock.sfs_size = 3; 

            // initialize the root directory inode
            memset(root.direct, -1, sizeof root.direct);  // mark all pointers as empty
            root.direct[0] = 1;     // root is stored at block 1 by convention
            root.file_size = 0;     // root is empty initially
            root.indirect = -1;
            root.link_count = 1;    // root calls root? sure, why not
            root.mode = 0;          // still don't know what this is or does lol
            rmemset(root.fill, -1, sizeof root.fill);   // rest of inode is empty
            
            superblock.root = root;
            memset(superblock.fill, -1, sizeof superblock.fill); // rest of block is empty

            // initialize the root directory block (block 1) to empty
            memset(&rootDirectory, -1, sizeof rootDirectory);

            // write all structs to the disk
            // write the free block map
            char blockMapInBytes[sizeof freeBlockMap];                      // array of bytes 
            memcpy(blockMapInBytes, &freeBlockMap, sizeof freeBlockMap);    // convert the struct into an array of bytes to write to disk
            if (write_blocks( 1023, 1, blockMapInBytes) < 0){
                printf("An error occured when writing the free block map to the disk\n");
                exit(-1); 
            }

            // write the super block
            char superblockInBytes[sizeof superblock];                           // array of bytes 
            memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
            if (write_blocks( 0, 1, superblockInBytes) < 0){
                printf("An error occured when writing the superblock to the disk\n");
                exit(-1); 
            }

            // write the root directory
            char rootDirectoryInBytes[sizeof rootDirectory];                           // array of bytes 
            memcpy(rootDirectoryInBytes, &rootDirectory, sizeof rootDirectoryInBytes);    // convert the struct into an array of bytes to write to disk
            if (write_blocks( 1, 1, superblockInBytes) < 0){
                printf("An error occured when writing the root directory to the disk\n");
                exit(-1); 
            }

        }  
        
        // initialize root cache (read from disk)
        char readBlock[block_size];                         // array of bytes to store read data    
        if(read_blocks(1, 1, read_blocks) < 0){             // read the root directory block
            printf("An error occured when reading the root directory from the disk\n");
            exit(-1); 
        }
        memcpy(&rootDirectory, readBlock, sizeof rootDirectory);     // fill the struct with data read from disk
        
        // initialize startingPointer and walkingPointer for sfs_getnextfilename
        startingPointer = &rootDirectory.entries[0]; // set the starting pointer to the top of the root directory
        walkingPointer = NULL;  // used as a check in sfs_getnextfilename 

        // initialize fd table to empty
        fdTableSize = 0; 
}

/**
 * @brief Opens the given file and returns the fd
 * If the file doesn't exist, it creates it
 * 
 * @param name file name
 * @return fd number
 */
int sfs_fopen(char *name){
// pseudo code:
    // read the superblock to get the root directory size
    char readBlock[block_size];                         // array of bytes to store read data    
    if(read_blocks(0, 1, read_blocks) < 0){             // read the superblock
        printf("An error occured when reading the superblock from the disk\n");
        exit(-1); 
    }
    memcpy(&superblock, readBlock, sizeof superblock);  // fill the struct with data read from disk

    // get the size of the root directory from the supernode root inode
    int rootSize = superblock.root.file_size; // in bytes

    // check cached root directory to see if file is listed there
    char existsFlag = 0; // if the file is found in the root directory, set this flag to true
    for (int i = 0; i < rootSize / 24; i++){    // since an entry is 24 bytes long, divide size by 24 to get number of entries
        if (strcmp(rootDirectory.entries[i].name, name) == 0){  
            existsFlag = 1; // the file exists
            break;
        }
    }

    // create the new file
    if (existsFlag == 0){
        // check that the filename is within constraints
        if (sizeof name > 20){
            printf("File name must be under 16 characters long\n"); // 16 characters + 4 characters for the extension
            return -1;
        }

        // check the number of free blocks in the superblock
        // read the free block map from memory
        char readBlock[block_size];                         // array of bytes to store read data    
        if(read_blocks(1023, 1, read_blocks) < 0){          // read the free block map block
            printf("An error occured when reading the free block map from the disk\n");
            exit(-1); 
        }
        memcpy(&freeBlockMap, readBlock, sizeof freeBlockMap); // fill the struct with data read from disk
        int freeBlockNumber = 1024; // out of bounds
        for (int i = 0; i < sizeof freeBlockMap; i++){
            if (freeBlockMap.freeBlockMap[i] == 0){
                freeBlockNumber = i; // save the block number (will be RW pointer)
                freeBlockMap.freeBlockMap[i] = 1; // mark it as used
                break;
            }
        }
        if (freeBlockNumber == 1024){
            printf("The disk is full. Delete files before creating a new one\n");
            return -1;
        }
        
        // initialize an inode
        inode_t newInode;
        // check free block map and choose first available block (mark as used)
        int freeInodeNumber = 1024; // out of bounds
        for (int i = 0; i < sizeof freeBlockMap; i++){
            if (freeBlockMap.freeBlockMap[i] == 0){
                freeInodeNumber = i; // save the block number
                freeBlockMap.freeBlockMap[i] = 1; // mark it as used
                break;
            }
        }
         if (freeInodeNumber == 1024){
            printf("The disk is full. Delete files before creating a new one\n");
            return -1;
        }
        
        // fill inode information
        newInode.inode_number = freeInodeNumber;                // assign block number as inode number
        newInode.file_size = 0;                                 // set file size = 0 for now (block will be empty)
        newInode.mode = 0;                                      // set mode/file permissions ??????????
        newInode.link_count = 1;                                // set link count to 1
        memset(newInode.direct, -1, sizeof newInode.direct);    // set other direct pointers to -1 (empty)
        newInode.direct[0] = freeBlockNumber;                   // set first direct pointer to free data block
        newInode.indirect = -1;                                 // set indirect pointer to -1 (empty) 
        memset(newInode.fill, -1, sizeof newInode.fill);        // fill rest of block with empty values

        // write inode to disk
        char inodeInBytes[sizeof newInode];                         // array of bytes 
        memcpy(inodeInBytes, &newInode, sizeof inodeInBytes);       // convert the struct into an array of bytes to write to disk
        if (write_blocks(freeInodeNumber, 1, inodeInBytes) < 0){    // write to free inode block
            printf("An error occured when writing the inode to the disk\n");
            exit(-1); 
        }

        // initialize the data block
        block_t dataBlock;
        memset(dataBlock.bytes, -1, sizeof dataBlock.bytes); // fill block with empty values

        // write data block to disk
        char dataBlockInBytes[sizeof dataBlock];                           // array of bytes 
        memcpy(dataBlockInBytes, &dataBlock, sizeof dataBlockInBytes);     // convert the struct into an array of bytes to write to disk
        if (write_blocks(freeBlockNumber, 1, dataBlockInBytes) < 0){       // write to free data block
            printf("An error occured when writing the data block to the disk\n");
            exit(-1); 
        }
        
        // update superblock
        superblock.sfs_size = superblock.sfs_size + 2;                  // two new blocks added
        superblock.inode_table_size = superblock.inode_table_size + 1;  // one new inode added

        // write superblock to disk
        char superblockInBytes[sizeof superblock];                           // array of bytes 
        memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(0, 1, superblockInBytes) < 0){
            printf("An error occured when writing the superblock to the disk\n");
            exit(-1); 
        }

        // add a new entry to the root directory in cache
        directoryEntry_t newEntry;
        newEntry.inode_index = newInode.inode_number;       // add the inode number
        strcpy(newEntry.name, name);                        // add the file name
        rootDirectory.entries[rootSize / 24] = newEntry;    // 24 bytes per entry

        // write root directory to disk
        char rootDirectoryInBytes[sizeof rootDirectory];                              // array of bytes 
        memcpy(rootDirectoryInBytes, &rootDirectory, sizeof rootDirectoryInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(1, 1, rootDirectoryInBytes) < 0){                            // root directory is always block 1
            printf("An error occured when writing the root directory to the disk\n");
            exit(-1); 
        }

        // add the file to the fd table
        fileDescriptor_t newfdEntry;
        strcpy(newfdEntry.filename, name);              // add the file name
        newfdEntry.inode = freeInodeNumber;             // add the inode number
        newfdEntry.RWBlockPointer = freeBlockNumber;    // add the data block number
        newfdEntry.RWBytePointer = 0;                   // set byte pointer to the top of the block
        newfdEntry.fd = fdTableSize;                    // add fd number (table index)
        fileDescriptorTable[fdTableSize] = newfdEntry;  // add entry to fd table
        fdTableSize++;                                  // increase fd table size

        // return the fd number if everything is successful
        return newfdEntry.fd;  // could be 0 if its the first entry
    }
    
    // open the existing file
    else if (existsFlag == 1){
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
    // something went wrong, you should never get here
    else {
        printf("How did you get here? this code is unreachable!\n");
        exit(-1);
    }  
}

/**
 * @brief Gets the filename of the next file in the root directory and writes it into fname
 * Useful for iterating through the root directory
 * Returns 0 once you have walked the entire directory and looped back
 * 
 * @param fname file name
 * @return int 1 for success, 0 when entire root directory has been walked
 */
int sfs_getnextfilename(char *fname) {
// pseudo code:
    // check if walkingPointer is initialized yet
        // if not, then this is the first time calling the function
            // set walkingPointer equal to the startingPointer
            // get the file name at the walkingPointer
            // save it in fname
            // increase the walkingPointer by 1
            // return 1 for success

        // if yes, then we've started walking
            // compare walkingPointer and startingPointer to check they are not equal
                // if they are equal, 
                    // you've completed the walk through the root directory
                    // return 0
            // get the file name at the walkingPointer
            // save it in fname
            // increase the walkingPointer by 1
            // return 1 for success

}
/**
 * @brief Given the file name, return its size
 * 
 * @param path filename
 * @return fileSize file size
 */
int sfs_getfilesize(const char* path) {
// pseudo code:
    // Since there are no subdirectories, the path is always the file name
    // search in the cached root directory for the file name
    // get the inode number
    // read the inode from the disk
    // get the file size inside the inode
    // return the file size
} 

/**
 * @brief Writes the contents of buf into the file at the current RWPointer location
 * This may be the end of the file, or it may be inside the file, in which case it will overwrite the file
 * Return value of -1 indicates an error occured
 * 
 * @param fileID fd number of opened file
 * @param buf  data to write to the file
 * @param length length of data in bytes of what is to be written
 * @return length the number of bytes written. -1 means an error occured
 */
int sfs_fwrite(int fileID, char *buf, int length) {
// pseudo code:
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
                    // we'll need to calculate if the file size grows after to save in the inode 
                    // calculate by how much
                        // do length % 1024 to get overflow of writing bytes
                        // number of overwriting bytes = number of overflow bytes in last block - number of overflow new writing bytes
                        // save this number to use in the inode file size later 
    // if we're not overwriting, then we're at the end of the file 
    // either way, use the RWBlockPointer and RWBytePointer to do the next calculations
        // check if we're going to need another block -----------------> can be expanded to more than 1 more block
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
                        // write to the superblock
                            // increase sfs_size by 1 block
                // write the (RWBytePointer - 1024) bytes into the original block
                // write the (RWBytePointer - (RWBytePointer - 1024)) into the next block
                // if not overwriting, 
                    // set inode file size = file size + length
                // if overwriting, 
                    // set inode file size = file size + (length % 1024)
                // set RWBlockPointer to the last block written to
                // set the RWBytePointer to the last byte written to
                // return length if everything worked!

} 

/**
 * @brief Reads the contents of file into buf at the current RWPointer location
 * This may be the end of the file (which would return nothing), or it may be inside the file
 * Therefore, you should call sfs_seek first
 * Return value of -1 indicates an error occured
 * 
 * @param fileID fd number
 * @param buf location to store read data
 * @param length length of bytes to read
 * @return length number of bytes read
 */
int sfs_fread(int fileID, char *buf, int length) {
// pseudo code:
    // I'm going to assume the fileID is the fd number in the fd table, buf is the buffer to save what is read, and length is the number of bytes to read

    // search for fd number in fd table and get inode number, RWBlockPointer, and RWBytePointer
        // if not found, return error (file not opened)
    // calculate if the read fits in one block
        // if (RWBytePointer + length) <= 1024 then only need one block
            // read the block from the disk
            // read from RWBytePointer to 1024 (end of block)
            // append to end of buf
            // return length

        // if not, then we need to get the next block(s)
            // read inode from disk and save it into something we can work with
            // calculate if we're reading past the file size
                // if ((RWBlockPointer x 1024) + RWBytePointer + length) > file size stored in inode
                    // set length = file size - ((RWBlockPointer X 1024) + RWBytePointer) 
                    // this is the max length can be, and we'll return this value at the end to indicate that that's the max bytes we read
            // create a temporary array of block numbers to read from
            // first block number is RWBlockNumber (array[0])
            // number of addiitonal blocks we need to read from: (length + RWBytePointer) / 1024
            // for every additional block we need to read from:
                // loop to find RWBlockPointer in the direct list
                    // if you find it, return the next block number in the next pointer
                        // if its the last pointer
                            // read the block in the indirect pointer
                            // return the block number in the first entry of the array
                    // if you dont find it
                        // read the block in the indirect pointer
                        // loop through the array to find the RWBlockPointer and return the next block number
                            // if its the last entry, return error (no more space in the inode)
                // add the block number to the array of block numbers we will need to read from
                // make RWBlockPointer equal the block number just obtained
                // restart the for loop until all block numbers are in the array
                // calculate the number of bytes to read in the last block
                    // last bytes to read = (RWBytePointer + length) % 1024
                // for every block in the array of block numbers:
                    // if its the last block   
                        // read the block from the disk
                        // read from RWBytePointer to last bytes to read
                        // append to end of buf
                    // else
                        // read the block from the disk
                        // read from RWBytePointer to 1024 (end of block)
                        // append to end of buf
                        // set RWBytePointer = 0 (start of block)
                // once done, return length
}

/**
 * @brief Move the RW pointer to the designated location in the file
 * Return 0 if successful, -1 on error
 * 
 * @param fileID fd number
 * @param loc location in bytes
 * @return int 0 on success, -1 on error
 */
int sfs_fseek(int fileID, int loc) {
// pseudo code
    // I'm assuming loc is in bytes

    // use fd table to get the inode number
    // read inode from disk
    // check if loc is within the file size
        // if loc > file size from inode
            // return -1 (error loc not within file)
    // calculate which block the loc is in
        // pointer number = loc / 1024
        // if pointer number <= 12
            // go to the direct pointer that's equal to the pointer numebr you just found
            // get the block number
        // else if (12 < pointer number <= 12 + 1024)
            // get the indirect pointer block number
            // read the block from the disk
            // array index = (pointer number - 12)
            // go to array index and get the block number
        // else if pointer number > 12 + 1024
            // location is not within inode 
            // return -1 error 
        // else set RWBlockPointer = block number
    // calculate the byte its at in the block
        // RWBytePointer = loc % 1024
    // if everything worked, return 0
} 

/**
 * @brief Close the opened file with given fd number
 * Return 0 on success, -1 on error
 * 
 * @param fileID fd number
 * @return int 0 on success, -1 on error
 */
int sfs_fclose(int fileID) {
// pseudo code
    // find the fd number in the fd table
        // if not found, return -1 (error, file wasn't open)
    // remove it from the table
    // return 0
} 

/**
 * @brief Deletes the file from the file system
 * Returns 0 on success, -1 on error
 * 
 * @param file file name
 * @return int 0 on success, -1 on error
 */
int sfs_remove(char *file) {
// pseudo code
    // use file name to get inode number from cached root directory
    // remember what entry it is to delete it later
    // calculate where the file ends
        // last pointer = file size from inode / 1024
        // last byte = file size from inode % 1024
    // if last pointer <= 12 
        // file only occupies direct pointers
        // for every direct pointer up to the last pointer:
            // get the block number in the pointer
            // write 0 or empty to the block on the disk (deletes file information)
            // mark that block as free in the free block map
            // set block number in direct pointer to -1
    // if last pointer > 12 
        // file occupies all direct pointers and indirect pointer
            // for every direct pointer:
                // get the block number in the pointer
                // write 0 or empty to the block on the disk (deletes file information)
                // mark that block as free in the free block map
                // set block number in direct pointer to -1
            // get the block number in the indirect pointer
            // read the block containing the indirect pointer array
            // for every pointer from 13 to the last pointer:
                // get the block number in the pointer
                // write 0 or empty to the block on the disk (deletes file information)
                // mark that block as free in the free block map
                // set block number in pointer to -1
    // Now the data has been deleted, delete the inode
        // set link count to 0 (not used) -> this is done as a precaution, but probably isn't necessary if everything is done properly
        // mark the block as free in the free block map
    // remove the file from the root directory on the disk
    // remove the file from the root directory in cache
    // write to the superblock
        // sfs_size = sfs_size - (file_size / 1024)
        // decrease inode_table_size by 1
    // find the inode number in the fd table
        // if not found, return 0 (you're done!)
        // if found, remove it from the table and return 0 
}

