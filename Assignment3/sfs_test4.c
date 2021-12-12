//test doing multiple writes to the same file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main() {
    mksfs(1);
    int f = sfs_fopen("some_name.txt");

    char my_data1[] = "The quick brown fox";
    char my_data2[] = " jumps over "; 
    char my_data3[] = "the lazy dog";
    char out_data[1024];
    sfs_fwrite(f, my_data1, sizeof(my_data1));
    sfs_fwrite(f, my_data2, sizeof(my_data2));
    sfs_fwrite(f, my_data3, sizeof(my_data3) + 1);
    sfs_fseek(f, 0);   
    sfs_fread(f, out_data, sizeof(out_data)+1);


    printf("%s\n", out_data);
    sfs_fclose(f);
    //sfs_remove("some_name.txt");
}
