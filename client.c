#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

char *itoa(int val)
{
    static char buf[32] = {0};
    int i = 30;
    for (; val && i; --i, val /= 10)
        buf[i] = "0123456789abcdef"[val % 10];
    return &buf[i + 1];
}

int main(int argc, char *argv[])
{
    int serverfd, broadcastsendfd, broadcastrecvfd, nbytes, port, broadcast, flg;
    struct addrinfo hints, *server;
    char msg[512], username[512], buf[512];
    char *token;
    int firsttype = 0;
    struct sockaddr_in bcsendaddr, bcrecvaddr;
    socklen_t addrlen;
    fd_set users, ready_users, bc, bc_ready;

    port = atoi(argv[1]);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, argv[1], &hints, &server) != 0)
        exit(0);

    serverfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (connect(serverfd, server->ai_addr, server->ai_addrlen) == -1)
    {
        write(STDERR, "SERVER CONNECT ERROR\n", 21);
        close(serverfd);
    }
    freeaddrinfo(server);

    broadcastsendfd = socket(AF_INET, SOCK_DGRAM, 0);
    broadcast = 1;
    if (setsockopt(broadcastsendfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
        write(STDERR, "OPT BROADCAST ERROR\n", 20);
    broadcastrecvfd = socket(AF_INET, SOCK_DGRAM, 0);

    flg = 1;
    if (setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEADDR, &flg, sizeof(flg)) < 0)
        write(STDERR, "OPT REUSEADDR ERROR\n", 20);
    flg = 1;
    if (setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEPORT, &flg, sizeof(flg)) < 0)
        write(STDERR, "OPT REUSEPORT ERROR\n", 20);

    bcsendaddr.sin_family = AF_INET;
    bcsendaddr.sin_port = htons(port);
    bcsendaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    bcrecvaddr.sin_family = AF_INET;
    bcrecvaddr.sin_port = htons(port);
    bcrecvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    FD_ZERO(&users);
    FD_ZERO(&ready_users);
    FD_ZERO(&bc);
    FD_ZERO(&bc_ready);
    FD_SET(STDIN, &users);
    FD_SET(serverfd, &users);
    FD_SET(STDIN, &bc);
    FD_SET(broadcastrecvfd, &bc);

    while (1)
    {
        if (port == atoi(argv[1]))
        {
            memset(msg, '\0', 256);
            ready_users = users;
            if (select(serverfd + 1, &ready_users, NULL, NULL, NULL) <= 0)
                break;
            if (FD_ISSET(serverfd, &ready_users))
            {
                nbytes = recv(serverfd, msg, sizeof(msg), 0);
                if (nbytes <= 0)
                {
                    write(STDERR, "SERVER\n", 7);
                    exit(0);
                }
                if (strncmp(msg, "#groupport=", 11) == 0)
                {
                    write(STDOUT, "YOU JOINED THE GROUP! (Type $exit_group$ To Quit)\n", 50);
                    port = atoi(strtok(msg, "#groupport="));
                    bcsendaddr.sin_port = htons(port);
                    bcrecvaddr.sin_port = htons(port);
                    if (bind(broadcastrecvfd, (struct sockaddr *)&bcrecvaddr, sizeof(bcrecvaddr)) < 0)
                        write(STDERR, "BIND ERROR\n", 11);
                }
                else if (strncmp(msg, "#newgroupport=", 14) == 0)
                {
                    write(STDOUT, "YOU CREATED THE GROUP! (Type $exit_group$ To Quit)\n", 51);
                    port = atoi(strtok(msg, "#newgroupport="));
                    bcsendaddr.sin_port = htons(port);
                    bcrecvaddr.sin_port = htons(port);
                    if (bind(broadcastrecvfd, (struct sockaddr *)&bcrecvaddr, sizeof(bcrecvaddr)) < 0)
                        write(STDERR, "BIND ERROR\n", 11);
                }
                else
                    write(STDOUT, msg, strlen(msg));
            }
            if (FD_ISSET(STDIN, &ready_users))
            {
                read(STDIN, msg, sizeof(msg));
                if (firsttype == 0)
                {
                    firsttype = 1;
                    strcpy(buf, msg);
                    buf[strlen(buf) - 1] = 0;
                    strcpy(username, buf);
                }
                nbytes = send(serverfd, msg, strlen(msg), 0);
            }
        }
        else
        {
            bc_ready = bc;
            if (select(broadcastrecvfd + 1, &bc_ready, NULL, NULL, NULL) <= 0)
                break;
            if (FD_ISSET(broadcastrecvfd, &bc_ready))
            {
                memset(msg, '\0', 256);
                nbytes = recvfrom(broadcastrecvfd, msg, sizeof(msg), 0, NULL, 0);
                if (nbytes > 0)
                {
                    strcpy(buf, msg);
                    token = strtok(buf, ":");
                    if (strcmp(token, username) != 0)
                        write(STDOUT, msg, strlen(msg));
                }
            }
            if (FD_ISSET(STDIN, &bc_ready))
            {
                memset(msg, '\0', 256);
                read(STDIN, msg, sizeof(msg));
                if (strlen(msg) > 0)
                {
                    strcpy(buf, username);
                    if (strcmp(msg, "$exit_group$\n") == 0)
                    {

                        strcat(buf, " LEFT THE GROUP\n");
                        nbytes = sendto(broadcastsendfd, buf, strlen(buf), 0, (struct sockaddr *)&bcsendaddr, sizeof(bcsendaddr));
                        if (nbytes < 0)
                            write(STDERR, "BROADCAST SEND ERROR\n", 21);

                        close(broadcastsendfd);
                        close(broadcastrecvfd);
                        port = atoi(argv[1]);

                        broadcastsendfd = socket(AF_INET, SOCK_DGRAM, 0);
                        broadcast = 1;
                        if (setsockopt(broadcastsendfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
                            write(STDERR, "OPT BROADCAST ERROR\n", 20);
                        broadcastrecvfd = socket(AF_INET, SOCK_DGRAM, 0);

                        flg = 1;
                        if (setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEADDR, &flg, sizeof(flg)) < 0)
                            write(STDERR, "OPT REUSEADDR ERROR\n", 20);
                        flg = 1;
                        if (setsockopt(broadcastrecvfd, SOL_SOCKET, SO_REUSEPORT, &flg, sizeof(flg)) < 0)
                            write(STDERR, "OPT REUSEPORT ERROR\n", 20);

                        bcsendaddr.sin_port = htons(port);

                        bcrecvaddr.sin_port = htons(port);
                    }
                    else
                    {
                        strcat(buf, ":");
                        strcat(buf, msg);
                        nbytes = sendto(broadcastsendfd, buf, strlen(buf), 0, (struct sockaddr *)&bcsendaddr, sizeof(bcsendaddr));
                        if (nbytes < 0)
                            write(STDERR, "BROADCAST SEND ERROR\n", 21);
                    }
                }
            }
        }
    }
}
