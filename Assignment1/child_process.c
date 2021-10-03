#include <stdio.h> 
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int getcmd(char *prompt, char *args[], int *background){

    int length, i = 0, k = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }

    // check if background is specified
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else {
        *background = 0;
    }

    while ((token = strsep(&line, " \t\n")) != NULL) { // remove space, tab, null
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { // 
                token[j] = '\0'; // set to null ? 
            } 
            if (strlen(token) > 0) { // if token not \0
                i++; // count non null char
            }
        }
        args[k++] = token; // add arg to array (except &)
    }

    // return i; //number of non empty characters in command
    return k; // number of args excluding any &

}



int main(void) { 
    char *args[20];
    int bg; // bg is a flag for & 
    int status; // status is the status of the child process

    printf("value of bg is %d", bg);
    printf("running getcmd...");

    // int i = getcmd("\n>> ", args, &bg);

    printf("value of i is %d ", i);
    printf("value of args is %s + %s + %s + %s ", args[0], args[1], args[2], args[3]);
    printf("value of bg is %d ", bg);


    while(1){
        bg = 0; // reset background until its checked in getcmd
        int count = getcmd("\n>> ", args, &bg); // k = count
        // args array now contains command split by arg
        // k is length of array

        // fork child process
        pid_t pid = fork();

        if ( pid == 0 ){ // child
            // do execvp
            execvp(args[0], args);
            // may need to exit?
        }
        else if ( pid == -1){ // error
            errExit("Fork failed. \n");
        }
        else { // parent
            if (bg == 0) { // command did not end with &
                // wait for child to finish
                waitpid(pid, &status, 0);
            }
            // else & means continue loop
        }

    }
}