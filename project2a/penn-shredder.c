#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "tokenizer.h"
// could I use this?
#include <fcntl.h>

#define INPUT_SIZE 1024

pid_t childPid = 0;

void executeShell();

void writeToStdout(char *text);

void alarmHandler(int sig);

void sigintHandler(int sig);

char **getCommandFromInput();

void registerSignalHandlers();

void killChildProcess();

char* trimSpaces(char *str);

// redirections
void redirectionToFile(char* token, int fileDescriptor);
void redirctionsSTDOUTtoFile(char* token);
void redirectionsSTDINtoFile(char* token);

int main(int argc, char **argv) {
    registerSignalHandlers();
    while (1) {
        executeShell();
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
    if(sig == SIGALRM){
        if(childPid != 0){
            killChildProcess();
            writeToStdout("Bwahaha ... tonight I dine on turtle soup\n");
        }
    }
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
    if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
        perror("Error in alarm signal");
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
void executeShell() {
    char **commandArray;
    int status;
    char minishell[] = "penn-shredder# ";
    writeToStdout(minishell);

    commandArray = getCommandFromInput();

    if (commandArray[0] != NULL){
        childPid = fork();
        // writeToStdout("forked\n");
        if (childPid < 0){
            perror("Error in creating child process");
            exit(EXIT_FAILURE);
        }
        else if(childPid == 0){
            // child process   
            int argsSize = INPUT_SIZE;
            char **args = (char**) malloc(sizeof(char*) * argsSize);
            int commandArrayPtr = 0;
            char *command;

            int redirectionsSTDOUT = 0;
            int redirectionsSTDIN = 0;
            // iterate through the command array
            int argsPtr = 0;
            for (;;) {
                command = commandArray[commandArrayPtr++];
                // writeToStdout(command);
                // writeToStdout("\n");
                if (command == NULL){
                    // end of the command array
                    args[argsPtr] = NULL;
                    break;
                } else if (strcmp(command, ">") == 0){
                    if(redirectionsSTDOUT > 0){
                        perror("Invalid: Multiple standard output redirects");
                        exit(EXIT_FAILURE);
                    }
                    command = commandArray[commandArrayPtr++];
                    // redirect standard output to file
                    redirctionsSTDOUTtoFile(command);
                    redirectionsSTDOUT++;
                } else if (strcmp(command, "<") == 0){
                    if(redirectionsSTDIN > 0){
                        perror("Invalid: Multiple standard input redirects");
                        exit(EXIT_FAILURE);
                    }
                    command = commandArray[commandArrayPtr++];
                    // redirect standard input to file
                    redirectionsSTDINtoFile(command);
                    redirectionsSTDIN++;
                } else{

                    // add the command to the args
                    args[argsPtr++] = command;
                }
            }

            if(execvp(args[0], args) == -1){
                perror("Error in execvp");
                exit(EXIT_FAILURE);
            }

            free(args);

        }
        else{
            // parent process
            // wait for the child process to terminate
            do {
                if (wait(&status) == -1) {
                    // check for errors in waiting for child process
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            // reset the childPid
            childPid = 0;
        }

    }
    for(int i = 0; commandArray[i]!= NULL; i++){
        free(commandArray[i]);
    }
    free(commandArray);


}

void redirctionsSTDOUTtoFile(char* token){
    redirectionToFile(token, STDOUT_FILENO);
}

void redirectionsSTDINtoFile(char* token){
    redirectionToFile(token, STDIN_FILENO);
}

/* Redirect to files*/
void redirectionToFile(char* token, int fileDescriptor){
    if(token == NULL || token[0] == '\0'){
        perror("Invalid standard input redirect: Empty file name");
        exit(EXIT_FAILURE);
    }
    // open the file
    int file;
    if (fileDescriptor == STDIN_FILENO){
        file = open(token, O_RDONLY);
    } else if(fileDescriptor == STDOUT_FILENO){
        file = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else{
        perror("Invalid standard redirect: File descriptor not recognized");
        exit(EXIT_FAILURE);
    }
    if (file == -1){
        if(fileDescriptor == STDIN_FILENO){
            perror("Invalid standard input redirect: No such file or directory");
        } else if(fileDescriptor == STDOUT_FILENO){
            perror("Invalid standard output redirect: No such file or directory");
        } else {
            perror("Invalid standard redirect: File descriptor not recognized");
        }
        exit(EXIT_FAILURE);
    }

    // redirect the standard output to the file
    if( dup2(file, fileDescriptor) == -1){
        perror("invalid: dup2 failed");
        exit(EXIT_FAILURE);
    }
}

/* Writes particular text to standard output */
void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        // check for errors in writing to standard output
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
char **getCommandFromInput() {
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
    char *command = (char*) malloc(sizeof(char) * (strlen(buffer) + 1));

    // check whether malloc was successful
    if(command == NULL){
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }

    // copy buffer to command
    strcpy(command, buffer);

    // trim the spaces
    //command = trimSpaces(command);



    char** commandArray;
    int commandArraySize = 1024;
    int commandArrayPtr = 0;
    TOKENIZER* tokenizer;
    char* tok;

    // allocate memory for command array
    commandArray = (char**) malloc(sizeof(char**) * commandArraySize);

    if(commandArray == NULL){
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }

    tokenizer = init_tokenizer( command );
    while( (tok = get_next_token( tokenizer )) != NULL){
        // commandArray = (char*) malloc(sizeof(char*) * strlen(tok) + 1);
        commandArray[commandArrayPtr++] = tok;
    }
    commandArray[commandArrayPtr] = NULL;
    free(command);
    free_tokenizer(tokenizer);
    return commandArray;
}

/*
 * Helper function for trim the string 
 * 1. ignore leading spaces
 * 2. copy the string to the beginning
 * 3. ignore trailing spaces by adding null terminator
 */

char* trimSpaces(char *str){
    // index for the string
    int i = 0;
    int j = 0;
    // ignore leading spaces
    while(str[i] == ' '){
        i++;
    }
    // copy the string to the beginning
    while(str[i] != ' ' && str[i] != '\0'){
        str[j++] = str[i++];
    }
    // ignore trailing spaces
    str[j] = '\0';
    
    return str;
}
