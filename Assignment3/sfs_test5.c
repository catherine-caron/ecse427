//test a really long write that should hit the indirect pointers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main() {
    mksfs(1);
    int f = sfs_fopen("some_name.txt");
    char my_data[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Suspendisse blandit quam ac mi tincidunt elementum. Proin lacinia risus lacinia libero dignissim efficitur. Etiam placerat faucibus libero sed hendrerit. Duis ut posuere augue. Morbi tellus lectus, mattis et tristique ut, commodo non nisi. Nullam suscipit, ipsum vitae gravida dictum, nisl orci cursus magna, ut auctor nisl metus malesuada dolor. Aliquam erat volutpat. Donec nec semper erat. Suspendisse eget viverra nibh. Fusce vel mi ut sapien placerat tincidunt. Praesent cursus tristique magna sed posuere. Sed nunc elit, interdum non quam eu, gravida aliquam ex. Proin nec bibendum ligula. Nam tellus neque, pretium nec maximus ac, eleifend eu metus. Donec nec iaculis eros. Duis ante tellus, ullamcorper quis eros eget, maximus condimentum massa. Cras at pharetra tortor. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Nullam dapibus erat. In et tempor tortor, et egestas velit. Aenean gravida maximus tellus vitae iaculis. Quisque vitae malesuada nisi, ac fermentum nunc. Praesent quis justo lectus. Suspendisse vulputate leo at urna iaculis ultrices. Pellentesque at condimentum velit. Vestibulum non tortor sed velit laoreet pretium quis a massa. Morbi ac tortor purus. In eget posuere massa, id eleifend sapien. In ullamcorper bibendum lobortis. Phasellus porta laoreet diam nec commodo. Praesent nunc dui, porttitor lacinia aliquam vel, lacinia vitae nisi. Sed mattis magna et nibh lacinia, vitae euismod sapien varius. Suspendisse ornare, urna a tincidunt scelerisque, urna diam sagittis mauris, ut elementum diam eros at neque. Aliquam vitae lacus condimentum, lacinia nisl vel, viverra libero. Aliquam mi risus, sollicitudin vitae imperdiet nec, ultrices at dui. Donec luctus leo ut purus tincidunt dignissim. Nunc commodo mi eu leo facilisis egestas. Donec gravida nunc et mi iaculis tempus. Aenean vulputate eleifend nunc, vel luctus risus rutrum vitae. Vestibulum ullamcorper gravida urna quis vehicula. Nulla tempus mi sed dolor blandit porttitor. Aenean nec ipsum maximus, ornare felis vitae, tempor elit. Mauris vel ante blandit, molestie sapien et, facilisis est. Donec luctus mauris eget pretium tincidunt. Duis justo magna, hendrerit in sagittis vel, pulvinar eget elit. Donec pulvinar ultrices iaculis. Fusce porttitor felis eu ex volutpat auctor. Phasellus at eros sit amet velit rhoncus aliquet vel vel lorem. Quisque eget sapien rhoncus, vulputate felis vel, varius nunc. Proin lobortis nisi et posuere suscipit. Fusce et laoreet massa. Cras at efficitur tellus. Etiam sed turpis vel quam lacinia vulputate vitae vel urna. Aliquam sed velit at ligula egestas placerat vitae vitae ex. Proin sed imperdiet nunc, sit amet volutpat eros. Aliquam lobortis porta odio, id volutpat massa laoreet nec. Nunc facilisis ante sem, in blandit purus pharetra id. Praesent pharetra urna suscipit, ultricies lorem eu, tempor libero. Proin at fringilla orci. Nam maximus convallis justo, in congue odio gravida sed. Integer bibendum, purus congue bibendum dignissim, ex leo mattis massa, vitae euismod diam tortor id tellus. Morbi sed rutrum urna. Duis consequat, dui quis congue vehicula, dui urna viverra ipsum, a feugiat nulla risus nec risus. ";
    int N = 6;
    for(int i = 0; i < N; i++) {
        sfs_fwrite(f, my_data, sizeof(my_data));
    }
    sfs_fseek(f, 0);

    char out_data[(N * strlen(my_data)) + 1];
    sfs_fread(f, out_data, (N * sizeof(my_data))+1);
    printf("%s\n", out_data);
    sfs_fclose(f);
    //sfs_remove("some_name.txt");
}
