Notes to clean up later:

1024 block size
1024 blocks in file system
1 block for superblock at address 0
1 block for root directory (design choice, since 1 MB is a small file system anyway) at block address 1
1 block for free block map (1 byte to represent free (1) or used (0)) at block address 1023 (last block)

the inode number is equal to the block address, so you can get the location directly from the root directory table
therefore the root directory is the inode table too

cache a copy of the root directory in sfs_api.c or .h to make things faster to access. Any updates must be done to both cache and disk
cache a file descriptor table that holds the inode number and R/W pointer for every open file. Index is the file descriptor number

in sfs implementation, you need to do all the calculations necessary then call the disk operations to read/write

RW pointer is actually two things:
    1. the block number/address the pointer is in (RWBlockPointer)
    2. the bytes after the start of the block its pointing at (RWBytePointer)

write: could update it to be able to write to more than two blocks (sue read implementation)
read: could stop if we reach the end of the file



Write: consider changing it to cases: write to one block, write to onyl whole blocks, write to multiple blocks with a partially fileld block at the end
then set these flags or categorize these as 3 if statements, and reorganize the code to go less calculations
