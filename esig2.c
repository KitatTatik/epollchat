// daemonised
// uses signal_fd approach instead of signal()
// SIGUSR1 only, makes a warning message, then exits after 30 s delay
// https://manpath.be/f33/2/signalfd#L22
// https://stackoverflow.com/questions/43212106/handle-signals-with-epoll-wait-and-signalfd


#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>

#define port 4444
#define STR_MESSAGE "User # %d:  %s"
#define WELCOME "Welcome to daemon-epoll-chat, your ID is # %d. %s"

#define SIG_MESSAGE "\n     Ladies and gentlemen, this is your daemon speaking.\n     The server is affected by an external signal and goes to shutdown in 30 s\n\n"

const int MAX_EPOLL_EVENTS = 100;
const int BUF_SIZE = 1024;

static void mydaemon() {
    pid_t pid;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    chdir("/");

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close (x);
    }
    openlog ("epolldaemon", LOG_PID, LOG_DAEMON);
    syslog (LOG_NOTICE, "My epoll daemon started.");
}


int RemoveClient(int array[], int n, int value) { 
    int j = 0; 
    for (int i = 0; i < n; i++) {
        if (array[i] != value) { 
            array[j++] = array[i]; 
        }
    }
    n = j;
    return(n); 
}

void setFdNonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}
 
void err_exit(const char *s) {
    printf("error: %s\n",s);
    exit(0);
}
 
int msg_welcome(int fd) {
    char buff[BUF_SIZE], message[BUF_SIZE];
    memset(buff, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    sprintf(message, WELCOME, fd, buff);
    send(fd, message, BUF_SIZE, 0);
    return(1);
}

int msg_handler(int fd, int array[], int n, int epollfd) {
    char buff[BUF_SIZE], message[BUF_SIZE];
    memset(buff, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
    int ret = read(fd, buff, BUF_SIZE);
//        printf("%s\n", buff);
    if (ret == 0) {
        printf("client closed\n");
        close(fd);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        n = RemoveClient(array, n, fd);
        return(-1);
    } else {
        sprintf(message, STR_MESSAGE, fd, buff);
        for (int i = 0; i < n; i++) {
            if (array[i] != fd) {
               send(array[i], message, BUF_SIZE, 0);
            }
        }  
    return(1);
    }
}

int exit_handler(int fd, int array[], int n, int epollfd) {
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    sprintf(message, SIG_MESSAGE);
    for (int i = 0; i < n; i++) {
        if (array[i] != fd) {
            send(array[i], message, BUF_SIZE, 0);
        }
    }
    sleep(30);
/*        for (int i=0; i < n; i++) {
               close(array[i]);
               epoll_ctl(epollfd, EPOLL_CTL_DEL, array[i], NULL);
        }
*/
    return(1);
}


int create_socket(int port_number) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port_number);
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        err_exit("socket");
    }
    int reuse = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        err_exit("setsockopt");
    }
    if(bind(sockfd, (struct sockaddr *)(&server_addr), sizeof(server_addr)) == -1) {
        err_exit("bind");
    }
    if(listen(sockfd, 5) == -1) {
        err_exit("listen");
    }
    return sockfd;
}
 
int main(int argc, const char *argv[]) {
    int portn = port; 
    mydaemon();    
    
    int sockfd = create_socket(portn);
    printf(" sockfd %d created \n", sockfd);
    int running = 1;
    setFdNonblock(sockfd);
    char buff[BUF_SIZE];
    int listcl[20];
    int numcl = 0;

    int epollfd = epoll_create1(0);
    if(epollfd == -1) {
        err_exit("epoll_create1");
    }
    struct epoll_event ev;
    ev.data.fd = sockfd;
    ev.events = EPOLLIN ;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        err_exit("epoll_ctl1");
    }
    
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    int r = sigprocmask(SIG_BLOCK, &mask, 0);
    if (r == -1) {
        err_exit("sigprocmask");
    }
    int signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        err_exit("signal descriptor");
    }

    ev.data.fd = signal_fd;
    ev.events = EPOLLIN;
    r = epoll_ctl(epollfd, EPOLL_CTL_ADD, signal_fd, &ev);
    if (r == -1) {
        err_exit("sigfd add to epoll error");
    }

    struct epoll_event events[MAX_EPOLL_EVENTS];
    for (int i = 0; i < 20; i++) { 
        listcl[i] = 0;
    }
    while(1) 
    {
        int number = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, -1);
        for (int i = 0; i < number; i++) { 
            int eventfd = events[i].data.fd;
            if(eventfd == sockfd) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int connfd = accept(sockfd, (struct sockaddr *)(&client_addr), &client_addr_len);
                setFdNonblock(connfd);
                struct epoll_event ev;
                ev.data.fd = connfd;
                ev.events = EPOLLIN;
                if(epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    err_exit("epoll_ctl2");
                }
                listcl[numcl] = connfd;
                numcl++;
                msg_welcome(connfd);
            } else if  (events[i].data.fd!=signal_fd) {
                msg_handler(events[i].data.fd, listcl, numcl, epollfd);                    
            } else {
                exit_handler(signal_fd,listcl,numcl,epollfd);
                exit(0);
            }
        }
    }
    close(sockfd);
    close(epollfd);

    syslog (LOG_NOTICE, "My daemon terminated.");
    closelog();

    return(0);
}
 
