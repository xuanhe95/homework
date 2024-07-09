#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_LINE 20

int send_message(int s, char *message, size_t size);
int receive_message(int s, char *message, size_t size);

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        perror("invalid: wrong argument numbers");
        exit(EXIT_FAILURE);
    }
    in_addr_t host_addr;
    int port;
    int sequence_number;
    // check if the ip address is valid
    if ((host_addr = inet_addr(argv[1])) < 0)
    {
        perror("invalid: invalid ip address");
        exit(EXIT_FAILURE);
    }
    // check if the port number is valid
    port = atoi(argv[2]);
    if (port < 1024 || port > 49151)
    {
        perror("invalid: port number must be between 1024 and 49151");
        exit(EXIT_FAILURE);
    }
    // check if the sequence number is valid
    if ((sequence_number = atoi(argv[3])) < 0)
    {
        perror("invalid: invalid sequence number");
        exit(EXIT_FAILURE);
    }
    int s;
    // create a socket
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("invalid: socket failed");
        exit(EXIT_FAILURE);
    }
    // configure the server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = host_addr;
    server_addr.sin_port = htons(port);
    // set all bits of the padding field to 0
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // struct honstent *server_addr = gethostbyname(ip);

    if (connect(s, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0)
    {
        perror("ERROR: connect failed");
        exit(EXIT_FAILURE);
    }

    char message[MAX_LINE];

    // send the first message
    snprintf(message, sizeof(message), "HELLO %d", sequence_number);
    if ((send_message(s, message, strlen(message) + 1)) < 0)
    {
        perror("ERROR: send failed");
        exit(EXIT_FAILURE);
    };
    // reset the buffer
    memset(message, 0, sizeof(message));
    // receive the response
    if (receive_message(s, message, sizeof(message)) < 0)
    {
        perror("ERROR: receive failed");
        exit(EXIT_FAILURE);
    }
    // print the response
    // if (fputs(message, stdout) == EOF)
    // {
    //     perror("invalid: fputs failed");
    //     exit(EXIT_FAILURE);
    // }

    fputs(message, stdout);
    fflush(stdout);
    fputs("\n", stdout);
    fflush(stdout);

    // send the second message

    int next_sequence_number = atoi(message + 6);
    if (next_sequence_number != sequence_number + 1)
    {
        close(s);
        perror("ERROR: sequence number is not correct");
        printf("sequence_number: %d\n", next_sequence_number);
        exit(EXIT_FAILURE);
    }
    // reset the buffer
    memset(message, 0, sizeof(message));

    next_sequence_number++;

    snprintf(message, sizeof(message), "HELLO %d", next_sequence_number);

    if (send_message(s, message, strlen(message) + 1) < 0)
    {
        perror("ERROR: send failed");
        exit(EXIT_FAILURE);
    }

    // close the socket
    if (close(s) < 0)
    {
        perror("ERROR: close failed");
        exit(EXIT_FAILURE);
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