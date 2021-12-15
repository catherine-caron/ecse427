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

Unfortunately, my code does not run, but it compiles. As you will see, my code is commented with my intentions at every step. You can compile the code using `gcc sfs_test0.c sfs_api.c disk_emu.c -o output`. I worked on this every week since it came out and put in so many hours of work, but this is my first class ever writing in C, so I still struggle a lot with debugging. Please consider my logic and pseudo code when grading so I can still get part marks.

Thank you!