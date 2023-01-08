#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
 
#define BUF_SIZE 1024
#define PORT 4444
#define MAX_EVENTS 16
#define SERVER "127.0.0.4"

void err_exit(const char *s) {
    printf("error: %s\n",s);
    exit(0);
}
 
int create_socket(int port_number) {
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;           
    server_addr.sin_port = htons(port_number);
    if ( inet_pton(AF_INET, SERVER, &server_addr.sin_addr.s_addr) == 0 )  err_exit("server");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)  err_exit("socket");
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)  err_exit("connect");
    return sockfd;
}
 
int main(int argc, const char *argv[]) { 
    int port =  PORT;
    int sock = create_socket(port);
    char sbuffer[BUF_SIZE], rbuffer[BUF_SIZE];

    struct epoll_event event, events[MAX_EVENTS];
    int i, num_ready, bytes_read;
    int running = 1;

    int epfd = epoll_create1(0);
    if (epfd == -1) err_exit("epoll_create");

    event.events = EPOLLIN;
    event.data.fd = sock;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event)) err_exit(" socket descriptor");

    event.events = EPOLLIN;
    event.data.fd = 0;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event)) err_exit(" stdin descriptor");

    while (running) {
        num_ready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (i = 0; i < num_ready; i++) {
            if (0 == events[i].data.fd) {
                memset(sbuffer, 0, BUF_SIZE);
                fgets(sbuffer, BUF_SIZE, stdin);
                send(sock, sbuffer, strlen(sbuffer + 1),0);
            } else {
                memset(rbuffer,0,BUF_SIZE);
                bytes_read = read(events[i].data.fd, rbuffer, BUF_SIZE);
		rbuffer[bytes_read] = '\0';
		printf("\t %s\n", rbuffer);
            }
	}
    }
    return 0;
}    
