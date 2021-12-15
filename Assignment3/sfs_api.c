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
rootInode_t root;                   // root Directory inode

// root is only one block, so max number of files that can be open is 42
fileDescriptor_t fileDescriptorTable[42]; // fd table
int fdTableSize; // actual size

/**
 * Initializes the Simple File System
 * If fresh is true, create a new file system
 * If fresh is false, initialize an existing file system with disk called disk
 * 
 * @param fresh 
 */
void mksfs(int fresh) {
// pseudo code written in comments

    // initialize the disk
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
        root.indirect = -1;     // no indirect pointers for now
        root.link_count = 1;    // root calls root? sure, why not!
        root.mode = 0;          // unused for now
        
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
 * Returns fd number on success, -1 on error
 * 
 * @param name file name
 * @return fd number
 */
int sfs_fopen(char *name){
// pseudo code written in comments

    // read the superblock to get the root directory size
    char readBlock[block_size];                         // array of bytes to store read data    
    if(read_blocks(0, 1, read_blocks) < 0){             // read the superblock
        printf("An error occured when reading the superblock from the disk\n");
        return -1; 
    }
    memcpy(&superblock, readBlock, sizeof superblock);  // fill the struct with data read from disk

    // get the size of the root directory from the supernode root inode
    int rootSize = superblock.root.file_size; // in bytes
    int locationInRoot; // location of file in root directory (used for opening an existing file)

    // check cached root directory to see if file is listed there
    char existsFlag = 0; // if the file is found in the root directory, set this flag to true
    for (int i = 0; i < rootSize / 24; i++){    // since an entry is 24 bytes long, divide size by 24 to get number of entries
        if (strcmp(rootDirectory.entries[i].name, name) == 0){  
            existsFlag = 1;       // the file exists
            locationInRoot = i;   // save the array index for later
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

        // read the free block map from memory
        char readBlock[block_size];                         // array of bytes to store read data    
        if(read_blocks(1023, 1, read_blocks) < 0){          // read the free block map block
            printf("An error occured when reading the free block map from the disk\n");
            return -1; 
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

        // write updated free block map to disk
        char freeBlockMapInBytes[sizeof freeBlockMap];                           // array of bytes 
        memcpy(freeBlockMapInBytes, &freeBlockMap, sizeof freeBlockMapInBytes);  // convert the struct into an array of bytes to write to disk
        if (write_blocks(1023, 1, freeBlockMapInBytes) < 0){                     // write to free block map
            printf("An error occured when writing the free block map to the disk\n");
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
            return -1; 
        }

        // initialize the data block
        block_t dataBlock;
        memset(dataBlock.bytes, -1, sizeof dataBlock.bytes); // fill block with empty values

        // write data block to disk
        char dataBlockInBytes[sizeof dataBlock];                           // array of bytes 
        memcpy(dataBlockInBytes, &dataBlock, sizeof dataBlockInBytes);     // convert the struct into an array of bytes to write to disk
        if (write_blocks(freeBlockNumber, 1, dataBlockInBytes) < 0){       // write to free data block
            printf("An error occured when writing the data block to the disk\n");
            return -1; 
        }
        
        // update superblock
        superblock.sfs_size = superblock.sfs_size + 2;                  // two new blocks added
        superblock.inode_table_size = superblock.inode_table_size + 1;  // one new inode added

        // write superblock to disk
        char superblockInBytes[sizeof superblock];                           // array of bytes 
        memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(0, 1, superblockInBytes) < 0){
            printf("An error occured when writing the superblock to the disk\n");
            return -1; 
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
            return -1; 
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

        // check if file is already in the fd table
        for (int i = 0; i < fdTableSize; i++){
            if (strcmp(fileDescriptorTable[i].filename, name) == 0){ // file is open already
                return fileDescriptorTable[i].fd;
            }
        }

        // get the entry using the location found earlier in the for loop
        directoryEntry_t foundFile;
        foundFile = rootDirectory.entries[locationInRoot];
        
        // read the inode from memory
        char readBlock[block_size];                                     // array of bytes to store read data    
        if(read_blocks(foundFile.inode_index, 1, read_blocks) < 0){     // use the inode number from the entry
            printf("An error occured when reading the inode from the disk\n");
            return -1; 
        }
        // initialize an inode
        inode_t foundInode;
        memcpy(&foundInode, readBlock, sizeof foundInode); // fill the inode struct with data read from disk

        // check if link count is not 0 (unused inode error)
        if (foundInode.link_count == 0){
            printf("An error occured while accessing the inode\n");
        }

        // find where the file ends
        int numBlocks; // number of blocks the file occupies

        if (foundInode.file_size % 1024 == 0){ // do file size % 1024 to get overflow bytes 
            numBlocks = foundInode.file_size / 1024; 
        }
        else {
            numBlocks = (foundInode.file_size / 1024) + 1;
        }
        
        // check if the end of the file is in a direct pointer or the indirect pointer
        if (numBlocks <= 12) { // it fits in the direct pointers
            // initialize a new entry in the fd table
            fileDescriptor_t newfdEntry;
            newfdEntry.inode = foundInode.inode_number;
            strcpy(newfdEntry.filename, foundFile.name);                // add the file name
            newfdEntry.inode = foundInode.inode_number;                 // add the inode number
            newfdEntry.fd = fdTableSize;                                // add fd number (table index)
            newfdEntry.RWBlockPointer = foundInode.direct[numBlocks];   // set RWBlockPointer to block number in direct pointer
            newfdEntry.RWBytePointer = foundInode.file_size % 1024;     // set RWBytePointer to the overflow bytes
            fileDescriptorTable[fdTableSize] = newfdEntry;              // add entry to fd table
            fdTableSize++;                                              // increase fd table size 
            
            return newfdEntry.fd;
        }
        // else check if its in the indirect pointer
        else if (numBlocks > 12 && numBlocks < (12 + 256)){ // its in the direct pointer but within the max file size
            // initialize an inode indirect array
            indirectBlock_t indirectArray;
            
            // read the indirect inode array from memory
            char readBlock[block_size];                                 // array of bytes to store read data    
            if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                printf("An error occured when reading the indirect inode array from the disk\n");
                return -1; 
            }
            memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk

            // loop through the array (size 256) until you find -1 pointer
            int blockNumber = indirectArray.pointer[255];  // if not found, then take the last entry
            for (int i = 0; i < sizeof indirectArray.pointer; i++){
                // if found, then take the entry just before
                if (indirectArray.pointer[i] == -1){
                    blockNumber = indirectArray.pointer[i - 1];
                }
            }
            
            // initialize a new entry in the fd table
            fileDescriptor_t newfdEntry;
            newfdEntry.inode = foundInode.inode_number;
            strcpy(newfdEntry.filename, foundFile.name);                // add the file name
            newfdEntry.inode = foundInode.inode_number;                 // add the inode number
            newfdEntry.fd = fdTableSize;                                // add fd number (table index)
            newfdEntry.RWBlockPointer = blockNumber;                    // set RWBlockPointer to block number in indirect pointer
            newfdEntry.RWBytePointer = foundInode.file_size % 1024;     // set RWBytePointer to the overflow bytes
            fileDescriptorTable[fdTableSize] = newfdEntry;              // add entry to fd table
            fdTableSize++;                                              // increase fd table size 
            
            return newfdEntry.fd;

        }

        else { // its outside the max file size (should be impossible)
            printf("The file is too large\n");
            return -1;
        }
    }

    // something went wrong, you should never get here
    else {
        printf("How did you get here? This code is unreachable!\n");
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
// pseudo code in comments
    // check if walkingPointer is initialized yet
    if (walkingPointer == NULL){
        // this is the first time calling the function
        // set walkingPointer equal to the startingPointer
        walkingPointer = startingPointer;
        // get the file name at the walkingPointer
        fname = walkingPointer->name;   // save it in fname
        walkingPointer++;               // increase the walkingPointer by 1
        return 1;                       // return 1 for success
    }
    else{ //we've started walking
        // compare walkingPointer and startingPointer to check they are not equal
        if (walkingPointer == startingPointer){
            // we've completed the walk through the root directory
            return 0;
        }
        else {
            // get the file name at the walkingPointer
            fname = walkingPointer->name;   // save it in fname
            walkingPointer++;               // increase the walkingPointer by 1
            return 1;
        }
    }
}
/**
 * @brief Given the file name, return its size
 * Since there are no subdirectories, the path is always the file name
 * 
 * @param path filename
 * @return fileSize file size
 */
int sfs_getfilesize(const char* path) {
// pseudo code in comments
    // search in the cached root directory for the file name
    int inodeNumber = -1;
    for (int i = 0; i < sizeof rootDirectory; i++){   // assuming the file exists, so this should not run for too long (max 42)
        if (strcmp(rootDirectory.entries[i].name, path) == 0){  
            inodeNumber = rootDirectory.entries[i].inode_index; // get the inode number
            break;
        }
    }
    if (inodeNumber == -1) { // file not found
        printf("The file was not found\n");
        return -1;
    }
    // initialize the inode
    inode_t foundInode;

    // read the inode from the disk
    char readBlock[block_size];                                 // array of bytes to store read data    
    if(read_blocks(inodeNumber, 1, read_blocks) < 0){           // read the inode block
        printf("An error occured when reading the inode from the disk\n");
        return -1; 
    }
    memcpy(&foundInode, readBlock, sizeof foundInode); // fill the struct with data read from disk

    // return the file size inside the inode
    return foundInode.file_size;
} 

/**
 * @brief Writes the contents of buf into the file at the current RWPointer location
 * This may be the end of the file, or it may be inside the file, in which case it will overwrite the file
 * 
 * @param fileID fd number of opened file
 * @param buf  data to write to the file
 * @param length length of data in bytes of what is to be written
 * @return actual number of bytes written. -1 means an error occured
 */
int sfs_fwrite(int fileID, char *buf, int length) {
// pseudo code in comments
    // I'm going to assume the fileID is the fd number in the fd table, buf is the buffer of what to write, and length is the number of bytes to write

    // initialize an fd table entry
    fileDescriptor_t fdEntry;

    // search for fd number in fd table and get inode number, RWBlockPointer, and RWBytePointer
    int fileIsOpenFlag = 0;
    for (int i = 0; i < fdTableSize; i++){
        if (fileDescriptorTable[i].fd == fileID){   // file is open already
            fileIsOpenFlag = 1;                     // flag that it's in the table (will return error otherwise)
            fdEntry = fileDescriptorTable[i];       // save the entry
        }
    }
    // if not found, return error (file not opened)
    if (fileIsOpenFlag == 0){
        printf("The file is not open. Open the file before writing\n");
        return -1;
    }
    // initialize the inode
    inode_t foundInode;

    // read the inode from the disk
    char readBlock[block_size];                                   // array of bytes to store read data    
    if(read_blocks(fdEntry.inode, 1, read_blocks) < 0){           // read the inode block
        printf("An error occured when reading the inode from the disk\n");
        return -1; 
    }
    memcpy(&foundInode, readBlock, sizeof foundInode); // fill the struct with data read from disk

    // initialize writing variables
    block_t currentBlock;           // block to read/write from
    int currentPointer = -1;        // current position in inode pointers
    int currentLength = length;     // length of buf left to write
    int addedBytes = 0;             // return value, number of bytes written (length - currentLength)
    int bufCounter = 0;             // current location in buf
    int addedBlocks = 0;            // number of blocks added to the sfs

    // initialize an inode indirect array in case we need it
    indirectBlock_t indirectArray;

    // use the RWBlock pointer by searching for it in the inode to get the starting block number
    for (int i = 0; i < 12 + 256; i++){
        if (i < 12){   // its in a direct pointer
            if (fdEntry.RWBlockPointer == foundInode.direct[i]){
                currentPointer = i; // current direct pointer index
            }
        }
        else {      // its in the indirect pointer array
            if (i == 12){ // only read the idirect array once!
                // read the indirect inode array from memory
                char readBlock[block_size];                                 // array of bytes to store read data    
                if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                    printf("An error occured when reading the indirect inode array from the disk\n");
                    return -1; 
                }
                memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
            }

            if (fdEntry.RWBlockPointer == indirectArray.pointer[i]){
                currentPointer = i; // current indirect array pointer (do i - 12 to get index in array)
            }

        }
    }

    // write the first block (which may not be a full block)
    memset(currentBlock.bytes, -1, sizeof currentBlock.bytes); // initialize data block with all empty values
    
    // read first block to get beginning bytes
    char readFirstBlock[block_size];                                // array of bytes to store read data    
    if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read first data block
        printf("An error occured when reading the first data block from the disk\n");
        return -1; 
    }
    memcpy(&currentBlock, readFirstBlock, sizeof currentBlock); // fill the struct with data read from disk

    //  write the first block by reading what we had, adding (1024 - RWBytePointer) bytes 
    //  to the end of the file, and writing that to the block

    // check if we're writing a whole block or not
    if (length <= 1024 - fdEntry.RWBytePointer){ // write entire buf
        memcpy(&currentBlock + fdEntry.RWBytePointer, buf, length);
        currentLength = 0;
    }
    else { // write first (1024 - RWBytePointer) bytes
        memcpy(&currentBlock + fdEntry.RWBytePointer, buf, (1024 - fdEntry.RWBytePointer));
        currentLength = length - (1024 - fdEntry.RWBytePointer); // what's left of buf to write
    }

    // write block to disk
    char dataBlockInBytes[sizeof currentBlock];                              // array of bytes 
    memcpy(dataBlockInBytes, &currentBlock, sizeof dataBlockInBytes);        // convert the struct into an array of bytes to write to disk
    if (write_blocks(fdEntry.RWBlockPointer, 1, dataBlockInBytes) < 0){  
        printf("An error occured when writing the block to the disk\n");
        return addedBytes; 
    }

    // update variables
    addedBytes = length - currentLength;
    bufCounter = addedBytes - 1;

    if (currentLength == 0){ // done! do cleanup 
        // update inode file size
        foundInode.file_size = foundInode.file_size + addedBytes;
        // write inode to disk
        char inodeInBytes[sizeof foundInode];                      // array of bytes 
        memcpy(inodeInBytes, &foundInode, sizeof inodeInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(foundInode.inode_number, 1, inodeInBytes) < 0){         
            printf("An error occured when writing the inode to the disk\n");
            return -1; 
        }

        // RWBlockPointer stays the same

        // set RWRBytePointer to end of file
        fdEntry.RWBytePointer = foundInode.file_size % 1024; 

        // superblock stays the same
        // free block map stays the same
     
        return addedBytes;
    }

    // loop to write additional blocks until buf is all written
    while(1){

        if (currentLength - 1024 <= 0){ // less than one full block to write
            // get next block from inode
            // currentPointer contains index of previous pointer
            currentPointer++; // get next pointer

            if (currentPointer >= 12 + 256){
                // no more pointers possible, reached max size
                printf("The max file size has been reached\n");
                // since this is bound to happen regularly, perform cleanup on this error
                
                // update inode file size
                foundInode.file_size = foundInode.file_size + addedBytes;
                // write inode to disk
                char inodeInBytes[sizeof foundInode];                      // array of bytes 
                memcpy(inodeInBytes, &foundInode, sizeof inodeInBytes);    // convert the struct into an array of bytes to write to disk
                if (write_blocks(foundInode.inode_number, 1, inodeInBytes) < 0){         
                    printf("An error occured when writing the inode to the disk\n");
                    return -1; 
                }

                // set RWBlockPointer to last block
                fdEntry.RWBlockPointer = indirectArray.pointer[255]; // last possible block
                // set RWRBytePointer to end of file
                fdEntry.RWBytePointer = foundInode.file_size % 1024; 
                
                if (addedBlocks != 0){
                    // update superblock
                    superblock.sfs_size = superblock.sfs_size + addedBlocks;

                    // write superblock to disk
                    char superblockInBytes[sizeof superblock];                           // array of bytes 
                    memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
                    if (write_blocks(0, 1, superblockInBytes) < 0){
                        printf("An error occured when writing the superblock to the disk\n");
                        return -1; 
                    }

                    // write free block map to disk
                    char freeBlockMapInBytes[sizeof freeBlockMap];                              // array of bytes 
                    memcpy(freeBlockMapInBytes, &freeBlockMap, sizeof freeBlockMapInBytes);     // convert the struct into an array of bytes to write to disk
                    if (write_blocks(1023, 1, freeBlockMapInBytes) < 0){
                        printf("An error occured when writing the free block map to the disk\n");
                        return -1; 
                    }
                }
                
                printf("The current file size is %d", foundInode.file_size); // for fun, print the file size
                return addedBytes;
            }

            else if (currentPointer < 11){ // next block is in the direct pointers
                fdEntry.RWBlockPointer = foundInode.direct[currentPointer]; // get next block number
            }

            else { // next block is in the indirect pointer array
                if (currentPointer == 12){ // only read the idirect array once!
                    // read the indirect inode array from memory
                    char readBlock[block_size];                                 // array of bytes to store read data    
                    if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                        printf("An error occured when reading the indirect inode array from the disk\n");
                        return -1; 
                    }
                    memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
                }
                // shift index such that 12 is index 0 in the array
                fdEntry.RWBlockPointer = indirectArray.pointer[currentPointer - 12];
            }

            // if next block is not defined in inode, assign one
            if (fdEntry.RWBlockPointer == -1){
                // go in free block map and get a free block
                // read the free block map from memory
                char readBlock[block_size];                         // array of bytes to store read data    
                if(read_blocks(1023, 1, read_blocks) < 0){          // read the free block map block
                    printf("An error occured when reading the free block map from the disk\n");
                    return -1; 
                }
                memcpy(&freeBlockMap, readBlock, sizeof freeBlockMap); // fill the struct with data read from disk

                int freeBlockNumber = 1024; // out of bounds
                for (int i = 0; i < sizeof freeBlockMap; i++){
                    if (freeBlockMap.freeBlockMap[i] == 0){
                        freeBlockNumber = i; // save the block number
                        freeBlockMap.freeBlockMap[i] = 1; // mark it as used
                        break;
                    }
                }
                if (freeBlockNumber == 1024){
                    printf("The disk is full. Delete files before creating a new one\n");
                    return -1;
                }
                // update addedBlocks
                addedBlocks++;

                // update the inode pointer
                if (currentPointer < 12){ // direct pointer
                    foundInode.direct[currentPointer] = freeBlockNumber;
                }
                else { // indirect pointer
                    indirectArray.pointer[currentPointer - 12] = freeBlockNumber;
                }

                // set RWBlock pointer to new block number
                fdEntry.RWBlockPointer = freeBlockNumber;
            }

            // write block (we know there's just 1 block to write)
            memset(currentBlock.bytes, -1, sizeof currentBlock.bytes);  // initialize data block with all empty values

            // read block
            char readBlock[block_size];                                     // array of bytes to store read data    
            if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read data block
                printf("An error occured when reading the data block from the disk\n");
                return -1; 
            }
            memcpy(&currentBlock, readBlock, sizeof currentBlock); // fill the struct with data read from disk

            // write from buf to block
            memcpy(&currentBlock, buf + bufCounter, currentLength); // write all that is left of buf

            // write block to disk
            char dataBlockInBytes[sizeof currentBlock];                              // array of bytes 
            memcpy(dataBlockInBytes, &currentBlock, sizeof dataBlockInBytes);        // convert the struct into an array of bytes to write to disk
            if (write_blocks(fdEntry.RWBlockPointer, 1, dataBlockInBytes) < 0){  
                printf("An error occured when writing the block to the disk\n");
                return addedBytes; 
            }

            // update variables
            addedBytes = addedBytes + currentLength; 
            bufCounter = addedBytes - 1; // this should be the end of buf
            currentLength = 0;

            // done!
            // break from while loop for cleanup
            break;
        }
        else { // more than one block to write, so write one block now then loop again

            // write full 1024 bytes to a block

            // get next block from inode
            // currentPointer contains index of previous pointer
            currentPointer++; // get next pointer

            if (currentPointer >= 12 + 256){
                // no more pointers possible, reached max size
                printf("The max file size has been reached\n");
                // since this is bound to happen regularly, perform cleanup on this error
                
                // update inode file size
                foundInode.file_size = foundInode.file_size + addedBytes;
                // write inode to disk
                char inodeInBytes[sizeof foundInode];                      // array of bytes 
                memcpy(inodeInBytes, &foundInode, sizeof inodeInBytes);    // convert the struct into an array of bytes to write to disk
                if (write_blocks(foundInode.inode_number, 1, inodeInBytes) < 0){         
                    printf("An error occured when writing the inode to the disk\n");
                    return -1; 
                }

                // if we added blocks, update the superblock, free block map, and indirect array
                if (addedBlocks != 0){
                    // update the superblock
                    superblock.sfs_size = superblock.sfs_size + addedBlocks;  // add new blocks

                    // write superblock to disk
                    char superblockInBytes[sizeof superblock];                           // array of bytes 
                    memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
                    if (write_blocks(0, 1, superblockInBytes) < 0){
                        printf("An error occured when writing the superblock to the disk\n");
                        return -1; 
                    }

                    // write free block map to disk
                    char freeBlockMapInBytes[sizeof freeBlockMap];                       // array of bytes 
                    memcpy(freeBlockMapInBytes, &freeBlockMap, sizeof freeBlockMapInBytes);    // convert the struct into an array of bytes to write to disk
                    if (write_blocks(1023, 1, freeBlockMapInBytes) < 0){
                        printf("An error occured when writing the free block map to the disk\n");
                        return -1; 
                    }

                    // write indirect array to disk
                    char indirectArrayInBytes[sizeof indirectArray];                              // array of bytes 
                    memcpy(indirectArrayInBytes, &indirectArray, sizeof indirectArrayInBytes);    // convert the struct into an array of bytes to write to disk
                    if (write_blocks(foundInode.indirect, 1, indirectArrayInBytes) < 0){
                        printf("An error occured when writing the indirect array to the disk\n");
                        return -1; 
                    }

                }
                
                printf("The current file size is %d", foundInode.file_size); // for fun, print the file size
                return addedBytes;
            }
            
            else if (currentPointer < 11){ // next block is in the direct pointers
                fdEntry.RWBlockPointer = foundInode.direct[currentPointer]; // get next block number
            }

            else { // next block is in the indirect pointer array
                if (currentPointer == 12){ // only read the idirect array once!
                    // read the indirect inode array from memory
                    char readBlock[block_size];                                 // array of bytes to store read data    
                    if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                        printf("An error occured when reading the indirect inode array from the disk\n");
                        return -1; 
                    }
                    memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
                }
                // shift index such that 12 is index 0 in the array
                fdEntry.RWBlockPointer = indirectArray.pointer[currentPointer - 12];
            }

            // if next block is not defined in inode, assign one
            if (fdEntry.RWBlockPointer == -1){
                // go in free block map and get a free block
                // read the free block map from memory
                char readBlock[block_size];                         // array of bytes to store read data    
                if(read_blocks(1023, 1, read_blocks) < 0){          // read the free block map block
                    printf("An error occured when reading the free block map from the disk\n");
                    return -1; 
                }
                memcpy(&freeBlockMap, readBlock, sizeof freeBlockMap); // fill the struct with data read from disk

                int freeBlockNumber = 1024; // out of bounds
                for (int i = 0; i < sizeof freeBlockMap; i++){
                    if (freeBlockMap.freeBlockMap[i] == 0){
                        freeBlockNumber = i; // save the block number
                        freeBlockMap.freeBlockMap[i] = 1; // mark it as used
                        break;
                    }
                }
                if (freeBlockNumber == 1024){
                    printf("The disk is full. Delete files before creating a new one\n");
                    return -1;
                }
                // update addedBlocks
                addedBlocks++;

                // update the inode pointer
                if (currentPointer < 12){ // direct pointer
                    foundInode.direct[currentPointer] = freeBlockNumber;
                }
                else { // indirect pointer
                    indirectArray.pointer[currentPointer - 12] = freeBlockNumber;
                }

                // set RWBlock pointer to new block number
                fdEntry.RWBlockPointer = freeBlockNumber;
            }

            // write full block 
            memset(currentBlock.bytes, -1, sizeof currentBlock.bytes);  // initialize data block with all empty values
            // don't read data block because we are overwriting it completely

            // write from buf to block
            memcpy(&currentBlock, buf + bufCounter, 1024); // write 1024 bytes of buf from last stopping point

            // write block to disk
            char dataBlockInBytes[sizeof currentBlock];                              // array of bytes 
            memcpy(dataBlockInBytes, &currentBlock, sizeof dataBlockInBytes);        // convert the struct into an array of bytes to write to disk
            if (write_blocks(fdEntry.RWBlockPointer, 1, dataBlockInBytes) < 0){  
                printf("An error occured when writing the block to the disk\n");
                return -1; 
            }

            // update variables
            addedBytes = addedBytes + 1024; 
            currentLength = currentLength - 1024;
            bufCounter = addedBytes - 1; 

            // don't break, do the loop again
        }

    } // end of while loop


    // done writing blocks, so cleanup!
    
    // update inode file size
    foundInode.file_size = foundInode.file_size + addedBytes;
    // write inode to disk
    char inodeInBytes[sizeof foundInode];                      // array of bytes 
    memcpy(inodeInBytes, &foundInode, sizeof inodeInBytes);    // convert the struct into an array of bytes to write to disk
    if (write_blocks(foundInode.inode_number, 1, inodeInBytes) < 0){         
        printf("An error occured when writing the inode to the disk\n");
        return -1; 
    }

    // RWBlockPointer should already be set to last block
    // set RWRBytePointer to end of file
    fdEntry.RWBytePointer = foundInode.file_size % 1024; 

    // if we added blocks, update the superblock and free block map
    if (addedBlocks != 0){
        // update the superblock
        superblock.sfs_size = superblock.sfs_size + addedBlocks;  // add new blocks

        // write superblock to disk
        char superblockInBytes[sizeof superblock];                           // array of bytes 
        memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(0, 1, superblockInBytes) < 0){
            printf("An error occured when writing the superblock to the disk\n");
            return -1; 
        }

        // write free block map to disk
        char freeBlockMapInBytes[sizeof freeBlockMap];                       // array of bytes 
        memcpy(freeBlockMapInBytes, &freeBlockMap, sizeof freeBlockMapInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(1023, 1, freeBlockMapInBytes) < 0){
            printf("An error occured when writing the free block map to the disk\n");
            return -1; 
        }

        // write indirect array to disk (may be unecessary)
        char indirectArrayInBytes[sizeof indirectArray];                              // array of bytes 
        memcpy(indirectArrayInBytes, &indirectArray, sizeof indirectArrayInBytes);    // convert the struct into an array of bytes to write to disk
        if (write_blocks(foundInode.indirect, 1, indirectArrayInBytes) < 0){
            printf("An error occured when writing the indirect array to the disk\n");
            return -1; 
        }
    }
    
    return addedBytes;
} 

/**
 * @brief Reads the contents of file into buf at the current RWPointer location
 * This may be the end of the file (which would return nothing), or it may be inside the file
 * Therefore, you should call sfs_seek first
 * 
 * @param fileID fd number
 * @param buf location to store read data
 * @param length length of bytes to read
 * @return actual number of bytes read. -1 means an error occured
 */
int sfs_fread(int fileID, char *buf, int length) {
// pseudo code:
    // I'm going to assume the fileID is the fd number in the fd table, buf is the buffer to save what is read, and length is the number of bytes to read

    // initialize an fd table entry
    fileDescriptor_t fdEntry;

    // search for fd number in fd table and get inode number, RWBlockPointer, and RWBytePointer
    int fileIsOpenFlag = 0;
    for (int i = 0; i < fdTableSize; i++){
        if (fileDescriptorTable[i].fd == fileID){ // file is open already
            fileIsOpenFlag = 1;                 // flag that it's in the table (will return error otherwise)
            fdEntry = fileDescriptorTable[i];   // save the entry
        }
    }
    // if not found, return error (file not opened)
    if (fileIsOpenFlag == 0){
        printf("The file is not open. Open the file before writing\n");
        return -1;
    }
    // initialize the inode
    inode_t foundInode;

    // read the inode from the disk
    char readBlock[block_size];                                   // array of bytes to store read data    
    if(read_blocks(fdEntry.inode, 1, read_blocks) < 0){           // read the inode block
        printf("An error occured when reading the inode from the disk\n");
        return -1; 
    }
    memcpy(&foundInode, readBlock, sizeof foundInode); // fill the struct with data read from disk

    // initialize reading variables
    block_t currentBlock;           // block to read/write from
    int currentPointer = -1;        // current position in inode pointers
    int currentLength = length;     // amount of length left to read
    int readBytes = 0;              // return value, number of bytes read (length - currentLength)
    int bufCounter = 0;             // current location in buf (addedBytes - 1)

    int numBlocks = 0;              // number of blocks the file occupies
    int lastBlockNumber = -1;       // block number of the last block the file occupies

    // initialize an inode indirect array in case we need it
    indirectBlock_t indirectArray;

    // use the RWBlockPointer by searching for it in the inode to get the starting block number
    for (int i = 0; i < 12 + 256; i++){
        if (i < 12){   // its in a direct pointer
            if (fdEntry.RWBlockPointer == foundInode.direct[i]){
                currentPointer = i; // current direct pointer index
            }
        }
        else {      // its in the indirect pointer array
            if (i == 12){ // only read the idirect array once!
                // read the indirect inode array from memory
                char readBlock[block_size];                                 // array of bytes to store read data    
                if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                    printf("An error occured when reading the indirect inode array from the disk\n");
                    return -1; 
                }
                memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
            }

            if (fdEntry.RWBlockPointer == indirectArray.pointer[i]){
                currentPointer = i; // current indirect array pointer (do i - 12 to get index in array)
            }

        }
    }

    // find when the file ends to know when to stop reading
    if (foundInode.file_size % 1024 == 0){ // do file size % 1024 to get overflow bytes 
        numBlocks = foundInode.file_size / 1024; 
    }
    else {
        numBlocks = (foundInode.file_size / 1024) + 1;
    }

    // use numBlocks to get the block number from the inode
    if (numBlocks <= 12){ // block number is in the direct pointers
        lastBlockNumber = foundInode.direct[numBlocks - 1];
    }
    else if ( numBlocks > 12 && numBlocks < 12 + 256) { // block number is in the indirect pointer array
        
        // read the indirect inode array from memory
        char readBlock[block_size];                                 // array of bytes to store read data    
        if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
            printf("An error occured when reading the indirect inode array from the disk\n");
            return -1; 
        }
        memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk

        lastBlockNumber = indirectArray.pointer[(numBlocks - 1) - 12];
    }

    // compare RWBlockPointer with lastBlockNumber
    if (fdEntry.RWBlockPointer == lastBlockNumber){
        // last block, so need to be careful how much we read!

        if (length + fdEntry.RWBytePointer <= foundInode.file_size % 1024){
            // read will end before the file ends, so read everything and return length
            // read the data block from memory
            char readBlock[block_size];                                     // array of bytes to store read data    
            if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                printf("An error occured when reading the data block from the disk\n");
                return -1; 
            }
            memcpy(&buf, readBlock + fdEntry.RWBytePointer, sizeof length); // fill the buf with data read from disk up to length

            // cleanup
            // set RWByte Pointer where length ended
            fdEntry.RWBytePointer = fdEntry.RWBytePointer + length;
            // read entire length
            readBytes = length;
            return readBytes;

        }
        else {
            // file will end before read ends, so read until the end of the file and return the difference
            // read the data block from memory
            char readBlock[block_size];                                     // array of bytes to store read data    
            if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                printf("An error occured when reading the data block from the disk\n");
                return -1; 
            }
            memcpy(&buf, readBlock + fdEntry.RWBytePointer, sizeof ((foundInode.file_size % 1024) - fdEntry.RWBytePointer)); // fill the buf with data read from disk up to end of file

            // cleanup
            // set RWByte Pointer where length ended
            fdEntry.RWBytePointer = fdEntry.RWBytePointer + length;
            // read from pointer to end of file
            readBytes = (foundInode.file_size % 1024) - fdEntry.RWBytePointer;
            return readBytes;       
        }
    }

    // not reading from last block yet, so read first block

    // check if we're reading a whole block or not
    if (length <= 1024 - fdEntry.RWBytePointer){ // read entire buf and end
        // read the data block from memory
        char readBlock[block_size];                                     // array of bytes to store read data    
        if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
            printf("An error occured when reading the data block from the disk\n");
            return -1; 
        }
        memcpy(&buf, readBlock + fdEntry.RWBytePointer, sizeof length); // fill the buf with data read from disk up to length

        // done! only reading this first block
        // cleanup
        fdEntry.RWBytePointer = fdEntry.RWBytePointer + length;
        readBytes = length;
        return readBytes;
    }
    else { // read first (1024 - RWBytePointer) bytes of length
        // read the data block from memory
        char readBlock[block_size];                                     // array of bytes to store read data    
        if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
            printf("An error occured when reading the data block from the disk\n");
            return -1; 
        }

        // fill the buf with data read from disk up to (1024 - RWBytePointer)
        memcpy(&buf, readBlock + fdEntry.RWBytePointer, sizeof (1024 - fdEntry.RWBytePointer)); 

        // update variables
        readBytes = readBytes + (1024 - fdEntry.RWBytePointer); 
        currentLength = length - readBytes;
        bufCounter = readBytes - 1;
        fdEntry.RWBytePointer = 0;

        // continue to read next blocks
    }

    // if we got here, then we need to read more blocks
    // loop to read additional blocks until all length is read
    while (1){

        if (currentLength - 1024 <= 0){ // less than one full block to read left
            // get next block from inode
            // currentPointer contains index of previous pointer
            currentPointer++; // get next pointer

            if (currentPointer >= 12 + 256){
                // reached the max file length
                printf("Reached the end of the file\n");

                // cleanup 
                return readBytes;

            }
            else if (currentPointer < 12){ // its in a direct pointer
                fdEntry.RWBlockPointer = foundInode.direct[currentPointer];
            }
            else { // its in the indirect pointer array
                if (currentPointer == 12){ // only read the idirect array once!
                    // read the indirect inode array from memory
                    char readBlock[block_size];                                 // array of bytes to store read data    
                    if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                        printf("An error occured when reading the indirect inode array from the disk\n");
                        return -1; 
                    }
                    memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
                }

                fdEntry.RWBlockPointer = indirectArray.pointer[currentPointer - 12];
            }

            // compare RWBlockPointer with lastBlockNumber
            if (fdEntry.RWBlockPointer == lastBlockNumber){
                // last block, so need to be careful how much we read!

                if (currentLength <= foundInode.file_size % 1024){
                    // read will end before the file ends, so read everything end return length
                    // read the data block from memory
                    char readBlock[block_size];                                     // array of bytes to store read data    
                    if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                        printf("An error occured when reading the data block from the disk\n");
                        return -1; 
                    }
                    memcpy(&buf, readBlock, sizeof currentLength); // fill the buf with data read from disk up to length

                    // cleanup
                    readBytes = length; // read everything!
                    fdEntry.RWBytePointer = currentLength - 1; // from zero to end
                    return readBytes;

                }
                else {
                    // file will end before read ends, so read until the end of the file and return the difference
                    // read the data block from memory
                    char readBlock[block_size];                                     // array of bytes to store read data    
                    if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                        printf("An error occured when reading the data block from the disk\n");
                        return -1; 
                    }
                    memcpy(&buf, readBlock, sizeof (foundInode.file_size % 1024)); // fill the buf with data read from disk up to end of file

                    // cleanup
                    readBytes = currentLength + foundInode.file_size % 1024;
                    fdEntry.RWBytePointer = foundInode.file_size % 1024; // end of file
                    return readBytes;       
                }
            }

            // not reading last block of file
            // read the data block from memory
            char readBlock[block_size];                                     // array of bytes to store read data    
            if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                printf("An error occured when reading the data block from the disk\n");
                return -1; 
            }

            // add the last of the data read from disk to buf
            memcpy(&buf + bufCounter, readBlock, sizeof currentLength); 

            // done! this was the last block to read!
            // cleanup
            fdEntry.RWBytePointer = currentLength - 1; // last place we read
            readBytes = readBytes + currentLength; // size of buf
            return readBytes;
        }

        else { // read a full block and redo the loop

            // get next block from inode
            // currentPointer contains index of previous pointer
            currentPointer++; // get next pointer

            if (currentPointer >= 12 + 256){
                // reached the max file length
                printf("Reached the end of the file\n");

                // cleanup 
                fdEntry.RWBytePointer = 1023; // end of last block
                return readBytes;

            }

            else if (currentPointer < 12){ // its in a direct pointer
                fdEntry.RWBlockPointer = foundInode.direct[currentPointer];
            }

            else { // its in the indirect pointer array
                if (currentPointer == 12){ // only read the idirect array once!
                    // read the indirect inode array from memory
                    char readBlock[block_size];                                 // array of bytes to store read data    
                    if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
                        printf("An error occured when reading the indirect inode array from the disk\n");
                        return -1; 
                    }
                    memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
                }

                fdEntry.RWBlockPointer = indirectArray.pointer[currentPointer - 12];
            }

            // compare RWBlockPointer with lastBlockNumber
            if (fdEntry.RWBlockPointer == lastBlockNumber){
                // last block, so need to be careful how much we read!

                if (currentLength <= foundInode.file_size % 1024){
                    // read will end before the file ends, so read everything end return length
                    // read the data block from memory
                    char readBlock[block_size];                                     // array of bytes to store read data    
                    if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                        printf("An error occured when reading the data block from the disk\n");
                        return -1; 
                    }
                    memcpy(&buf, readBlock, sizeof currentLength); // fill the buf with data read from disk up to length

                    // cleanup
                    fdEntry.RWBytePointer = currentLength - 1;
                    readBytes = length; // read everything!
                    return readBytes;

                }
                else {
                    // file will end before read ends, so read until the end of the file and return the difference
                    // read the data block from memory
                    char readBlock[block_size];                                     // array of bytes to store read data    
                    if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                        printf("An error occured when reading the data block from the disk\n");
                        return -1; 
                    }
                    memcpy(&buf, readBlock, sizeof (foundInode.file_size % 1024)); // fill the buf with data read from disk up to end of file

                    // cleanup
                    fdEntry.RWBytePointer = foundInode.file_size % 1024; // end of file
                    readBytes = currentLength + foundInode.file_size % 1024;
                    return readBytes;       
                }
            }

            // not reading last block of file
            // read the data block from memory
            char readBlock[block_size];                                     // array of bytes to store read data    
            if(read_blocks(fdEntry.RWBlockPointer, 1, read_blocks) < 0){    // read the data block
                printf("An error occured when reading the data block from the disk\n");
                return -1; 
            }

            // add the last of the data read from disk to buf
            memcpy(&buf + bufCounter, readBlock, sizeof currentLength); 

            // update variables
            readBytes = readBytes + currentLength; 
            currentLength = length - readBytes;
            bufCounter = readBytes - 1;
            fdEntry.RWBytePointer = 0;

            // restart the loop
        }
        
    } // end of while loop

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
    // Since I use a block pointer and a byte pointer, 
    // I have to do an extra read to get the block number from the inode

    // use fd table to get the inode number

    // initialize an fd table entry
    fileDescriptor_t fdEntry;

    // search for fd number in fd table and get inode number, RWBlockPointer, and RWBytePointer
    int fileIsOpenFlag = 0;
    for (int i = 0; i < fdTableSize; i++){
        if (fileDescriptorTable[i].fd == fileID){ // file is open already
            fileIsOpenFlag = 1;                 // flag that it's in the table (will return error otherwise)
            fdEntry = fileDescriptorTable[i];   // save the entry
        }
    }
    // if not found, return error (file not opened)
    if (fileIsOpenFlag == 0){
        printf("The file is not open. Open the file before seeking\n");
        return -1;
    }
    // initialize the inode
    inode_t foundInode;
    // initialize indirect array 
    indirectBlock_t indirectArray;

    // read the inode from the disk
    char readBlock[block_size];                                   // array of bytes to store read data    
    if(read_blocks(fdEntry.inode, 1, read_blocks) < 0){           // read the inode block
        printf("An error occured when reading the inode from the disk\n");
        return -1; 
    }
    memcpy(&foundInode, readBlock, sizeof foundInode); // fill the struct with data read from disk

    // check if loc is within the file size
    if (loc > foundInode.file_size){
        printf("Seeking location is outside of the file size\n");
        return -1;
    }

    int numBlocks = -1; // number of blocks loc takes up (use to find inode pointer)

    // set the RWBytePointer
    fdEntry.RWBytePointer = loc % 1024;
    if (fdEntry.RWBytePointer == 0){
        // no overflow
        numBlocks = loc / 1024;
    }
    else {
        // overflow to next block
        numBlocks = (loc / 1024) + 1;
    }

    // use numBlocks to get the block number from the inode
    if (numBlocks <= 12){ // block number is in the direct pointers
        fdEntry.RWBlockPointer = foundInode.direct[numBlocks - 1];
    }
    else if ( numBlocks > 12 && numBlocks < 12 + 256) { // block number is in the indirect pointer array
        
        // read the indirect inode array from memory
        char readBlock[block_size];                                 // array of bytes to store read data    
        if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
            printf("An error occured when reading the indirect inode array from the disk\n");
            return -1; 
        }
        memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk

        fdEntry.RWBlockPointer = indirectArray.pointer[(numBlocks - 1) - 12];
    }

    // done!
    return 0; // success
} 

/**
 * @brief Close the opened file with given fd number
 * Return 0 on success, -1 on error
 * 
 * @param fileID fd number
 * @return int 0 on success, -1 on error
 */
int sfs_fclose(int fileID) {
// pseudo code in comments

    // search for fd number in fd table and get index
    int index = -1;
    for (int i = 0; i < fdTableSize; i++){
        if (fileDescriptorTable[i].fd == fileID){ // file is opened
            index = i; // save location to delete after
            break; 
        }
    }

    if (index == -1){ // if not found, return error (file not opened)
        printf("The file is not open. Open the file before closing\n");
        return -1;
    } else {
        // remove entry from table
        for(int i = index; i < fdTableSize - 1; i++){
            fileDescriptorTable[i] = fileDescriptorTable[i+1];
        }

        fdTableSize--; // decrease table size
        return 0;
    }
}

/**
 * @brief Deletes the file from the file system
 * Returns 0 on success, -1 on error
 * 
 * @param file file name
 * @return int 0 on success, -1 on error
 */
int sfs_remove(char *file) {
// pseudo code in comments

    // use file name to get inode number from cached root directory
    int inodeNumber = -1;
    int rootIndex = -1; // remember location for removing later
    for (int i = 0; i < (sizeof rootDirectory.entries) / 24; i++){
      if (strcmp(rootDirectory.entries[i].name, file) == 0){  
            inodeNumber = rootDirectory.entries[i].inode_index;
            rootIndex = i;
            break;
        }
    }

    if (inodeNumber == -1){
        printf("File not found\n");
        return -1;
    }

    // use free block map in cache
    // use superblock in cache

    // initialize the inode
    inode_t foundInode;

    // read the inode from the disk
    char readBlock[block_size];                                   // array of bytes to store read data    
    if(read_blocks(inodeNumber, 1, read_blocks) < 0){             // read the inode block
        printf("An error occured when reading the inode from the disk\n");
        return -1; 
    }
    memcpy(&foundInode, readBlock, sizeof foundInode); // fill the struct with data read from disk

    // flag for if there's no indirect pointers
    int indirectPointers = 1; // initially true
    // count blocks deleted to update superblock later
    int deletedBlocks = 0;

    // for every direct pointer in the inode
    for (int i = 0; i < 12; i++){
        // check if pointer is valid
        if (foundInode.direct[i] != -1){
            // write block to disk
            char dataBlockInBytes[blockSize];                             // array of bytes 
            memset(dataBlockInBytes, -1, sizeof dataBlockInBytes);        // convert the struct into an array of bytes to write to disk 
            if (write_blocks(foundInode.direct[i], 1, dataBlockInBytes) < 0){  
                printf("An error occured when erasing the block\n");
                return -1; 
            }
            // remove from the free block map
            freeBlockMap.freeBlockMap[foundInode.direct[i]] = 0; 

            // increment deletedBlocks counter for supernode later
            deletedBlocks++;
        }
        else{
            // no more pointers
            // no indirect pointers either, so flag
            indirectPointers = 0; // set to false
            break;
        }
    }

    // if indirect flag not set, go through indirect pointers
    // initialize indirect array
    indirectBlock_t indirectArray;

    if (indirectPointers == 1){
        // get indirect pointer array
        // read the indirect inode array from memory
        char readBlock[block_size];                                 // array of bytes to store read data    
        if(read_blocks(foundInode.indirect, 1, read_blocks) < 0){   // read the indirect array block
            printf("An error occured when reading the indirect inode array from the disk\n");
            return -1; 
        }
        memcpy(&indirectArray, readBlock, sizeof indirectArray); // fill the struct with data read from disk
        
        // for every pointer in the array
        for (int i = 0; i < 256; i++){
            // check if pointer is valid
            if (indirectArray.pointer[i] != -1){
                // write block to disk
                char dataBlockInBytes[blockSize];                             // array of bytes 
                memset(dataBlockInBytes, -1, sizeof dataBlockInBytes);        // convert the struct into an array of bytes to write to disk
                if (write_blocks(indirectArray.pointer[i], 1, dataBlockInBytes) < 0){  
                    printf("An error occured when erasing the block\n");
                    return -1; 
                }
                // remove from the free block map
                freeBlockMap.freeBlockMap[indirectArray.pointer[i]] = 0; 

                // increment deletedBlocks counter for supernode later
                deletedBlocks++;
            }
            else {
                // no more pointers
                // delete indirect array block
                char indirectArrayInBytes[sizeof indirectArray];                  // array of bytes 
                memset(indirectArrayInBytes, -1, sizeof indirectArrayInBytes);    // convert the struct into an array of bytes to write to disk
                if (write_blocks(foundInode.indirect, 1, indirectArrayInBytes) < 0){
                    printf("An error occured when erasing the indirect array\n");
                    return -1; 
                }
                // remove from the free block map
                freeBlockMap.freeBlockMap[foundInode.indirect] = 0; 
                // increment deletedBlocks counter for supernode later
                deletedBlocks++;
                break;
            }
        }
    }

    // done erasing data blocks

    // delete inode
    char inodeInBytes[sizeof foundInode];             // array of bytes 
    memset(inodeInBytes, -1, sizeof inodeInBytes);    // convert the struct into an array of bytes to write to disk
    if (write_blocks(foundInode.inode_number, 1, inodeInBytes) < 0){
        printf("An error occured when writing the superblock to the disk\n");
        return -1; 
    }
    // remove from the free block map
    freeBlockMap.freeBlockMap[foundInode.indirect] = 0; 
    // increment deletedBlocks counter for supernode later
    deletedBlocks++;
    
    // delete entry from root
    for(int i = rootIndex; i < (sizeof rootDirectory.entries / 24) - 1; i++){
        rootDirectory.entries[i] = rootDirectory.entries[i+1];
    }

    // write root to memory
    char rootInBytes[sizeof rootDirectory];                     // array of bytes 
    memcpy(rootInBytes, &rootDirectory, sizeof rootInBytes);    // convert the struct into an array of bytes to write to disk
    if (write_blocks(1, 1, rootInBytes) < 0){                   // root is always block 1
        printf("An error occured when writing the root directory to the disk\n");
        return -1; 
    }

    // write free block map to disk
    char freeBlockMapInBytes[sizeof freeBlockMap];                       // array of bytes 
    memcpy(freeBlockMapInBytes, &freeBlockMap, sizeof freeBlockMapInBytes);    // convert the struct into an array of bytes to write to disk
    if (write_blocks(1023, 1, freeBlockMapInBytes) < 0){
        printf("An error occured when writing the free block map to the disk\n");
        return -1; 
    }

    // update the superblock in cache
     superblock.sfs_size = superblock.sfs_size - deletedBlocks; 

    // write the superblock to memory
    char superblockInBytes[sizeof superblock];                           // array of bytes 
    memcpy(superblockInBytes, &superblock, sizeof superblockInBytes);    // convert the struct into an array of bytes to write to disk
    if (write_blocks(0, 1, superblockInBytes) < 0){
        printf("An error occured when writing the superblock to the disk\n");
        return -1; 
    }

    // search for file name in fd table to know if we must delete it
    int fdIndex = -1;
    for (int i = 0; i < fdTableSize; i++){
        if (strcmp(fileDescriptorTable[i].filename, file) == 0){ // file is opened
            fdIndex = i; // save location to delete after
            break; 
        }
    }

    if (fdIndex != -1){ // if found
        // remove entry from table
        for(int i = fdIndex; i < fdTableSize - 1; i++){
            fileDescriptorTable[i] = fileDescriptorTable[i+1];
        }
        fdTableSize--; // decrease table size
    }
    
    // done! 
    return 0; // success
}

