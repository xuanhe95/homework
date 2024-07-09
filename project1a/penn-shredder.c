#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INPUT_SIZE 1024

pid_t childPid = 0;

void executeShell(int timeout);

void writeToStdout(char *text);

void alarmHandler(int sig);

void sigintHandler(int sig);

char *getCommandFromInput();

void registerSignalHandlers();

void killChildProcess();


/*
 * Helper functions
*/
char* trimSpaces(char *str);

int main(int argc, char **argv) {
    registerSignalHandlers();

    int timeout = 0;
    if (argc == 2) {
        timeout = atoi(argv[1]);
    }

    if (timeout < 0) {
        writeToStdout("Invalid input detected. Ignoring timeout value.\n");
        timeout = 0;
    }

    while (1) {
        executeShell(timeout);
    }

    return 0;
}

/* Sends SIGKILL signal to a child process.
 * Error checks for kill system call failure and exits program if
 * there is an error */
void killChildProcess() {
    if (kill(childPid, SIGKILL) == -1) {
        perror("Error in kill");
        exit(EXIT_FAILURE);
    }
}

/* Signal handler for SIGALRM. Catches SIGALRM signal and
 * kills the child process if it exists and is still executing.
 * It then prints out penn-shredder's catchphrase to standard output */
void alarmHandler(int sig) {
}

/* Signal handler for SIGINT. Catches SIGINT signal (e.g. Ctrl + C) and
 * kills the child process if it exists and is executing. Does not
 * do anything to the parent process and its execution */
void sigintHandler(int sig) {
    if (childPid != 0) {
        killChildProcess();
    }
}


/* Registers SIGALRM and SIGINT handlers with corresponding functions.
 * Error checks for signal system call failure and exits program if
 * there is an error */
void registerSignalHandlers() {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }
}

/* Prints the shell prompt and waits for input from user.
 * Takes timeout as an argument and starts an alarm of that timeout period
 * if there is a valid command. It then creates a child process which
 * executes the command with its arguments.
 *
 * The parent process waits for the child. On unsuccessful completion,
 * it exits the shell. */
void executeShell(int timeout) {
    char *command;
    int status;
    char minishell[] = "penn-shredder# ";
    writeToStdout(minishell);

    command = getCommandFromInput();
    // check if the command is empty


    if(strcmp(command, "") == 0){
        free(command);
        return;
    }

    if (command != NULL ){
        childPid = fork();

        if (childPid < 0) {
            perror("Error in creating child process");
            exit(EXIT_FAILURE);
        }

        if (childPid == 0) {
            char *const envVariables[] = {NULL};
            char *const args[] = {command, NULL};
            if (execve(command, args, envVariables) == -1) {
                perror("Error in execve");
                exit(EXIT_FAILURE);
            }
        } else {
            do {
                if (wait(&status) == -1) {
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    free(command);
}

/* Writes particular text to standard output */
void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        perror("Error in write");
        exit(EXIT_FAILURE);
    }
}

/* Reads input from standard input till it reaches a new line character.
 * Checks if EOF (Ctrl + D) is being read and exits penn-shredder if that is the case
 * Otherwise, it checks for a valid input and adds the characters to an input buffer.
 *
 * From this input buffer, the first 1023 characters (if more than 1023) or the whole
 * buffer are assigned to command and returned. An \0 is appended to the command so
 * that it is null terminated */
char *getCommandFromInput() {
    // declare a buffer to store the input
    char buffer[INPUT_SIZE];
    int i = 0;

    // read input from stdin
    while(i < INPUT_SIZE){
        // read one character at a time
        ssize_t bytesRead = read(STDIN_FILENO, &buffer[i], sizeof(char));
        // check for errors
        if (bytesRead == -1) {
            perror("invalid: read character failed");
            exit(EXIT_FAILURE);
        }
        // check for EOF
        else if(bytesRead == 0){
            exit(EXIT_SUCCESS);
        }
        // check for new line
        else if(buffer[i] == '\n'){
            break;
        }
        // update index
        else{
            i++;
        }
    }

    // add null terminator to the end of the buffer
    buffer[i] = '\0';

    // allocate memory for command
    char *command = malloc(sizeof(char) * (strlen(buffer) + 1));

    // check whether malloc was successful
    if(command == NULL){
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }

    // copy buffer to command
    strcpy(command, buffer);

    // trim the spaces
    trimSpaces(command);
    
    return command;
}

/*
 * Helper function for trim the string */

char* trimSpaces(char *str){
    // index for the string
    int i = 0;
    int j = 0;
    // ignore leading spaces
    while(str[i] == ' '){
        i++;
    }
    
    while(str[i] != ' ' && str[i] != '\0'){
        str[j++] = str[i++];
    }

    str[j] = '\0';
    
    return str;
}
