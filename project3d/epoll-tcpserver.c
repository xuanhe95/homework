#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_PENDING 10
#define MAX_LINE 20
#define MAX_THREADS 100

#define CLOSED 0
#define SYN_SENT 1
#define ESTABLISHED 2

struct client_state
{
    int socket;
    int phase;
    int sequence_number;
    char *buf;
};

int bind_and_listen(struct sockaddr_in server_addr);
struct sockaddr_in configure_server_address(int addr, int port);

void handle_first_shake(struct client_state *client);
void handle_second_shake(struct client_state *client);
void print_buf(char *buf);

int main(int argc, char **argv)
{
    // check if the number of arguments is valid
    if (argc != 2)
    {
        perror("ERROR: wrong argument numbers");
        exit(EXIT_FAILURE);
    }

    int addr = inet_addr("127.0.0.1");
    // convert the input to an integer
    int port = atoi(argv[1]);
    // check if the port number is valid
    if (port < 1024 || port > 49151)
    {
        perror("ERROR: port number must be between 1024 and 49151");
        exit(EXIT_FAILURE);
    }
    // convert the port number to network byte order
    port = htons(port);
    // configure the server address
    struct sockaddr_in server_addr = configure_server_address(addr, port);
    // bind the socket and listen for incoming connections
    int listener_fd = bind_and_listen(server_addr);

    struct client_state client_states[MAX_THREADS];

    struct epoll_event ev, events[MAX_THREADS];
    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        perror("ERROR: epoll_create1 failed");
        exit(EXIT_FAILURE);
    }
    // edge-triggered
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listener_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener_fd, &ev) == -1)
    {
        perror("ERROR: epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    // set the client_states array
    for (int i = 0; i < MAX_THREADS; i++)
    {
        client_states[i].socket = -1;
        client_states[i].phase = CLOSED;
        client_states[i].buf = NULL;
    }

    // round-robin
    while (1)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_THREADS, 0);

        if (nfds == -1)
        {
            perror("ERROR: epoll_wait failed");
            continue;
        }

        for (int n = 0; n < nfds; n++)
        {
            if (events[n].data.fd == listener_fd)
            {
                for (int i = 0; i < MAX_THREADS; i++)
                {
                    if (client_states[i].socket < 0)
                    {
                        socklen_t len = sizeof(server_addr);
                        client_states[i].socket = accept(listener_fd, (struct sockaddr *)&server_addr, &len);

                        if (client_states[i].socket < 0)
                        {
                            perror("ERROR: accept failed");
                        }

                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = client_states[i].socket;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_states[i].socket, &ev) == -1)
                        {
                            perror("ERROR: epoll_ctl failed");
                            exit(EXIT_FAILURE);
                        }

                        fcntl(client_states[i].socket, F_SETFL, O_NONBLOCK);
                        client_states[i].phase = SYN_SENT;
                        client_states[i].buf = (char *)malloc(MAX_LINE);

                        if (client_states[i].buf == NULL)
                        {
                            perror("ERROR: malloc failed");
                            exit(EXIT_FAILURE);
                        }
                        break;
                    }
                }
            }
            else
            {
                for (int i = 0; i < MAX_THREADS; i++)
                {

                    if (events[n].data.fd == client_states[i].socket)
                    {
                        if (client_states[i].socket < 0)
                        {
                            continue;
                        }
                        switch (client_states[i].phase)
                        {
                        case SYN_SENT:
                            handle_first_shake(&client_states[i]);
                            break;
                        case ESTABLISHED:
                            handle_second_shake(&client_states[i]);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
    }

    // close the socket
    if (close(listener_fd) < 0)
    {
        perror("ERROR: close failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}

struct sockaddr_in configure_server_address(int addr, int port)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = addr;
    // convert the port number to network byte order
    server_addr.sin_port = port;
    return server_addr;
}

int bind_and_listen(struct sockaddr_in server_addr)
{
    // file descriptor for the server
    int s;
    // create a socket
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERROR: socket failed");
        exit(EXIT_FAILURE);
    }

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
    return s;
}

void handle_first_shake(struct client_state *client)
{
    int s = client->socket;
    char *buf = client->buf;

    // receive the message
    int bytes_received = recv(s, buf, MAX_LINE, 0);
    if (bytes_received < 0)
    {
        perror("ERROR: receive failed");
    }
    // null terminate the message
    buf[bytes_received] = '\0';

    print_buf(buf);
    // ignore the "HELLO " part of the message, add 1 to the sequence number
    int sequence_number = atoi(buf + 6) + 1;
    // reset the buffer
    memset(buf, 0, MAX_LINE);

    snprintf(buf, MAX_LINE, "HELLO %d", sequence_number);
    // null terminate the message
    buf[MAX_LINE - 1] = '\0';
    // send the message
    int bytes_sent = send(s, buf, strlen(buf) + 1, 0);
    if (bytes_sent < 0)
    {
        perror("ERROR: send failed");
    }

    // reset the buffer
    memset(buf, 0, MAX_LINE);
    // update the phase and the sequence number
    client->phase = ESTABLISHED;
    client->sequence_number = sequence_number;
}

void handle_second_shake(struct client_state *client)
{
    int sequence_number = client->sequence_number;
    int s = client->socket;
    char *buf = client->buf;

    // receive the message
    int bytes_received = recv(s, buf, MAX_LINE, 0);
    if (bytes_received < 0)
    {
        perror("ERROR: receive failed");
    }
    // null terminate the message
    buf[bytes_received] = '\0';

    // check if the sequence number is correct
    int next_sequence_number = atoi(buf + 6);
    if (next_sequence_number != sequence_number + 1)
    {
        close(s);
        perror("ERROR: sequence number is not correct");
    }

    print_buf(buf);

    free(client->buf);
    client->phase = CLOSED;
    client->buf = NULL;
    if (close(s) < 0)
    {
        perror("ERROR: close failed");
    }
}

void print_buf(char *buf)
{
    fputs(buf, stdout);
    fflush(stdout);
    fputs("\n", stdout);
    fflush(stdout);
}