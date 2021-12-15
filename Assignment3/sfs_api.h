#ifndef SFS_API_H
#define SFS_API_H

#define blockSize 1024          /* recommended size                                     */
#define sizeOfPointer 4         /* 4 byte pointers                                      */
#define numberOfBlocks 1024     /* 1024 blocks X 1024 byte block size = 1MB disk size   */

// -------------------------------------------------------------------------------------------------

/* Structs */

/**
 * I-node
 * Contains the inode number, file size, mode, link count, 12 direct pointers, and 1 indirect pointer. 
 * An inode occupies 1 block of data on the disk
 * 1 block is 1024 bytes, and 1 inode is 64 bytes, so there's 956 bytes wasted (fill)
 * Because the root directory is restricted in size, the max inode number is 42
 * Empty pointers should contain -1
 * 
 */
typedef struct {
    int inode_number;   // inode number in inode table (max 42)
	int file_size;      // total number of bytes for the file
    int mode;           // unused
    int link_count;     // counts number of files linked to this inode (can also use to check if inode is valid if non-zero)
	int direct[12];     // 12 direct pointers of size 4 bytes each point directly to file data blocks
	int indirect;       // 1 indirect pointer points to an array of 256 pointers 
    char fill[956];     // wasted space in the block
} inode_t;

/**
 * Root Inode
 * Since the root inode is contained in the Superblock, we don't want to fill it with empty space. 
 * To solve this, I created a special struct just for the root inode
 * It contains everything a regular inode would contain except the fill array
 * 
 */
typedef struct {
    int inode_number;   // inode number in inode table (max 42)
	int file_size;      // total number of bytes for the file
    int mode;           // unused
    int link_count;     // counts number of files linked to this inode (can also use to check if inode is valid if non-zero)
	int direct[12];     // 12 direct pointers of size 4 bytes each point directly to file data blocks
	int indirect;       // 1 indirect pointer points to an array of 256 pointers 
} rootInode_t;

/**
 * Superblock
 * This is the first block containing all the data for the file system
 * It contains the madgic number, the block size, the current file system size, and the inode of the root directory
 * Since the root directory is only 1 block and is always located at block 1, saving the root inode is unecessary
 * However, it is implemented because a further improvement could be to expand the root directory
 * 
 */
typedef struct {
    int magic;                          // magic number equals 0xACBD0005 or 2898067461 in decimal
    int block_size;                     // always equals 1024
    int sfs_size;                       // current number of blocks, max 1024
    int inode_table_size;               // current inode table length, max 42
    rootInode_t root;                   // inode of root directory (64 bytes)
    char fill[944];                     // 16 + 64 = 80 bytes used means 1024 - 80 = 944 bytes wasted
} superblock_t;

/**
 * Data Block
 * 1 block is 1024 bytes 
 * If an inode indirect pointer points to a block, it becomes an array of 256 4 byte pointers to other blocks
 * Otherwise, it contains file data
 * Since there's one superblock, one root directory block, and one bitmap block,
 * there can be at most 1024 - 3 - inode_table_size blocks in the file system
 * 
 */
typedef struct{
	unsigned char bytes[blockSize];     // 1024 block size 
}block_t;

/**
 * Indirect Block Pointer Array
 * This is the block that an indirect pointer points too
 * It is a list of 256 4 byte pointers that point to data blocks for files
 * Since a block is 1024 bytes and a pointer is 4 bytes, there are 256 entries in the array
 * An empty pointer is represented by a -1
 * 
 */
typedef struct{
    int pointer[256];               // 1024 block size / 4 byte pointers = 256 pointers in the array
}indirectBlock_t;

/**
 * File Descriptor Entry
 * Each entry represents an open file
 * The filename is restricted to 20 characters (16 characters for the name, 1 for the period, and 3 for the extension)
 * The fd number is used in function calls to refer to the open file
 * The Read/Write pointer is divided into two parts: 
 *      - The RWBlockPointer contains the block number that the R/W pointer is current located in
 *      - The RWBytePointer contains the byte location within that block
 * 
 */
typedef struct {
    char filename[20];      // the file name
    int fd;                 // the file descriptor number
    int inode;              // the inode number for the file
    int RWBlockPointer;     // the Read/Write pointer that represents the block number
    int RWBytePointer;      // the Read/Write pointer that represents the byte location in the block
}fileDescriptor_t;
 
/**
 * Free Block Map
 * A Block where every byte represents the availability of a block in the file system (in order)
 *      - 1 means the block is used
 *      - 0 means the block is free
 * Block 0 is the superblock, block 1 is the root directory, and block 1023 is the free block map
 * These blocks are always in use
 * 
 */
typedef struct{
    char freeBlockMap[1024];        // each byte represents a block (1024 blocks in total)
}freeBlockMap_t;

/**
 * Directory Entry
 * Each entry maps the file name to the inode number
 * The file name is restricted to 20 chars long, where 16 chars are for the given name, 1 is for the . 
 * and the last three are for the file extension
 * Maximum of 42 entries can exist at once because the root directory is only 1 block
 * 
 */
typedef struct {
    char name[20];      // limited name size (16 chars + 4 chars for extension)
    int inode_index;    // inode number in inode table
}directoryEntry_t;

/**
 * Root Directory
 * The root directory contains all the entries that map the file names to the inode numbers
 * Since an entry is 20 + 4 = 24 bytes in size, 1 block can contain 42 entries
 * I chose to make the root directory only one block to simplify the implementation, 
 * which is suitable for a file system of only 1 MB with 1024 a block size
 * Future improvements could be made to support more than just 42 files
 * 
 */
typedef struct{
	directoryEntry_t entries[42];   // array of all entries in the root directory
    char fill[16];                  // 1024 - (24 bytes X 42 entries) = 16 bytes of wasted space
}rootDirectory_t;

// -------------------------------------------------------------------------------------------------

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
