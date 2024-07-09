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
pid_t childPid1 = 0;
pid_t childPid2 = 0;
int test = 1;

void executeShell();

void writeToStdout(char *text);

void alarmHandler(int sig);

void sigintHandler(int sig);

char **getCommandFromInput();

void registerSignalHandlers();

void killChildProcess();

// helper function for trim the string

char *trimSpaces(char *str);

// redirections
int redirectionToFile(const char *token, int fileDescriptor);
int redirctionsSTDOUTtoFile(const char *token);
int redirectionsSTDINtoFile(const char *token);

// pipe
char **createArrayOfTokensBeforePipe(char **commandArray);
char **createArrayOfTokensAfterPipe(char **commandArray);

char **redirectionsPipeWriterProcess(char **commandArrayBeforePipe, int *fd);
char **redirectionsPipeReaderProcess(char **commandArrayAfterPipe, int *fd);

int isPipe(char **commandArray);
void processPipe(char **commandArray);
void processRedirections(char **commandArray);
void executeRedirections(char **commandArray);

int output(char *str);

int main(int argc, char **argv)
{
    registerSignalHandlers();
    while (1)
    {
        executeShell();
    }
    return 0;
}

/* Sends SIGKILL signal to a child process.
 * Error checks for kill system call failure and exits program if
 * there is an error */
void killChildProcess()
{
    if (kill(childPid, SIGKILL) == -1)
    {
        perror("Error in kill");
        exit(EXIT_FAILURE);
    }
}

/* Signal handler for SIGALRM. Catches SIGALRM signal and
 * kills the child process if it exists and is still executing.
 * It then prints out penn-shredder's catchphrase to standard output */
void alarmHandler(int sig)
{
    if (sig == SIGALRM)
    {
        if (childPid != 0)
        {
            killChildProcess();
            writeToStdout("Bwahaha ... tonight I dine on turtle soup\n");
        }
    }
}

/* Signal handler for SIGINT. Catches SIGINT signal (e.g. Ctrl + C) and
 * kills the child process if it exists and is executing. Does not
 * do anything to the parent process and its execution */
void sigintHandler(int sig)
{
    if (childPid != 0)
    {
        killChildProcess();
    }
}

/* Registers SIGALRM and SIGINT handlers with corresponding functions.
 * Error checks for signal system call failure and exits program if
 * there is an error */
void registerSignalHandlers()
{
    if (signal(SIGINT, sigintHandler) == SIG_ERR)
    {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGALRM, alarmHandler) == SIG_ERR)
    {
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
void executeShell()
{
    char **commandArray;
    char minishell[] = "penn-sh> ";
    writeToStdout(minishell);

    commandArray = getCommandFromInput();
    // check for empty command
    if (commandArray[0] != NULL)
    {
        if (isPipe(commandArray) != 0)
        {
            processPipe(commandArray);
        }
        else
        {
            processRedirections(commandArray);
        }
    }
    for (int i = 0; commandArray[i] != NULL; i++)
    {
        free(commandArray[i]);
    }
    free(commandArray);
}

void processRedirections(char **commandArray)
{
    int status;
    childPid = fork();
    if (childPid < 0)
    {
        perror("Error in creating child process");
        exit(EXIT_FAILURE);
    }
    else if (childPid == 0)
    {
        // child process
        executeRedirections(commandArray);
    }
    else
    {
        // parent process
        // wait for the child process to terminate
        do
        {
            if (wait(&status) == -1)
            {
                // check for errors in waiting for child process
                perror("Error in child process termination");
                exit(EXIT_FAILURE);
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // reset the childPid
        childPid = 0;
    }
}

void executeRedirections(char **commandArray)
{
    char **args = (char **)malloc(sizeof(char *) * INPUT_SIZE);
    int commandArrayPtr = 0;
    char *command;
    int inputRedirectionCount = 0;
    int outputRedirectionCount = 0;
    // iterate through the command array
    int argsPtr = 0;
    for (;;)
    {
        command = commandArray[commandArrayPtr++];
        if (command == NULL)
        {
            // end of the command array
            args[argsPtr] = NULL;
            break;
        }
        else if (strcmp(command, ">") == 0)
        {
            if (inputRedirectionCount > 0)
            {
                perror("Invalid: Multiple standard output redirects");
                exit(EXIT_FAILURE);
            }
            command = commandArray[commandArrayPtr++];
            // redirect standard output to file
            redirctionsSTDOUTtoFile(command);
            inputRedirectionCount++;
        }
        else if (strcmp(command, "<") == 0)
        {
            if (outputRedirectionCount > 0)
            {
                perror("Invalid: Multiple standard input redirects");
                exit(EXIT_FAILURE);
            }
            command = commandArray[commandArrayPtr++];
            // redirect standard input to file
            redirectionsSTDINtoFile(command);
            outputRedirectionCount++;
        }
        else
        {
            // add the command to the args
            args[argsPtr++] = command;
        }
    }

    if (execvp(args[0], args) == -1)
    {
        perror("Error in execvp");
        exit(EXIT_FAILURE);
    }
    // free the memory
    free(args);
}

void processPipe(char **commandArray)
{

    char **commandArray1 = createArrayOfTokensBeforePipe(commandArray);
    char **commandArray2 = createArrayOfTokensAfterPipe(commandArray);

    // for (int i = 0; commandArray1[i] != NULL; i++)
    // {
    //     printf("commandArray1[%d] = %s\n", i, commandArray1[i]);
    // }
    // for (int i = 0; commandArray2[i] != NULL; i++)
    // {
    //     printf("commandArray2[%d] = %s\n", i, commandArray2[i]);
    // }

    char **args1;
    char **args2;

    int fd[2];

    if (pipe(fd) == -1)
    {
        perror("invalid: pipe failed");
        exit(EXIT_FAILURE);
    }

    if ((childPid1 = fork()) < 0)
    {
        perror("invalid: fork failed");
        exit(EXIT_FAILURE);
    }
    else if (childPid1 == 0)
    {
        // redirect the standard input to the file
        args1 = redirectionsPipeWriterProcess(commandArray1, fd);

        // for (int i = 0; args1[i] != NULL; i++)
        // {
        //     printf("args[%d] = %s\n", i, args1[i]);
        // }
        // execute the command
        if (execvp(args1[0], args1) == -1)
        {
            perror("Error in execvp");
            exit(EXIT_FAILURE);
        }
    }

    if ((childPid2 = fork()) < 0)
    {
        perror("invalid: fork failed");
        exit(EXIT_FAILURE);
    }
    else if (childPid2 == 0)
    {
        args2 = redirectionsPipeReaderProcess(commandArray2, fd);

        // for (int i = 0; args2[i] != NULL; i++)
        // {
        //     printf("args[%d] = %s\n", i, args2[i]);
        // }
        if (execvp(args2[0], args2) == -1)
        {
            perror("Error in execvp");
            exit(EXIT_FAILURE);
        }
    }
    close(fd[0]);
    close(fd[1]);
    int status;
    waitpid(childPid1, &status, 0);
    waitpid(childPid2, &status, 0);
    childPid1 = 0;
    childPid2 = 0;

    free(commandArray1);
    free(commandArray2);
}

char **redirectionsPipeWriterProcess(char **commandArrayBeforePipe, int *fd)
{

    int inputRedirectionCount = 0;
    char **args = (char **)malloc(sizeof(char *) * INPUT_SIZE);
    char *file;
    // X -> pipe read
    close(fd[0]);
    // check memory allocation
    if (args == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }
    int inputRedirectionIndex = -1;

    for (int i = 0; commandArrayBeforePipe[i] != NULL; i++)
    {
        if (strcmp(commandArrayBeforePipe[i], ">") == 0)
        {
            perror("Invalid: Standard input redirect not allowed in pipe writer process");
            exit(EXIT_FAILURE);
        }
        else if (strcmp(commandArrayBeforePipe[i], "<") == 0)
        {
            inputRedirectionIndex = i;
            inputRedirectionCount++;
        }
    }
    if (inputRedirectionCount > 1)
    {
        perror("Invalid: Multiple standard output redirects");
        exit(EXIT_FAILURE);
    }

    if (inputRedirectionIndex != -1)
    {
        for (int i = 0; i < inputRedirectionIndex; i++)
        {
            args[i] = commandArrayBeforePipe[i];
        }
        args[inputRedirectionIndex] = NULL;
        file = commandArrayBeforePipe[inputRedirectionIndex + 1];
        // STDIN -> file
        int fileDescriptor = redirectionsSTDINtoFile(file);

        // STDOUT -> pipe write
        if (dup2(fd[1], STDOUT_FILENO) == -1)
        {
            perror("invalid: dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd[1]);
    }
    else
    {

        int i = 0;
        for (; commandArrayBeforePipe[i] != NULL; i++)
        {
            args[i] = commandArrayBeforePipe[i];
        }
        args[i] = NULL;

        if (dup2(fd[1], STDOUT_FILENO) == -1)
        {
            perror("invalid: dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd[1]);
    }

    return args;
}

char **redirectionsPipeReaderProcess(char **commandArrayAfterPipe, int *fd)
{
    int outputRedirectionCount = 0;
    char **args = (char **)malloc(sizeof(char *) * INPUT_SIZE);
    char *file_str;
    close(fd[1]);
    // check memory allocation
    if (args == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }
    int outputRedirectionIndex = -1;

    for (int i = 0; commandArrayAfterPipe[i] != NULL; i++)
    {
        if (strcmp(commandArrayAfterPipe[i], "<") == 0)
        {
            perror("Invalid: Standard output redirect not allowed in pipe reader process");
            exit(EXIT_FAILURE);
        }
        else if (strcmp(commandArrayAfterPipe[i], ">") == 0)
        {
            outputRedirectionIndex = i;
            outputRedirectionCount++;
        }
    }
    if (outputRedirectionCount > 1)
    {
        perror("Invalid: Multiple standard input redirects");
        exit(EXIT_FAILURE);
    }
    if (outputRedirectionIndex != -1)
    {
        for (int i = 0; i < outputRedirectionIndex; i++)
        {
            args[i] = commandArrayAfterPipe[i];
        }
        args[outputRedirectionIndex] = NULL;
        file_str = commandArrayAfterPipe[outputRedirectionIndex + 1];
        int file = redirctionsSTDOUTtoFile(file_str);

        // close(fd[1]);
        if (dup2(fd[0], STDIN_FILENO) == -1)
        {
            perror("invalid: dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd[0]);
    }
    else
    {
        // close(fd[1]);
        int i = 0;
        for (; commandArrayAfterPipe[i] != NULL; i++)
        {
            args[i] = commandArrayAfterPipe[i];
        }
        args[i] = NULL;

        if (dup2(fd[0], STDIN_FILENO) == -1)
        {
            perror("invalid: dup2 failed");
            exit(EXIT_FAILURE);
        }

        close(fd[0]);
    }

    return args;
}

char **createArrayOfTokensBeforePipe(char **commandArray)
{
    char **beforePipeArray = (char **)malloc(sizeof(char **) * INPUT_SIZE);
    // check memory allocation
    if (beforePipeArray == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; commandArray[i] != NULL; i++)
    {
        if (strcmp(commandArray[i], "|") == 0)
        {
            beforePipeArray[i] = NULL;
            break;
        }
        else
        {
            beforePipeArray[i] = commandArray[i];
        }
    }
    return beforePipeArray;
}

char **createArrayOfTokensAfterPipe(char **commandArray)
{
    char **afterPipeArray = (char **)malloc(sizeof(char **) * INPUT_SIZE);
    int foundPipe = 0;
    // check memory allocation
    if (afterPipeArray == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }
    int i = 0;
    int j = 0;
    for (; commandArray[i] != NULL; i++)
    {
        if (strcmp(commandArray[i], "|") == 0)
        {
            foundPipe = 1;
            continue;
        }
        if (foundPipe != 0)
        {
            afterPipeArray[j] = commandArray[i];
            j++;
        }
    }
    afterPipeArray[j] = NULL;
    return afterPipeArray;
}

int redirctionsSTDOUTtoFile(const char *token)
{
    return redirectionToFile(token, STDOUT_FILENO);
}

int redirectionsSTDINtoFile(const char *token)
{
    return redirectionToFile(token, STDIN_FILENO);
}

/* Redirect to files*/
int redirectionToFile(const char *token, int fileDescriptor)
{

    if (token == NULL || token[0] == '\0')
    {
        perror("Invalid standard input redirect: Empty file name");
        exit(EXIT_FAILURE);
    }
    // open the file
    int file;
    if (fileDescriptor == STDIN_FILENO)
    {
        file = open(token, O_RDONLY);
    }
    else if (fileDescriptor == STDOUT_FILENO)
    {
        file = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    else
    {
        perror("Invalid standard redirect: File descriptor not recognized");
        exit(EXIT_FAILURE);
    }
    if (file == -1)
    {
        if (fileDescriptor == STDIN_FILENO)
        {
            perror("Invalid standard input redirect: No such file or directory");
        }
        else if (fileDescriptor == STDOUT_FILENO)
        {
            perror("Invalid standard output redirect: No such file or directory");
        }
        else
        {
            perror("Invalid standard redirect: File descriptor not recognized");
        }

        exit(EXIT_FAILURE);
    }
    // redirect the standard output to the file
    if (dup2(file, fileDescriptor) == -1)
    {
        perror("invalid: dup2 failed");
        exit(EXIT_FAILURE);
    }
    return file;
}

/* Writes particular text to standard output */
void writeToStdout(char *text)
{
    if (write(STDOUT_FILENO, text, strlen(text)) == -1)
    {
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
char **getCommandFromInput()
{
    // declare a buffer to store the input
    char buffer[INPUT_SIZE];
    int i = 0;

    // read input from stdin
    while (i < INPUT_SIZE)
    {
        // read one character at a time
        ssize_t bytesRead = read(STDIN_FILENO, &buffer[i], sizeof(char));
        // check for errors
        if (bytesRead == -1)
        {
            perror("invalid: read character failed");
            exit(EXIT_FAILURE);
        }
        // check for EOF
        else if (bytesRead == 0)
        {
            exit(EXIT_SUCCESS);
        }
        // check for new line
        else if (buffer[i] == '\n')
        {
            break;
        }
        // update index
        else
        {
            i++;
        }
    }

    // add null terminator to the end of the buffer
    buffer[i] = '\0';

    // allocate memory for command
    char *command = (char *)malloc(sizeof(char) * (strlen(buffer) + 1));

    // check whether malloc was successful
    if (command == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }

    // copy buffer to command
    strcpy(command, buffer);

    // trim the spaces
    // command = trimSpaces(command);

    char **commandArray;
    int commandArraySize = 1024;
    int commandArrayPtr = 0;
    TOKENIZER *tokenizer;
    char *tok;

    // allocate memory for command array
    commandArray = (char **)malloc(sizeof(char **) * commandArraySize);

    if (commandArray == NULL)
    {
        perror("invalid: malloc failed");
        exit(EXIT_FAILURE);
    }

    tokenizer = init_tokenizer(command);
    while ((tok = get_next_token(tokenizer)) != NULL)
    {
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

char *trimSpaces(char *str)
{
    // index for the string
    int i = 0;
    int j = 0;
    // ignore leading spaces
    while (str[i] == ' ')
    {
        i++;
    }
    // copy the string to the beginning
    while (str[i] != ' ' && str[i] != '\0')
    {
        str[j++] = str[i++];
    }
    // ignore trailing spaces
    str[j] = '\0';

    return str;
}

int isPipe(char **commandArray)
{
    // traverse the command array and compare each character with pipe mark
    for (int i = 0; commandArray[i] != NULL; i++)
    {
        if (strcmp(commandArray[i], "|") == 0)
        {
            return 1;
        }
    }
    return 0;
}

int output(char *str)
{
    if (test != 0)
    {
        writeToStdout(str);
    }
    return 0;
}