#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_PENDING 10
#define MAX_LINE 20

int send_message(int s, char *message, size_t size);
int receive_message(int s, char *message, size_t size);

int main(int argc, char **argv)
{
    // check if the number of arguments is valid
    if (argc != 2)
    {
        perror("ERROR: wrong argument numbers");
        exit(EXIT_FAILURE);
    }
    // check if the port number is valid
    int addr = inet_addr("127.0.0.1");
    int port = atoi(argv[1]);
    if (port < 1024 || port > 49151)
    {
        perror("ERROR: port number must be between 1024 and 49151");
        exit(EXIT_FAILURE);
    }
    // file descriptor for the server
    int s;
    // create a socket
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERROR: socket failed");
        exit(EXIT_FAILURE);
    }
    // configure the server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = addr;
    server_addr.sin_port = htons(port);

    // set all bits of the padding field to 0
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // bind the socket to the address
    if (bind(s, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        perror("ERROR: bind failed");
        exit(EXIT_FAILURE);
    }
    // listen for incoming connections
    if (listen(s, MAX_PENDING) < 0)
    {
        perror("ERROR: listen failed");
        exit(EXIT_FAILURE);
    };
    // new socket for incoming connections
    int new_s;
    socklen_t len = sizeof(server_addr);

    char buf[MAX_LINE];
    // busy waiting for incoming connections
    while (1)
    {
        if ((new_s = accept(s, (struct sockaddr *)&server_addr, &len)) < 0)
        {
            perror("ERROR: accept failed");
            // exit(EXIT_FAILURE);
        }
        // receive the message
        if ((receive_message(new_s, buf, sizeof(buf))) < 0)
        {
            perror("ERROR: receive failed");
            // exit(EXIT_FAILURE);
        }
        // print the message
        // if (fputs(buf, stdout) == EOF)
        // {
        //     perror("ERROR: fputs failed");
        //     exit(EXIT_FAILURE);
        // };
        fputs(buf, stdout);
        fflush(stdout);
        fputs("\n", stdout);
        fflush(stdout);

        // printf("%s\n", buf);

        // parse the message
        int sequence_number = atoi(buf + 6) + 1;
        // reset the buffer
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "HELLO %d", sequence_number);

        // send the response
        if ((send_message(new_s, buf, strlen(buf) + 1)) < 0)
        {
            perror("ERROR: send failed");
            // exit(EXIT_FAILURE);
        };
        // reset the buffer
        memset(buf, 0, sizeof(buf));
        // receive the response
        if ((receive_message(new_s, buf, sizeof(buf))) < 0)
        {
            perror("ERROR: receive failed");
            // exit(EXIT_FAILURE);
        }

        int next_sequence_number = atoi(buf + 6);
        if (next_sequence_number != sequence_number + 1)
        {
            close(new_s);
            perror("ERROR: sequence number is not correct");
        }
        // print the response
        // printf("%s\n", buf);
        // fflush(stdout);
        // if (fputs(buf, stdout) == EOF)
        // {
        //     perror("ERROR: fputs failed");
        //     exit(EXIT_FAILURE);
        // };
        fputs(buf, stdout);
        fflush(stdout);
        fputs("\n", stdout);
        fflush(stdout);

        memset(buf, 0, sizeof(buf));
        // close the socket
        if (close(new_s) < 0)
        {
            perror("ERROR: close failed");
            // exit(EXIT_FAILURE);
        }
        memset(buf, 0, sizeof(buf));
    }

    return 0;
}

int receive_message(int s, char *message, size_t size)
{
    // receive the message
    int bytes_received = recv(s, message, size, 0);
    if (bytes_received < 0)
    {
        return -1;
    }
    // null terminate the message
    message[bytes_received] = '\0';
    return 0;
}

int send_message(int s, char *message, size_t size)
{
    message[MAX_LINE - 1] = '\0';
    int bytes_sent = send(s, message, size, 0);
    if (bytes_sent < 0)
    {
        return -1;
    }
    return 0;
}