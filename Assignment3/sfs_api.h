#ifndef SFS_API_H
#define SFS_API_H

// size of components
#define blockSize 1024          /* recommended size                                     */
#define sizeOfPointer 4         /* bytes                                                */
#define numberOfBlocks 1024     /*                                                      */
#define numberOfInodes 200      /*  max number of iNodes is equal to number of blocks of the partition */
#define sizeOfInode 64          /*                                                      */
#define sizeOfSuperBlockField 4 /*                                                      */
#define numberOfEntries 64      /*                                                      */

/* structs */

/* I-Node */
// non standard inode 
// size field  total number of bytes 
// no need to know about indirect 
// these block contains data

//File has lots of inodes 
// for j node   size = size of the file 
// set size = -1 to be blank

/* I-Node */
typedef struct {
	int size; // total number of bytes
	int direct[14]; // 
	int indirect;
} inode_t;

/* I-Node Block */
typedef struct {
	inode_t inodeSlot[16];
} inodeBlock_t;

// root is a jnode????
// have to add shadow jnode later????
// need to fill to get to 1024 or  copy memory to block_t then pass that to the disk * calloc

/* Superblock */
typedef struct {
    unsigned char magic[4];             // magic number
    int block_size;                     // equals 1024
    int fs_size;                        // number of blocks
    int Inodes;                         // iNode table length
    inode_t root;                       // iNode number of root directory 
    int rootDirectoryBlockNumber[4];    // 
    
    inode_t shadow[4];      // ?????
    int lastShadow;

    char fill[668];                     // unused space filled with empty values (may need to increase this amount)
} superblock_t;

/* Block */
typedef struct{
	unsigned char bytes[blockSize];     // 1024
}block_t;

/* File Descriptor */
typedef struct {
    int free;           // free space
    int inode;          //
    int rwptr;          // read/write pointer
	// int readptr;     // probably not needed
} fileDescriptor_t;
 
/* Directory entry */
typedef struct {
    char name[10];      // limited name size
    int inodeIndex;     // index in inode table
} directoryEntry_t;

/* Root Directory */
typedef struct{
	directoryEntry_t entries[64];   // special type of directory entry referencing all others
} rootDirectory_t;

/* methods */
void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#endif
