#ifndef SFS_API_H
#define SFS_API_H

#define blockSize 1024          /* recommended size                                     */
#define sizeOfPointer 4         /* 4 byte pointers                                      */
#define numberOfBlocks 1024     /* 1024 blocks X 1024 block size = 1MB disk size        */

// #define numberOfInodes 256      /* max number of iNodes is equal to number of blocks of the partition */
                                // used to be 200, changed to 256
                                // may not need this, it limits the number of files available.  

// #define sizeOfInode 64          /*   not needed          */
// #define sizeOfSuperBlockField 4 /*   equal to pointer size, which is 4bytes for everyone    */
// #define numberOfEntries 64      /*  number of entries in directory should be flexible    */

/* structs */

/**
 * I-node struct
 * Contains the inode number, file size, mode, link count, 12 direct pointers, and 1 indirect pointer. 
 * An inode occupies 1 block of data on the disk
 * Because the root directory is restricted in size, the max inode number is 42
 * Empty pointers should contain -1
 * 
 */
typedef struct {
    int inode_number;   // inode number in inode table (max 42)
	int file_size;      // total number of bytes for the file
    int mode;           // ???
    int link_count;     // counts number of files linked to this inode (can also use to check if inode is valid if non-zero)
	int direct[12];     // 12 direct pointers of size 4 bytes each point directly to file data blocks
	int indirect;       // 1 indirect pointer points to an array of 1024 direct pointers 
} inode_t;

/**
 * I-node Block
 * 1 inode occupies 1 block
 * 1 block is 1024 bytes, and 1 inode is 64 bytes, so there's 956 bytes wasted (fill)
 * Because the root directory is restricted in size, the max inode number is 42
 * 
 */
typedef struct {
	inode_t inode;      // the inode contained in this inode block
    char fill[956];     // wasted space in the block
} inodeBlock_t;

/**
 * Superblock
 * This is the first block containing all the data for the file system
 * It contains the madgic number, the block size, the current file system size,  
 * 
 */
typedef struct {
    unsigned char magic[4];             // magic number equals 0xACBD0005
    int block_size;                     // equals 1024
    int fs_size;                        // number of blocks, equals 1024
    int inode_table_size;               // inode table length, max 42
    inode_t root;                       // inode of root directory (64 bytes)
                                        // this may not be needed since the root is only 1 block
    
    // int rootDirectoryBlockNumber[4];    // extra: block number of the actual root directory

    char fill[944];                     // 16 + 64 = 80 bytes used means 1024 - 80 = 944 bytes wasted
} superblock_t;

/**
 * Block
 * 1 block is 1024 bytes 
 * If an inode indirect pointer points to a block, it becomes an array of 256 4 byte pointers to other blocks
 * Otherwise, it contains file data
 * 
 */
typedef struct{
	unsigned char bytes[blockSize];     // 1024 block size 
}block_t;

// /* File Descriptor */ // may be only in temp memory???
// typedef struct {
//     int free;           // free space
//     int inode;          //
//     int rwptr;          // read/write pointer
// 	// int readptr;     // probably not needed
// } fileDescriptor_t;
 
/**
 * Directory Entry
 * Each entry maps the file name to the inode number
 * The file name is restricted to 20 chars long, where 16 chars are for the given name, 1 is for the . 
 * and the last three are for the file extension
 * 
 */
typedef struct {
    char name[20];      // limited name size (16 chars + 4 chars for extension)
    int inode_index;    // inode number in inode table
} directoryEntry_t;

/**
 * Root Directory
 * The root directory contains all the entries that map the file names to the inode numbers
 * Since an entry is 20 + 4 = 24 bytes in size, 1 block can contain 42 entries
 * I chose to make the root directory only one block to simplify the implementation, 
 * which is suitable for a file system of only 1 MB with 1024 a block size
 * Future improvements could be made to support more than just 42 files
 */

typedef struct{
	directoryEntry_t entries[42];   // array of all entries in the root directory
    char fill[16];                  // 1024 - (24 bytes X 42 entries) = 16 bytes of wasted space
} rootDirectory_t;

/* API Methods implemented in sfs_api.c */
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
