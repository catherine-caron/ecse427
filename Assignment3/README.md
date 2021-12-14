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

write: could update it to be able to write to more than two blocks (see read implementation)
read: could stop if we reach the end of the file


Write: consider changing it to cases: write to one block, write to onyl whole blocks, write to multiple blocks with a partially fileld block at the end
then set these flags or categorize these as 3 if statements, and reorganize the code to go less calculations


TODO:
- write to multiple blocks in one write call
- write one block at a time so you can fail when the max is hit and know how big the system is
- read must only read valid data and return the number of bytes read and not the buf requested
- write must return the actual number of bytes written so you can detect errors and measure how much was written

---

Write: consider changing it to cases: write to one block, write to only whole blocks, write to multiple blocks with a partially filled block at the end
then set these flags or categorize these as 3 if statements, and reorganize the code to do less calculations

need to check if you can save -1 in 1 byte of data (char) or if it must be an int. if so, change -1 to 0 to represent empty

-> simplify design to pass test0 then add on stuff to pass more tests. leave the logic in pseudocode to get part marks and remember what you are missing
----

may have to change addedBytes = length - currentlength every time in write