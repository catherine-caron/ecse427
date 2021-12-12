//writing the same string many times to a file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main() {
    mksfs(1);
    int f = sfs_fopen("some_name.txt");
    char my_data[] = "The quick brown fox jumps over the lazy dog";
    int N = 400;
    for(int i = 0; i < N; i++) {
        sfs_fwrite(f, my_data, sizeof(my_data));
    }
    char out_data[32768];
    sfs_fseek(f, 0);   
    sfs_fread(f, out_data, sizeof(out_data)+1);
    printf("%s\n", out_data);
    sfs_fclose(f);
    //sfs_remove("some_name.txt");
}
