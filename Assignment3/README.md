This is a simple file system that creates, opens, writes, reads, seeks, closes, and removes files. The following API functions are supported:
- void mksfs(int)
- int sfs_getnextfilename(char*)
- int sfs_getfilesize(const char*)
- int sfs_fopen(char*)
- int sfs_fclose(int)
- int sfs_fwrite(int, char*, int)
- int sfs_fread(int, char*, int)
- int sfs_fseek(int, int)
- int sfs_remove(char*)

I made some decisions when implementing the sfs:
* First, I chose to set the block size to 1024 bytes and the number of blocks to 1024 blocks. This means the sfs size is about 1Mbyte, which is quite small, but makes the free block map easier to implement. Instead of a free bit map, I used one block as a free block map, where every byte represents one block in memory. If the byte is 1, the block with block number equal to index number is used, and if the byte is 0, it is free. 
* Since the file system is quite small, I chose to make the root directory only one block. By restricting the size of the root, the number of inodes, and therefore the number of files, is restricted to 42. This is reasonable for the sfs size but too small for real life situations. 
* Instead of a single Read/Write (RW) pointer that points to the byte location in the file, I chose to split the pointer into a `RWBlockPointer` that points to the block number, and a `RWBytePointer` that points to the byte location within the block. This doesn't change the number of reads much overall, but it did make calculations easier for me. The file descriptor table therefore has both pointers in every entry.
* I chose to cache the root directory, superblock, free block map, and root inode. Although the root inode is never used, the other structs are used frequently. Whenever I update these cached structs, I also write them to memory at the end of the function. 
* To implement `getnextfilename()`, I used a startingPointer and walkingPointer to point to the root directory entries. The `startingPointer` is always pointing to the top entry, and the `walkingPointer` moves everytime the function is called. 
* When an error occurs, usually the function returns -1. Occasionally, it will return a different value related to the function. Please read the function description for more information.

Unfortunately, my code does not run properly, but it compiles. When testing, I would get segfault, but I could see that the disk file is created and intialized, so the segfaults happens after. As you will see, my code is commented with my intentions at every step. You can compile the code using `gcc sfs_test0.c sfs_api.c disk_emu.c -o output`, then attempt to test it using `./output`. I worked on this assignment every week since it came out and put in so many hours of work, but this is my first class ever writing in C, so I still struggle a lot with debugging. Segfaults are still such a mystery to me. Please consider my logic and pseudo code when grading so I can still get part marks.

Thank you!