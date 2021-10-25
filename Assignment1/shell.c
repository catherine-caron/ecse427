/* Simple Shell Program by Catherine Caron (ID 260762540) */

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
    if (*fg_pid == processes[0]) { /* foreground process is the shell */
        printf("\nGoodbye!\n");
        exit(0);
    }
    else { /* foreground process is a child */
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

void handle_sigusr1(int signal) { /* CTL + D */
    printf("\nGoodbye!\n"); 
    exit(0);
}

void handle_sigchld(int signal) { /* Child terminated */
    /* get pid of dead child */
    pid_t dead_pid = waitpid(-1, NULL, WNOHANG);
    
    for ( int j = 1; j < process_count; j++){       /* find child that died, never parent */
        if (processes[j] = dead_pid){
            for (int i = j; i < process_count; i++){
                processes[i] = processes[i + 1];    /* remove pid from list */
            }
            process_count--;                    /* decrease size of list */
            fg_pid = &(processes[0]);           /* default the foreground job to the shell */
            fg_index = 0;                       /* default the index to point to the shell  */
        }
    }
    return; 
}


/* Parses the command into the args array. Returns the number of entries in the array */
int getcmd(char *prompt, char *args[], int *background){

    int length, flag, i = 0, k = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);


    if (length == 0) { /* invalid command */
        return 0; 
    }
    else if (length == -1){ /* CTL + D */
        exit(0);  
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
    char* args[LENGTH];      /* list of arguments of command */
    char** args2;            /* pointer to second command for piping */
    pid_t child_pid1;        /* first child pid */
    pid_t child_pid2;        /* second child pid (for piping) */
    char* output_filename;   /* name of file to redirect output to */
    int output; /* flag for output redirection */
    int piping; /* flag for piping */
    int fd[2];  /* the pipe */
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

    if (signal(SIGUSR1, handle_sigusr1) == SIG_ERR){ /* CTL + D entered */
        printf("*** ERROR: could not bind signal handler for SIGUSR1\n");
        exit(1);
    }

    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR){ /* child terminated */
        printf("*** ERROR: could not bind signal handler for SIGCHLD\n");
        exit(1);
    }
    
    parent_pid = getpid(); /* parent pid */
    processes[0] = parent_pid; /* processes[0] will always be the parent */
    process_count = 1; /* size of list is now 1 */

    while(1){
        bg = 0;     /* reset flags */
        output = 0;
        piping = 0;

        printf("Please enter a command: ");     /*   display a prompt  */
        if ( (k = getcmd("\n>> ", args, &bg)) == 0) { /* parse input and fill args */
            printf("*** ERROR: Invalid command\n");
            continue; /* give the user another try! */
        }

        /* built in commands */
        if (strcmp(args[0], "exit") == 0){  /* exit the shell */
            printf("\nGoodbye!\n");
            exit(0); 
        }
        /* change directory*/ 
        else if (strcmp(args[0], "cd") == 0){   
            if ( args[1] == NULL) { /* no args defaults to ls */
                bzero(args, LENGTH); /* empty array */
                args[0] = "ls";
                if (execvp(args[0], args) < 0) {     /* execute ls  */
                    printf("*** ERROR: cd failed\n");
                }
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
            if ((args[1] == NULL) || atoi(args[1]) > process_count || atoi(args[1]) == 0) {
                printf("*** ERROR: job not found\n");
            }
            else if ( processes[ atoi(args[1]) ] != 0){
                fg_index = atoi(args[1]);  /* point fg to process to bring to foreground */
                fg_pid = &(processes[fg_index]); 
                printf("Job %d with PID %d now running in foreground\n", (fg_index), *fg_pid);
            }
            else {
                printf("*** ERROR: job not found\n");
            }
        }
        /* display all backgroung jobs and their job number */
        else if (strcmp(args[0], "jobs") == 0){ 
            printf("Job Number  |   Job PID\n"); /* titles */ 
            for (int i = 1; i < process_count; i++){
                if (processes[i] == 0) {
                    if (fg_pid == &processes[i]){        /* if this was in the foreground */
                        fg_pid = &(processes[0]);       /* default the foreground job to the shell */
                        fg_index = 0;                   /* default the index to point to the shell  */
                    }
                    processes[i] = processes[i + 1];    /* remove useless pid from list */
                    process_count--;                    /* decrease size of list */
                }
                else if (fg_pid != &processes[i]){
                    printf("[%d]             %d\n", i, processes[i]); /* prints as a table */
                }
            }
        }

        /* use execvp in a child process */ 
        else { 

            /* piping */ 
            for (int i = 0; i < k; i++){
                if (strcmp(args[i], "|") == 0) { 
                    args2 = (args + i + 1); /* point to next item in list */
                    args[i] = NULL; /* indicates end of first command in piping */
                    piping = 1; /* set piping flag to true */
                }
            }

            /* redirected output */
            if ((strstr(args[k - 1], ".txt") != NULL) && ((strcmp(args[k - 2], ">") == 0))) { /* check if > and .txt specified */
                if (strcmp(args[k - 1], ".txt") == 0){  /* file name is empty */
                    printf("*** ERROR: invalid text file name\n");
                    exit(1);
                }
                output_filename = args[k - 1];  /* set output file name to given name */
                output = 1;  /* set output redirection flag to true */
                args[k - 1] = NULL;  /* clear unnecessary args */
                args[k - 2] = NULL;
            }
            
            /* fork child processes with piping */
            /* piping and output redirection at the same time is not supported */
            if (piping == 1){
                if (pipe(fd) < 0){
                    printf("*** ERROR: creating pipe failed\n");
                    exit(1);
                }

                if ( (child_pid1 = fork()) < 0) {  
                    printf("*** ERROR: forking child process failed\n");
                    exit(1);
                }
                /* first child */
                if (child_pid1 == 0) {      
                    dup2(fd[1], STDOUT_FILENO); /* set up pipe */
                    close(fd[0]);   /* close unneeded ends */
                    close(fd[1]);
                    if (execvp(args[0], args) < 0) {     /* execute the first command  */
                        printf("*** ERROR: exec failed\n");
                        exit(1); 
                    }
                }

                if ( (child_pid2 = fork()) < 0) {  
                    printf("*** ERROR: forking child process failed\n");
                    exit(1);
                }
                /* second child */
                if (child_pid2 == 0) {      
                    dup2(fd[0], STDIN_FILENO); /* set up pipe */
                    close(fd[0]);   /* close unneeded ends */
                    close(fd[1]);
                    if (execvp(*args2, args2) < 0) {     /* execute the second command  */
                        printf("*** ERROR: exec failed\n");
                        exit(1); 
                    }
                }
                /* for the parent */
                processes[process_count] = child_pid1; /* add first child to list of pids */
                processes[process_count + 1] = child_pid2; /* add second child to list of pids */
                process_count+= 2; /* increase list size */

                close(fd[0]); /* close unneeded ends */
                close(fd[1]);
                
                if (bg == 0){               /* check if & at end of command */
                    fg_pid = &(processes[process_count - 2]);   /* make first child the foreground task */
                    fg_index = process_count - 2;
                    waitpid(child_pid1, NULL, 0);     /* wait for child 1 */
                    waitpid(child_pid2, NULL, 0);     /* wait for child 2 */
                } 
                /* else don't wait for any child */   
                
            }

            /* fork a child process without piping */
            else {
                if ( (child_pid1 = fork()) < 0) {  
                    printf("*** ERROR: forking child process failed\n");
                    exit(1);
                }

                /* for the child process: */
                if (child_pid1 == 0) {  
                    
                    if (output == 1) {     /* perform output redirection */
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
                else if (child_pid1 == -1){      
                    printf("*** ERROR: fork failed\n");
                    exit(1);
                }
                /* for the parent: */
                else {       
                    processes[process_count] = child_pid1; /* add to list of pids */
                    process_count++; /* increase list size */
                    
                    if (bg == 0){               /* check if & at end of command */
                        fg_pid = &(processes[process_count-1]);   /* make this the foreground task */
                        fg_index = process_count - 1;             /* set fg index to child */
                        while (wait(&status) != child_pid1);      /* wait for child */
                    } 
                    /* else don't wait for child */   
                }
            }
        }
    }
}