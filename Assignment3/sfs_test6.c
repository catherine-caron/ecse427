//writing to multiple files while they are all open
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main() {
    mksfs(1);
    int NUM_FILES = 10;
    int files[NUM_FILES];
    char filenames[10][6] = {"0.txt","1.txt","2.txt","3.txt","4.txt","5.txt","6.txt","7.txt","8.txt","9.txt"};
    memset(files, -1, NUM_FILES);
    for(int i = 0; i < NUM_FILES; i++) {
        files[i] = sfs_fopen(filenames[i]);
        sfs_fclose(files[i]);
    }

    mksfs(0);

    for(int i = 0; i < NUM_FILES; i++) {
        files[i] = sfs_fopen(filenames[i]);
    }

    for(int i = 0; i < NUM_FILES; i++) {
        int f = files[i];
        char my_data[] = "The quick brown fox jumps over the lazy dog";
        char out_data[strlen(my_data) + 1];
        int writtenBytes = sfs_fwrite(f, my_data, sizeof(my_data)+1);
        int status = sfs_fseek(f, 0);   
        int readBytes = sfs_fread(f, out_data, sizeof(out_data)+1);
        printf("%s\n", out_data);
    }

    for(int i = 0; i < NUM_FILES; i++) {
        sfs_fclose(files[i]);
    }
    //sfs_remove("some_name.txt");
}
