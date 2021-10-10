#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <signal.h>
#include <linux/limits.h>

#define LENGTH 20           /* max number of args in command */
#define MAX_PROCESS 50      /* max number of processes running at a time */

pid_t parent_pid;
pid_t child_pid;


void handle_sigint(int signal) { /* CTL + C */ // needs to kill foreground process instead of child_pid
    if (getpid() == child_pid) {
        kill(child_pid, SIGKILL);  /* kill child running */
        printf("\nKilled child process %d \n", child_pid); 
    }
    return;
}

void handle_sigstop(int signal) { /* CTL + Z */
    return; /* do nothing, continue on */
}

/* Parses the command into the args array. Returns the number of entries in the array */
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

    /* check if background is specified */
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else {
        *background = 0;
    }
    
    bzero(args, LENGTH); /* fill array with /0 */

    while ((token = strsep(&line, " \t\n")) != NULL) { /* remove space, tab, null */
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { 
                token[j] = '\0'; 
            } 
            if (strlen(token) > 0) { /* if token not \0 */
                i++; /* count non null char */
            }
        }
        args[k++] = token; /* add arg to array (except &) */
    }

    return k; /* number of args excluding any & */

}


int main(void) { 
    char* args[LENGTH]; /* list of arguments of command */
    char* processes[MAX_PROCESS]; /* list of child pids */
    int bg;     /* flag for & */
    int status; /* status of child process */
    
    if (signal(SIGINT, handle_sigint) == SIG_ERR){ /* CTL + C entered */
        printf("*** ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }
    
    if (signal(SIGTSTP, handle_sigstop) == SIG_ERR){ /* CTL + Z entered */
        printf("*** ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }
    
    parent_pid = getpid(); /* parent pid */

    while(1){
        bg = 0; 
        
        printf("Please enter a command: ");     /*   display a prompt  */
        getcmd("\n>> ", args, &bg);     /* parse input and fill args */

        /* built in commands */
        if (strcmp(args[0], "exit") == 0){  /* exit the shell */
            exit(0); 
        }
        else if (strcmp(args[0], "cd") == 0){   /* change directory*/ 
            if ( args[1] == NULL) { /* no args defaults to ls */
                bzero(args, LENGTH); /* empty array */
                args[0] = "ls";
                execvp(args[0], args);
            }
            else if (chdir(args[1]) < 0 ) { /* change directory */
                printf("*** ERROR: cd failed\n");
            }
            
        }
        else if (strcmp(args[0], "pwd") == 0){  /* display current working directory */
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("Current working dir: %s\n", cwd);
            } else {
                printf("*** ERROR: pwd failed\n");
            }
        }
        else if (strcmp(args[0], "fg") == 0){


        }
        else if (strcmp(args[0], "jobs") == 0){
            
        }

        else { /* use execvp in a child process */ 
            if ( (child_pid = fork()) < 0) {     /* fork a child process  */
                printf("*** ERROR: forking child process failed\n");
                exit(1);
            }
            if (child_pid == 0) {          /* for the child process: */
                // kill(child_pid, SIGINT);

                if (execvp(args[0], args) < 0) {     /* execute the command  */
                    printf("*** ERROR: exec failed\n");
                    exit(1); 
                }
                exit(0); /* child successfully completes */
            }
            else if (child_pid == -1){      /* on error fork() returns -1 */
                printf("*** ERROR: fork failed\n");
                exit(1);
            }
            else {       /* for the parent: */

                if (bg == 0){               /* check if & at end of command */
                    while (wait(&status) != child_pid);    /* wait for child */
                } 
                /* else don't wait for child */     
            }
        }
    }
}