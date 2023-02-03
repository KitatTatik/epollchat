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

#define PORT 4444
#define STR_MESSAGE "User # %d: %s"

#define SIG_MESSAGE "\n     Ladies and gentlemen, this is your daemon speaking.\n     The server is affected by an external signal and goes to shutdown in 30 s\n\n"

#define BDFILE "/home/Tatik/share/mychat/bd.txt"
#define LOGFILE "/home/Tatik/share/mychat/log.txt"

#define MAX_EPOLL_EVENTS  30
#define BUF_SIZE  1024
#define MAX_USERS  10
#define MAX_LEN 10
#define CUR_VERSION "0.1"

char* WELCOME = "@@60@#0.1@#SERV@#User@#0@#Connected.Verifying autorisation##";
int refresh_list = 0;


typedef struct {
    char delimiters[14];    /* for correct sizeof only */
    int len;
    char version [4];
    char from [MAX_LEN];
    char to [MAX_LEN];
    int  comm;       /*0 entr,1 chat,2 priv,3 logg,4 exit 6 onlinelist*/
    char msg_itself [BUF_SIZE - 2 * MAX_LEN - 25];
    char message_str [BUF_SIZE];
    int fd;
} msg;


typedef struct {
    char login [MAX_LEN];
    int fd;
} usr;


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

int len_int(int number) {
    int result = 0;
    while (number != 0) {
        number /= 10;
        result++;
    }
    return result;
}

void setFdNonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void err_exit(const char *s) {
    printf("error: %s\n",s);
    exit(0);
}

void err_scream(const char *s) {
    printf("error: %s\n",s);
    return;
}


char* format_msg(msg *aptr, int i) {
    char *len_all;
    char *message_str = calloc (1,BUF_SIZE);
    printf("DEBUG 0002 format_msg, %d\n", aptr[i].len);
    if( NULL != (len_all = malloc(5))) sprintf (len_all, "%d",aptr[i].len);
    printf("DEBUG 0003 format_msg, %s\n", len_all);
    /* Glue message components into a string */
    strcat (message_str, "@@");
    strcat (message_str, len_all);
    free(len_all);
    strcat (message_str, "@#");
    strcat (message_str, aptr[i].version);
    strcat (message_str, "@#");
    strcat (message_str, aptr[i].from);
    strcat (message_str, "@#");
    strcat (message_str, aptr[i].to);
    strcat (message_str, "@#");
    if( NULL != (len_all = malloc(2))) sprintf (len_all, "%d", aptr[i].comm);
    strcat (message_str, len_all);
    free(len_all);
    strcat (message_str, "@#");
    strcat (message_str, aptr[i].msg_itself);
    strcat (message_str, "##");
    strcat (message_str, "\0");
    int len = strlen (message_str);
    if (len == aptr[i].len) {
        return (message_str);
    } else {
        err_scream ("outgoing length");
        printf("real length %d  differs from counted value %d oops\n", len, aptr[i].len);
        return("");
    }
}


void refresh_online(msg* mess, usr* list) {
    char message [BUF_SIZE];
    memset (message, 0, BUF_SIZE);
    mess[2].len = 0;
    mess[2].comm = 6;
    strcpy (mess[2].version, CUR_VERSION);
    strcpy (mess[2].from, "SERV");
    strcpy(mess[2].to, "CHAT");
    memset (mess[2].msg_itself, 0, MAX_LEN * MAX_USERS + 1);
    strcat (mess[2].msg_itself, "USERS ONLINE");
    strcat (mess[2].msg_itself, "\n");

    for (int i = 0; i < MAX_USERS; i++) {
         if (strlen(list[i].login)) {
             strcat (mess[2].msg_itself, list[i].login);
             strcat (mess[2].msg_itself, "\n");
         }
    }
    int msg_length =  strlen(mess[2].from) + strlen(mess[2].to)
                   + strlen(mess[2].msg_itself) + strlen(mess[2].version) + 15;  //+1 ??
    int tmp = len_int (msg_length);
    mess[2].len = msg_length + tmp;
    printf("DEBUG LIST AND msg_its %s \n", mess[2].msg_itself);
    strcpy(message, format_msg (mess, 2));
    for (int i = 0; i < MAX_USERS; i++) {
        if (strlen(list[i].login)) {
            send (list[i].fd, message, strlen(message) + 1, 0);
            printf("ADD SENT %d %s %d %s \n", i, mess[2].msg_itself, list[i].fd, list[i].login);
        }
    }
    return;
}


int remove_client(int array[], int n, int value, usr* list) { 
    int j = 0; 
    for (int i = 0; i < n; i++) {
        if (array[i] != value) { 
            array[j++] = array[i]; 
        } else {
            strcpy(list[i].login,"\0");
            list[i].fd = 0;
        }
    }
    n = j;
    return(n); 
}

int accept_user(msg *mess, int fd, usr* list) { 
    char message[BUF_SIZE];
    memset (message, 0, BUF_SIZE);
    mess[1].len = 0;
    mess[1].comm = 0;
    mess[0].fd = fd;
    strcpy (mess[1].version, CUR_VERSION);
    strcpy (mess[1].to, mess[0].from);
    strcpy (mess[1].from, "SERV");
    mess[1].fd = mess[0].fd;
    memset (mess[1].msg_itself, 0, sizeof(mess[1].msg_itself));
    strcat (mess[1].msg_itself,"WELCOME, ");
    strcat (mess[1].msg_itself, mess[1].to);
    if (strcmp(mess[1].version,mess[0].version)) {
        strcat (mess[1].msg_itself, " Please update your client version");
    }
    int msg_length =  strlen(mess[1].from) + strlen(mess[1].to)
                   + strlen(mess[1].msg_itself) + strlen(mess[1].version) + 15;  //+1 ??
    int tmp = len_int (msg_length);
    mess[1].len = msg_length + tmp;

    printf("DEBUG ACCEPT %d  MSG %s \n", mess[1].fd, mess[1].msg_itself);
    strcpy(message, format_msg (mess, 1));
    send (mess[1].fd, message, strlen(message) + 1, 0);
    printf("DEBUG ACCEPT SENT BEFORE LIST %s \n", message);
    sleep(2);
    refresh_online (mess, list);
//    sleep(2);
//    announce_join (mess,list,fd);
    printf("DEBUG ACCEPT SENT %d LIST %s \n", mess[1].fd, message);
    memset (message, 0, BUF_SIZE);
    return(1);
}

int my_cmp (char* large, char* small) {
    char* tmp = NULL;
    tmp = strstr(large,small);
    if (tmp != NULL) {
        return(1);
    } else {
        return(0);
    }
}

void find_user (msg* mess, usr* list, int fd, int k ) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (list[i].fd == fd) { 
            printf("DEBUG find user %d  AND login %s \n", i, list[i].login);
            strcpy(mess[k].from, list[i].login);
            return;
        }
    }
    err_scream ("User not found in find");
    return;
}

void add_user(msg* mess, usr* list, int fd ) { 
    int found = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (list[i].fd == fd) {
            strcpy(list[i].login, mess[0].from);
            found = 1;
        }
    } 
    if (!found) {
        err_scream( "user to add not found");
    } 
    return;
}

void refuse (msg* mess, usr* list, int fd ) {
    char message [BUF_SIZE];
    memset (message, 0, BUF_SIZE);
    mess[1].comm = 0;
    mess[0].fd = fd;
    strcpy (mess[1].version, CUR_VERSION);
    strcpy (mess[1].to, mess[0].from);
    strcpy (mess[1].from, "SERV");
    mess[1].fd = mess[0].fd;
    memset (mess[1].msg_itself, 0, sizeof(mess[1].msg_itself));
    strcat (mess[1].msg_itself, "Not found in BD, ");
    strcat (mess[1].msg_itself, mess[1].to);
    int msg_length =  strlen(mess[1].from) + strlen(mess[1].to)
                   + strlen(mess[1].msg_itself) + strlen(mess[1].version) + 15;  //+1 ??
    int tmp = len_int (msg_length);
    mess[1].len = msg_length + tmp;

    printf("DEBUG REFUSE %d  AND msg_its %s \n", mess[1].fd, mess[1].msg_itself);
    strcpy(message, format_msg (mess, 1));
    send (mess[1].fd, message, strlen(message) + 1, 0);
    printf("DEBUG refuse sent %s\n", message);
    sleep(5);
    for (int i = 0; i < MAX_USERS; i++) {
        if (list[i].fd == fd) {
            strcpy(list[i].login, "\0");
            close(fd);
            return;
        }
    }
    err_scream ("User to throw off not found");
    return;
}

int enter_handler (FILE* mybd, msg* mess, usr* list, int fd) {
    int acc = 0;
    char * string = calloc(1, 2 * MAX_LEN +1);
    char * string2 = calloc(1, 2 * MAX_LEN +1);
    strcat (string2, mess[0].from);
    strcat (string2, " ");
    strcat (string2, mess[0].msg_itself);
    strcat (string2, "\0");
    printf("DEBUG ENTER HANDLER %s\n", mess[0].msg_itself);
    while (!feof(mybd)) { 
        fgets(string, (2 * MAX_LEN + 1), mybd);
        acc = my_cmp (string, string2);
        if (acc) {
//            accept_user(mess, fd, list);
            add_user(mess,list,fd);
            accept_user(mess, fd, list);
            free(string);
            free(string2);
            rewind(mybd);
            return(1);
        } 
    } 
    if (!acc) {
        refuse(mess,list,fd);
    }
    free(string);
    free(string2);
    rewind(mybd);
    return(0);
}

int chat_handler (msg* mess, usr* list, int fd) {
    mess[1] = mess[0];
    //find_user (mess, list, fd, 1);
    strcpy (mess[1].version, CUR_VERSION);
    strcpy (mess[1].to, "CHAT");
    memset (mess[1].msg_itself, 0, sizeof(mess[1].msg_itself));
    strcat (mess[1].msg_itself, mess[1].from);
    strcat (mess[1].msg_itself, ": ");
    strcat (mess[1].msg_itself, mess[0].msg_itself);
    int msg_length =  strlen(mess[1].from) + strlen(mess[1].to)
                   + strlen(mess[1].msg_itself) + strlen(mess[1].version) + 15;  //+1 ??
    int tmp = len_int (msg_length);
    mess[1].len = msg_length + tmp;
    printf("DEBUG CHAT %d  AND msg_its %s \n", mess[1].fd, mess[1].msg_itself);
    strcpy(mess[1].message_str, format_msg (mess, 1));
    printf("DEBUG CHAT END string %s \n", mess[1].message_str);
    return(0);
}

int msg_welcome(int fd) {
    char  message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    sprintf(message, WELCOME, fd);
    return(1);
}

int read_msg(char *rcv_msg, msg* msgptr1, int i) {
/*  tears the received string apart*/
    int ctrl_len = 0,
        temp_len = 0;
    char *start = NULL;
    char *fin = NULL;
    char *temp = NULL;
    printf("DEBUG 2 received msg %s\n", rcv_msg);
    ctrl_len = strlen (rcv_msg);
    if (!ctrl_len) {
        err_scream ("incoiming nullstring");
        return 0;
    } else {
        if (NULL == (start = strstr (rcv_msg,"@@"))) {
           err_scream ("header lost");
           return 0; 
        } //RETODO if not found
        fin = strstr (rcv_msg,"@#");  
        temp_len = fin - start - 2;
        temp = calloc (1, temp_len + 1);
        memcpy (temp, start + 2, temp_len);
        temp_len = atoi (temp);
        printf("len diff is %d\n", ctrl_len - temp_len);
        if ((ctrl_len - temp_len) != 0) {
            err_scream ("incoiming length");
            printf("error in receiving length %d\n", ctrl_len -temp_len);
            return 0;
        } else {
            msgptr1[i].len = temp_len;
            start = strstr(rcv_msg,"@#") + 2;
            fin = strstr(start,"@#") ;
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1[i].version,temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1[i].from,temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1[i].to, temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            msgptr1[i].comm = atoi(temp);
            start = fin + 2;
            fin = strstr(start,"##"); 
            if(NULL == (fin = strstr (start,"##"))) {
                err_scream ("footer lost"); 
                return 0;  //RETODO if not found
            }
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1[i].msg_itself, temp);
            printf("DEBUG 3 received msg its %s\n", msgptr1[i].msg_itself);

            free(temp);
            return(1);
        }
    }
}             

int privat_handler() {
}

int log_handler() {
}

int msg_handler(int fd, int array[], int n, int epollfd, msg* mess, usr* list, FILE * mybd ) {
    char buff[BUF_SIZE], message[BUF_SIZE];
    memset(buff, 0, BUF_SIZE);
    memset(message, 0, BUF_SIZE);
//    if (refresh_list) {
//        refresh_online(mess, list);
//        refresh_list = 0;
//    }
    int ret = read(fd, buff, BUF_SIZE);
//        printf("%s\n", buff);
    if (ret == 0) {
        printf("client closed\n");
        close(fd);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        n = remove_client(array, n, fd, list);
        refresh_online(mess, list);
        return(-1);
    } else {
        printf("DEBUG 1 receive %s\n", buff);
        read_msg(buff, mess, 0);
        printf("DEBUG 3 before cmd %s\n", buff);

        int comd = mess[0].comm; 
        printf("DEBUG CMD %d\n", comd);

        switch (comd) {
            case 0:
                enter_handler(mybd,mess, list,fd);
            //    refresh_online(mess, list);
                return(1);
            case 1:
                chat_handler ( mess, list, n);
                break;
//        case 2:
//            privat_handler();
//            break;
//        case 3:
//            log_handler();
//            break; 
        }  
        printf("DEBUG 4 after cmd %s\n", mess[1].message_str);

//**********************
        strcpy(message, mess[1].message_str); 
        printf("DEBUG 5 after message reading fd and msg twice %d\t %s\t %s\n", fd, message, mess[1].message_str);
      
        for (int i = 0; i < n; i++) {
            printf("cycle send, %d \t %d \t %s \n", array[i], n, message);
           // if (list[i].fd != fd) {       /* fd 0 1 2 reserved by server itself */
                send (list[i].fd, message, BUF_SIZE, 0);
           // }
        }  
    }
    return(1);
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
    int portn = PORT; 
//    mydaemon();    
    FILE * mybd;
    mybd = fopen ("bd.txt","r");
    if (mybd == NULL) err_exit ("failed to open file");
    int sockfd = create_socket(portn);
    printf(" sockfd %d created \n", sockfd);
    int running = 1;
    setFdNonblock(sockfd);
    char buff [BUF_SIZE];
    int listcl [MAX_USERS];
    int numcl = 0;
    
    int maxmsg = 4;
    int maxclient = MAX_USERS;
    msg *mymessage = NULL;
    mymessage = calloc (maxmsg, sizeof *mymessage);
    usr *listusers = NULL;
    listusers = calloc (maxclient, sizeof *listusers);
    for (int i = 0; i < maxclient; i++) {
        strcpy(listusers[i].login, "\0");
        listusers[i].fd = 0;
        listcl[i] = 0;
    }
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

    while(1) {
        int number = epoll_wait(epollfd, events, MAX_EPOLL_EVENTS, -1);
        for (int i = 0; i < number; i++) { 
            int eventfd = events[i].data.fd;
            if(eventfd == sockfd) {  // listen event - new connect
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
                listusers[numcl].fd = connfd;
                msg_welcome (connfd);
                refresh_list = 1;
                if (refresh_list) {
                    refresh_online(mymessage, listusers);
                    refresh_list = 0;
                }
                numcl++;
            } else if  (eventfd != signal_fd) {
                msg_handler(eventfd, listcl, numcl, epollfd, mymessage, listusers, mybd);  // accepted sockets' event                  
            } else {
                exit_handler(signal_fd,listcl,numcl,epollfd); // signal
                exit(0);
            }
        }
    }
    close(sockfd);
    close(epollfd);
    free(mymessage);
    free(listusers);

//   syslog (LOG_NOTICE, "My daemon terminated.");
 //   closelog();

    return(0);
}
 
