# User Guide for Assignment 1 - Simple Shell Program
Hi! Welcome to my simple shell design! This program is intended to run in a linux shell. It has several capabilities, which are listed below. You will also find the limitations of the program listed in this documentation. Thank you! -- Catherine Caron (ID 260762540)

## Acceptable Commands
### EXECVP COMMANDS
The heart of the program is the execvp function. This is run in a child process. You can therefore use any commands listed in [execvp's documentation](https://www.qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/e/execvp.html).

### BUILT IN COMMANDS 
There are several built in commands that the simple shell accepts:
- **cd** Changes the directory. If no arguments are passed, it will display the contents of the current directory.
- **exit** Quits the shell. You can also use CTL+D to quit the shell program, or use CTL+C when the shell process is in the foreground.
- **jobs** Lists all jobs running in the background. The output is in a table format with the job number and job PID listed. If the table is empty, then only the titles are visibile in the output. 
- **fg** Bring specified job to foreground. Use the job number to select the job to bring to the foreground.
- **pwd** Display the current working directory. 

### OUTPUT REDIRECTION
You can use the `>` character to indicate output redirection. Only .txt files are supported. Output redirection and piping are not supported together. 

### PIPING
You can use the `|` character to pipe commands. Output redirection and piping are not supported together. You cannot pipe built in commands.

### BACKGROUND JOBS
You can use the `&` character to make the command run in the background. If the background job generates an output to the shell, ignore it and type your next command directly into the shell. The shell will only take your command as input and not the generated output from the background job. 

### CTL + D
This will quit the shell. 

### CTL + C
This will quit the foreground job. If the shell is the foreground job, this will quit the shell. 

### CTL + Z
This will do nothing. The shell will return invalid command error and request a new input. 



