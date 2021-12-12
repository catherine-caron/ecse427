//test writing multiple files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main() {
    mksfs(1);
    int f1 = sfs_fopen("some_name.txt");
    int f2 = sfs_fopen("sith.txt");

    //file 1
    char my_data[] = "The quick brown fox jumps over the lazy dog";
    char out_data[1024];
    sfs_fwrite(f1, my_data, sizeof(my_data)+1);
    sfs_fseek(f1, 0);   
    sfs_fread(f1, out_data, sizeof(out_data)+1);
    
    
    //file 2
    char sith[] = "Did you ever hear the tragedy of Darth Plagueis the Wise? I thought not. It's not a story the Jedi would tell you. It's a Sith legend. Darth Plagueis was a Dark Lord of the Sith, so powerful and so wise he could use the Force to influence the midichlorians to create life... He had such a knowledge of the dark side that he could even keep the ones he cared about from dying. The dark side of the Force is a pathway to many abilities some consider to be unnatural. He became so powerful... the only thing he was afraid of was losing his power, which eventually, of course, he did. Unfortunately, he taught his apprentice everything he knew, then his apprentice killed him in his sleep. Ironic, he could save others from death, but not himself.";
    char out_sith[strlen(sith) + 1];
    sfs_fwrite(f2, sith, sizeof(sith) + 1);
    sfs_fseek(f2, 0);
    sfs_fread(f2, out_sith, sizeof(out_sith) + 1);

    printf("%s\n", out_data);
    printf("%s\n", out_sith);
    sfs_fclose(f1);
    //sfs_remove("some_name.txt");
}
