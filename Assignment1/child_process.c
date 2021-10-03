#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>

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
    char* args[20];
    int bg; // bg is a flag for & 
    int status; // status is the status of the child process

    int count = getcmd("\n>> ", args, &bg); // k = count
    // printf("value of bg is %d", bg);
    // printf("running getcmd...");

    // // int i = getcmd("\n>> ", args, &bg);

    // printf("value of i is %d ", i);
    printf("value of args is %s + %s ", args[0], args[1]);
    printf("value of bg is %d ", bg);


    while(1){
        bg = 0; // reset background until its checked in getcmd
        pid_t tpid;
        
        // args array now contains command split by arg
        // k is length of array

        // fork child process
        pid_t pid = fork();

        if ( pid == 0 ){ // child
            // do execvp
            execvp(args[0], args);
            /* If execvp returns, it must have failed. */
            printf("Unknown command\n");
            exit(0);
        }
        // else if ( pid == -1){ // error
        //     errExit("Fork failed. \n");
        // }
        else { // parent
            if (bg == 0) { // command did not end with &
                // wait for child to finish
                // waitpid(pid, &status, 0);
                do {
                    tpid = wait(&status);
                    if(tpid != pid) process_terminated(tpid);
                } while(tpid != pid);

                return status;
            }
        }

        // double start_time = getTime(); //save time this process starts at
        // pid_t pid_from_fork = fork();
        // int status;
        // switch(pid_from_fork) {
        //     case -1: //on error fork() returns -1
        //         errExit("Fork failed. \n");
                
        //     case 0: //for the child process:
        //         if(execvp(*args, args)<0){ //execute the command
        //             //execvp returns negative value if execution fails
        //             errExit("Error: Exec failed. \n");
        //         }
        //     default: //for the parent: (on succes, fork() returns the pid of the child to the parent)
        //         while( wait(&status) != pid_from_fork); //wait for completion
        // }
        // double end_time = getTime();
        // double delta_time = end_time - start_time;
        // printf("Time ellapsed for this process: %f ms.\n", delta_time);
        }
    }