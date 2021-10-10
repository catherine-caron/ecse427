#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <signal.h>
#include <linux/limits.h>
#include <sys/wait.h>

#define LENGTH 20           /* max number of args in command */
#define MAX_PROCESS 50      /* max number of processes running at a time */

pid_t parent_pid;               /* shell process pid */
pid_t processes[MAX_PROCESS];   /* list of child pids */
pid_t* fg_pid;                  /* points to pid in processes array of child that is running in the foreground */
int fg_index;                   /* index of the foreground child */
int process_count;              /* how many processes are running. Max 50 */


void handle_sigint(int signal) { /* CTL + C */ 
    if (getpid() == *fg_pid) {                   /* kill foreground child */
        kill(*fg_pid, SIGKILL);  
        printf("\nKilled child process %d\n", *fg_pid); 

        for (int i = fg_index; i < process_count; i++){
            processes[i] = processes[i + 1];    /* remove pid from list */
        }

        process_count--;                    /* decrease size of list */
        fg_pid = &(processes[0]);           /* default the foreground job to the shell */
        fg_index = 0;                       /* default the index to point to the shell  */

    }
    return;
}

void handle_sigstop(int signal) { /* CTL + Z */
    return; /* do nothing, continue on */
}

void handle_sigchld(int signal) { /* Child terminated */
    /* get pid of dead child */
    pid_t dead_pid = waitpid(-1, NULL, WNOHANG);
    // printf("*** ERROR: could not find child with pid %d\n", dead_pid);
    
    for ( int j = 0; j < process_count; j++){       /* find child that died */
        if (processes[j] = dead_pid){
            for (int i = j; i < process_count; i++){
                processes[i] = processes[i + 1];    /* remove pid from list */
            }
            process_count--;                    /* decrease size of list */
            fg_pid = &(processes[0]);              /* default the foreground job to the shell */
            fg_index = 0;                       /* default the index to point to the shell  */
        }
    }
    
    return; 
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
    
    bzero(args, LENGTH); /* fill array with \0 */

    while ((token = strsep(&line, " \t\0\n")) != NULL) { /* remove space, tab, new line, null */
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { 
                token[j] = '\0'; 
            } 
            if (strlen(token) > 0) { /* if token not \0 */
                i++; /* count non null char */
            }
        }
        if (strcmp(token, "") != 0 ) {  /* only add non empty strings */
            args[k++] = token; /* add arg to array (except &) */
        }
    }
    return k; /* number of args excluding any & */
}


int main(void) { 
    char* args[LENGTH];     /* list of arguments of command */
    pid_t child_pid;        /* current child pid */
    char* output_filename;  /* name of file to redirect output to */
    int output = 0; /* flag for output redirection */
    int bg;     /* flag for & */
    int status; /* status of child process */
    int k;      /* number of non zero args excluding & */
    
    if (signal(SIGINT, handle_sigint) == SIG_ERR){ /* CTL + C entered */
        printf("*** ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }
    
    if (signal(SIGTSTP, handle_sigstop) == SIG_ERR){ /* CTL + Z entered */
        printf("*** ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }

    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR){ /* child terminated */
        printf("*** ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }
    
    parent_pid = getpid(); /* parent pid */
    printf("parent process is %d\n", parent_pid);
    processes[0] = parent_pid; /* processes[0] will always be the parent */
    process_count = 1; /* size of list is now 1 */

    while(1){
        bg = 0; 
        
        printf("Please enter a command: ");     /*   display a prompt  */
        k = getcmd("\n>> ", args, &bg);     /* parse input and fill args */

        /* built in commands */
        if (strcmp(args[0], "exit") == 0){  /* exit the shell */
            exit(0); 
        }
        /* change directory*/ 
        else if (strcmp(args[0], "cd") == 0){   
            if ( args[1] == NULL) { /* no args defaults to ls */
                bzero(args, LENGTH); /* empty array */
                args[0] = "ls";
                execvp(args[0], args);
            }
            else if (chdir(args[1]) < 0 ) { /* change directory */
                printf("*** ERROR: cd failed\n");
            }
        }
        /* display current working directory */
        else if (strcmp(args[0], "pwd") == 0){  
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("Current working dir: %s\n", cwd);
            } else {
                printf("*** ERROR: pwd failed\n");
            }
        }
        /* bring indexed job to foreground */
        else if (strcmp(args[0], "fg") == 0){  
            if ((args[1] == "\0") || atoi(args[1]) > process_count || atoi(args[1]) == 0) {
                printf("*** ERROR: job not found\n");
            }
            else {
                fg_index = atoi(args[1]) - 1;  /* point fg to process to bring to foreground */
                fg_pid = &(processes[fg_index]); 
                printf("Job %d with PID %d now running in foreground\n", (fg_index+1), *fg_pid);
            }

        }
        /* display all running jobs and their job number */
        else if (strcmp(args[0], "jobs") == 0){ 
            printf("Job Number  |   Job PID\n"); /* titles */ 
            for (int i = 0; i < process_count; i++){
                printf("%d               %d\n", i+1, processes[i]); /* prints as a table */
            }
        }
        
        /* redirected output */
        else if ((strstr(args[k - 1], ".txt") != NULL) && ((strcmp(args[k - 2], ">") == 0))) {
            output_filename = args[k - 1];  /* set output file name to given name */
            output = 1;  /* set output redirection flag to true */
            args[k -1] = "\0";  /* clear unnecessary args */
            args[k - 2] = "\0";
        }

        /* use execvp in a child process */ 
        else { 
            if ( (child_pid = fork()) < 0) {  /* fork a child process  */
                printf("*** ERROR: forking child process failed\n");
                exit(1);
            }
            /* for the child process: */
            if (child_pid == 0) {  
                
                if(output){     /* perform output redirection */
                    printf("Redirecting output to: %s\n", output_filename);
                    freopen(output_filename, "w+", stdout);
                }

                if (execvp(args[0], args) < 0) {     /* execute the command  */
                    printf("*** ERROR: exec failed\n");
                    exit(1); 
                }
                exit(0); /* child successfully completes */
            }
            /* on error fork() returns -1 */
            else if (child_pid == -1){      
                printf("*** ERROR: fork failed\n");
                exit(1);
            }
            /* for the parent: */
            else {       
                processes[process_count] = child_pid; /* add to list of pids */
                process_count++; /* increase list size */
                
                if (bg == 0){               /* check if & at end of command */
                    fg_pid = &(processes[process_count-1]);   /* make this the foreground task */
                    while (wait(&status) != child_pid);     /* wait for child */
                } 
                /* else don't wait for child */   
            }
        }
    }
}