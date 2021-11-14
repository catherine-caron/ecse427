#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


pid_t backgroundJobs[100];
pid_t foregroundJob;
pid_t pid;
pid_t pid2;


// get command Feature from the instructions. there might be bugs           
int getcmd(char *prompt, char* args[], int *background, int *isRed, int *isPipe)
{
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 1024;
    printf("%s", prompt);

    //this reads the stream of line
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }  
    if ((loc = index(line, '>')) != NULL) {
        *isRed = 1;
    }
    else *isRed = 0;

    if ((loc = index(line, '|')) != NULL) {
        *isPipe = 1;
    }
    else isPipe = 0;

    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    }
    else *background = 0;
        //this is supposed to split when they encounter a space followed by another space of a void
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
            if (strlen(token) > 0){
                args[i++] = token;
                int y = i -1;
            }
    }
    args[i] = NULL;
    return i;
}

//Command to add a job
void addJob(pid_t id, int *nextJob){
    //printf("Add a job is called");
    //printf("this is the current nextJob index: %d\n", *nextJob);
    // printf("this is the background %d\n", backgroundJobs[i]);

    backgroundJobs[*nextJob] = id; //this keeps adding to the first item
    *nextJob = 100;
    int status;
    int i;
    for(i = 0; i < 100; i++){
        if(waitpid(backgroundJobs[i], &status, WNOHANG)) 
        { 
            backgroundJobs[i] = 0; 
        } 
        if (backgroundJobs[i]== 0 ){ 
            if(i < *nextJob) *nextJob = i; //this should be on
        }
       // printf("this is the current nextJob index: %d\n", *nextJob);
    }
}

//Command to update the Job Index when adding a job
void updateJobIndex(int *nextJob){
    *nextJob = 100;
    int status;
    int i;
    for(i = 0; i < 100; i++){

        if(waitpid(backgroundJobs[i], &status, WNOHANG)) 
        {
            backgroundJobs[i] = 0; 
        } 
        if (backgroundJobs[i]== 0 ){
            if(i < *nextJob) *nextJob = i; //this should be on
        }
        //printf("this is the current nextJob index: %d\n", *nextJob);
    }
}

//function deciding if in need of redirection 
void needRedirection(char *args[], int *isRedirected, char* path, int *location){
    printf("here");
    int i;

    for(i = 0; i < 19; i++)
    {
        if(strcmp(">", args[i])==0){
            *location = i;
            args[i] = "";
            *path = *args[i+1];
            args[i+1] = "";
            *isRedirected =1 ;
            while(args[i]!=NULL){
                args[i] = "";
                i++;
            }
        }
    }   
    printf("this is the redirection bool: %d", *isRedirected );
}   

//Actually redirection 
static void redirectingOutput(char *args[]){
    close(1);
    int fileDescriptor = open(args[1],O_CREAT | O_WRONLY | O_TRUNC, 0600);
    printf("this is the file descriptor %d\n", fileDescriptor);
}

//command pwd
int pwd(){
    char pwd[1024];
    getcwd(pwd, sizeof(pwd));
    printf("\nCurrent directory is: %s\n", pwd);
    return 0;
}

//command cd
int cd(char *str){
    char s[1024];
    printf("Current directory is %s \n", getcwd(s, sizeof(s)));
    chdir(str);
    printf("The new directory is %s \n", getcwd(s, sizeof(s)));
    return 0;
}

//command exit need to kill all background processes 
int shellExit(){
    printf("The program is exiting\n");
        exit(0);
}

//command bringing a background job to the foreground
int fg(int jobIndex)
{
    if(jobIndex < 0 || jobIndex > 99) return 0;
    int status;
    int i;
    
    foregroundJob = backgroundJobs[jobIndex];
    printf("%d\n", foregroundJob);
    waitpid(foregroundJob, &status, WUNTRACED);
    backgroundJobs[jobIndex] = (pid_t) 0;
    //check if any background job has been terminated
    for(i = 0; i < 100; i++){
         if(backgroundJobs[i] != 0){
            if(waitpid(backgroundJobs[i], &status, WNOHANG)) backgroundJobs[i] = 0;
         }
    }
    return 0;
}

//command listing all the jobs
int jobs()
{
    printf("Current Background Jobs:\n");
    printf("Job \t\t\t | PID\n");
    int status;
    int i = 0;
    for(i = 0; i < 100; i++){
      //  printf("this is the background job %d: %d\n", i, backgroundJobs[i]);
        if(backgroundJobs[i] != 0){
            if(waitpid(backgroundJobs[i], &status, WNOHANG)) backgroundJobs[i] = 0;
            if(backgroundJobs[i] != 0){
                printf("%d \t\t\t | %d\n", i, backgroundJobs[i]);
            }
        }
        //printf("this is the background job %d: %d\n", i, backgroundJobs[i]);
    }
    return 0;
}

//Command checking if the built-in command exists or if it's a runnable file (forkable or not)
int RunArgs(char* cmd, char *args[])
{
    //printf("RunArgs first args: %s\n", args[0]);
    //printf("RunArgs second args: %s\n", args[1]);
    if(!strcmp(cmd,"cd")){
         if(args[1] == NULL){
            return 0;
        }
        else return cd(args[1]);  
    }
    else if (!strcmp(cmd, "exit")){
        return shellExit();    
    }
    else if(!strcmp(cmd, "pwd")){
        return pwd();
    }
    else if(!strcmp(cmd, "fg")){
        if(args[1] == NULL){
            return 0;
        }
        else return fg(atoi(args[1]));
    }
    else if(!strcmp(cmd, "jobs")){
        return jobs();
    }
    return 1; 
}

//Signal Handler for the Ctrl+C Feature
static void sigHandler(int sig){
   //this doesn't work properly on my machine
   //I kill the foregroundJob which the pid is given by the variable foregroundJob 
   //On my machine it kills all the background processes, which doesn't make sense
   //it also stops and kills the foreground process which is what we want
  
    printf("This is the foreGroundJob pid%d\n", foregroundJob); //showing that I am actually using the good pid
    printf("This is the background jobs before the KILL cmd\n");
    jobs(); // showing that the jobs are not done yet 
    if(foregroundJob != 0){
        updateJobIndex;
        kill(foregroundJob, SIGTERM);
         printf("This is the background jobs after the KILL cmd\n");
        jobs(); //showing that my command is good jobs are still logged here 
    }
    else exit(0);
}

int main(void)
{
    char *args[20]; //variable useful for the first fork
    char *args2[20]; //variable useful for the piping
    int *nextJob = malloc(sizeof(int)); //index of the next job to be added
    int *bg = malloc(sizeof(int)); //int indicating if & is present
    int *isRedirected = malloc(sizeof(int)); //int indicating if > is present
    int *isPipe = malloc(sizeof(int)); //int indicating if | is present
    int *location = malloc(sizeof(int)); //location of the | or > 
    char *path; // path to the new executable file

   //Handles Ctrl+Z
    if (signal(SIGTSTP, SIG_IGN) ==  SIG_ERR) {
         printf("Error, could not bind the signal handler #1 (SIGTSP)\n");
         exit(1);
    } 
    //Handles Ctrl+C
    if (signal(SIGINT, sigHandler) ==  SIG_ERR) {
         printf("Error, could not bind the signal handler #1 (SIGNT)\n");
         exit(1);
    }   

    while(1) {
        //reseting these variables at every itiration
        *bg = 0;
        *isRedirected = 0;
        *location = 0; 
        *isPipe = 0;
        
        //declaring counters
        int i = 0;
        int y;
        int j = 0;
        int z =1;
        int a = 1;
        int b = 0;

        //declaring the file descriptor
        int fd[2];


        //checks the input and if it is forkable;
        int cnt = getcmd("\n>> ", args, bg, isRedirected, isPipe);
        int forkable = RunArgs(args[0], args);
        
        //If pipe is needed we pipe
        if(*isPipe) {
            int temporary = pipe(fd);
            if(temporary != 0 ){ 
                printf("Couldn't create the pipe");
            }
        }

        //If pipe is needed, then we find the |
        // we then save the other cmd and save its inputs in the args2[] array
        if (*isPipe){
            while(a){
                if (args[j] == NULL) break;
                else if (b){
                    args2[j-*location-1] = args[j];
                    printf("this is the element %d of args1: %s\n", j, args[j]);
                    printf("this is the element %d of args2: %s\n", (j-*location-1), args2[j-*location-1]);
                    args[j-1] = '\0'; 
                }
                else if(!strcmp(args[j], "|")){
                    *location = j;
                    args[j] = '\0'; 
                    b =1;
                     if (args[j+1] == NULL){ 
                        printf("An argument is missing\n");
                     }
                }   
                j++;    
            }
        }

        //creating of the first child if forkable
        if(forkable) {
            pid = fork();
            foregroundJob = pid;}
        else continue;

        //If needs to be redirected we check the location of the > 
        //when found, we save the path of the new output document
        if(*isRedirected==1){
             while(z)
            { 
                if(!strcmp(args[i], ">")){
                    *location = i;
                    args[i] = '\0';
                    z = 0;      
                }   
                i++;
                if (args[i] == NULL) z =0;
                else path = args[i];
            }  
        }

        if(pid == -1){
            printf("Fork command isn't working");
        }
        //Entering the child
        else if(pid == 0 ){
            //if is redireted then we open the fd to prepare to redirect
            if (*isRedirected == 1){           
                close(1);
                int fileDescriptor = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644); 
            } 
            //if needs pipe, we start handling the STDOUT_FILENO
            else if (*isPipe){
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            //executing the first child
            int x = execvp(args[0], args);
            int status;
            if( x == -1) {
                printf("child 1 wrong termination\n"); 
                exit(-1);
            }
        }   
        else { // parent process

            if (*isPipe){ // if needs pipe, need to create a new child and handle the STDIN_FILENO
    	        pid2 = fork();
    	        if(pid2 == -1){
    	            printf("Fork command isn't working");
    	        }
    	        else if(pid2 == 0 ){ 
    	            dup2(fd[0], STDIN_FILENO); //dup STDIN_FILENO 
    	            close(fd[0]);
    	            close(fd[1]);
    	            printf("%s\t", args2[0]);
    	            printf("%s\n", args2[1]);
    	            int x = execvp(args2[0], args2);
    	            int status2;
    	            if( x == -1) {
    	                printf("child 2 wrong termination\n"); 
    	                exit(-1);
    	            }
    	        }
    	    }
    	    int statusChild = 0;
    	    int statusChild2 = 0;
	
    	    if (!*isPipe){ //if there is no pipe we handle the single child of the parent
    	        if (*bg == 0) {
    	            foregroundJob = pid;
                    printf("%d\n", foregroundJob);
    	            waitpid(foregroundJob, &statusChild, WUNTRACED);
                	updateJobIndex(nextJob);
            	}	
            	else if (*bg == 1){ 
                	updateJobIndex(nextJob);
                	addJob(pid, nextJob);
            	}	  
            }   
            else { //if was piped we need to handle both child of the parent
            	printf("here\n");
                close(fd[0]);
                close(fd[1]);
                waitpid(pid, &statusChild, WUNTRACED);
                waitpid(pid2, &statusChild2, WUNTRACED);
            }
            printf("Child status 1: %d\n", statusChild);
            printf("Child status 2: %d\n", statusChild2);
        }     
    }
}