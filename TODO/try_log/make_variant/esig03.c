// daemonised
// uses signal_fd approach instead of signal()
// SIGUSR1 only, makes a warning message, then exits after 30 s delay
// https://manpath.be/f33/2/signalfd#L22
// https://stackoverflow.com/questions/43212106/handle-signals-with-epoll-wait-and-signalfd

#include "hchat03.h"

#define BDFILE "/home/tatik/Tatik/share/mychat/bd.txt"  // path_from_root for the good of the daemon
#define LOGFILE "/home/tatik/Tatik/share/mychat/log.txt"

#define MAX_EPOLL_EVENTS 30
#define MAX_USERS 10
#define CUR_VERSION "0.3"

char* WELCOME = "@@60@#0.3@#SERV@#User@#0@#Connected.Verifying autorisation##";
char* SIG_MESSAGE = "@@154@#0.3@#SERV@#USER@#0@#Ladies and gentlemen, this is your daemon speaking. The server is affected by an external signal and goes to shutdown in 30 s##";


typedef struct {
    char login [MAX_LEN];
    int fd;
} usr;

/**************************************************************/

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

void setFdNonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

long int filesize( FILE *fp) {
    long int save_pos, size_of_file;
    save_pos = ftell( fp );
    fseek( fp, 0L, SEEK_END );
    size_of_file = ftell( fp );
    fseek( fp, save_pos, SEEK_SET );
    return( size_of_file );
}

void make_send(msg* mess, usr* list, int k) {
    char message [BUF_SIZE];
    memset (message, 0, BUF_SIZE);
    strcpy(message, format_msg (mess, k));
    for (int i = 0; i < MAX_USERS; i++) {
        if (strlen(list[i].login) && list[i].fd) {
            send (list[i].fd, message, BUF_SIZE, 0);
        }
    }
    return;
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
    strcat (mess[2].msg_itself, "\n  ");

    for (int i = 0; i < MAX_USERS; i++) {
         if (strlen(list[i].login)) {
             strcat (mess[2].msg_itself, list[i].login);
             strcat (mess[2].msg_itself, "\n  ");
         }
    }
    mess[2].len = len_count(mess,2);
    make_send(mess,list,2);
    return;
}

void remove_client(int value, usr* list) { 
    for (int i = 0; i < MAX_USERS; i++) {
        if (list[i].fd == value) { 
            strcpy(list[i].login,"\0");
            list[i].fd = 0;
            return;
        }
    }
    return; 
}

void announce_join (msg* mess, usr* list, int fd) {
    strcpy (mess[1].msg_itself, mess[1].to);
    strcpy (mess[1].to, "CHAT");
    strcat (mess[1].msg_itself, " enters the chat");
    mess[1].len = len_count(mess, 1);
    strcpy(mess[1].message_str, format_msg (mess, 1));
    sleep(1);
    return;
}


void accept_user(msg *mess, int fd, usr* list) { 
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
    mess[1].len = len_count(mess, 1);

    strcpy(message, format_msg (mess, 1));
    send (mess[1].fd, message, strlen(message) + 1, 0);
    sleep(1);
    refresh_online (mess, list);
    sleep(1);
    announce_join (mess,list,fd);
    make_send(mess,list,1);
    memset (message, 0, BUF_SIZE);
    return;
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
            strcpy(mess[k].from, list[i].login);
            return;
        }
    }
    err_scream ("User not found in find");
    return;
}

void private_user (msg* mess, usr* list, int k ) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (!strcmp(list[i].login,mess[k].to)) {
            mess[k].fd = list[i].fd;
            return;
        }
    }
    err_scream ("User not found in private");
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
    mess[1].len = len_count(mess,1);
    strcpy(message, format_msg (mess, 1));
    send (mess[1].fd, message, strlen(message) + 1, 0);
    sleep(5);
    remove_client (fd, list);
    err_scream ("User to throw off not found");
    return;
}

void register_handler (msg* mess, usr* list, int fd) {
    int acc = 0;
    int found = 0;
    FILE * mybd = fopen (BDFILE,"r");
    if (mybd == NULL) err_exit ("failed to open database file");

    char * string = calloc(1, MAX_LEN + 1);
    char * string2 = calloc(1, MAX_LEN + 1);
    char * string3 = calloc(1, 2 * MAX_LEN +1);
    strcat (string3, mess[0].from);
    strcat (string3, " ");
    strcat (string3, mess[0].msg_itself);
    strcat (string3, "\0");
    strcat (string2, mess[0].from);
    strcat (string2, "\0");
    while (!feof(mybd) ) {
        fscanf(mybd,"%s%*[^\n]", string);
        acc = my_cmp (string, string2);
        if (acc) {
            found = 1;
            refuse(mess, list, fd);
            return;
        }
    }
    if (!found) {
        freopen(BDFILE, "a", mybd);
        fprintf(mybd, "%s\n", string3);
        add_user(mess,list,fd);
        accept_user(mess, fd, list);
    }
    fclose(mybd);
    free(string);
    free(string2);
    free(string3);
    return;
}


void enter_handler (msg* mess, usr* list, int fd) {
    int acc = 0;
    FILE * mybd = fopen (BDFILE,"r");
    if (mybd == NULL) err_exit ("failed to open database file");

    char * string = calloc(1, 2 * MAX_LEN +1);
    char * string2 = calloc(1, 2 * MAX_LEN +1);
    strcat (string2, mess[0].from);
    strcat (string2, " ");
    strcat (string2, mess[0].msg_itself);
    strcat (string2, "\0");
    while (!feof(mybd)) { 
        fgets(string, (2 * MAX_LEN + 1), mybd);
        acc = my_cmp (string, string2);
        if (acc) {
            add_user(mess,list,fd);
            accept_user(mess, fd, list);
            free(string);
            free(string2);
            rewind(mybd);
            return;
        } 
    } 
    if (!acc) {
        refuse(mess,list,fd);
    }
    fclose(mybd);
    free(string);
    free(string2);
    rewind(mybd);
    return;
}

void chat_handler (msg* mess, usr* list) {
    time_t current_time;
    time(&current_time);
    mess[1] = mess[0];
    strcpy (mess[1].version, CUR_VERSION);
    strcpy (mess[1].to, "CHAT");
    memset (mess[1].msg_itself, 0, sizeof(mess[1].msg_itself));
    strcat (mess[1].msg_itself, mess[1].from);
    strcat (mess[1].msg_itself, ": ");
    strcat (mess[1].msg_itself, mess[0].msg_itself);
    
    char* tmp = ctime(&current_time);
    FILE *mylog = fopen (LOGFILE, "a");
    if (mylog == NULL) err_exit ("failed to open log file");
    long int fsize = filesize(mylog);
    if (fsize >= 512) {
        fclose(mylog);
        remove(LOGFILE);
        mylog = fopen (LOGFILE, "a");
    }
    fprintf(mylog, "%.16s  %s\n ", tmp, mess[1].msg_itself ); 
    fclose(mylog);
    
    mess[1].len = len_count(mess, 1);
    strcpy(mess[1].message_str, format_msg (mess, 1));
    return;
}


void immed_exit (msg* mess, usr* list, int fd) {
    mess[1] = mess[0];
    find_user (mess, list, fd, 1);
    strcpy (mess[1].to, "CHAT");
    strcpy (mess[1].msg_itself, mess[1].from);
    if (mess[1].comm == 4) {
        close(fd);
        strcat (mess[1].msg_itself, " says BYE and leaves the chat");
    } else {
        mess[1].comm = 4;
        strcat (mess[1].msg_itself, " leaves the chat and slams the door");
    }
    mess[1].len = len_count(mess,1);
    strcpy(mess[1].message_str, format_msg (mess, 1));
    sleep(1);
    remove_client(fd, list);
    refresh_online (mess, list);
    sleep(1);
    mess[1].comm = mess[0].comm;
    return;
}


void msg_welcome(int fd) {
    char  message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    sprintf(message, WELCOME, fd);
    return;
}


void private_handler(msg* mess, usr* list) {
    mess[4] = mess[0];
    private_user (mess, list, 4);
    memset (mess[4].msg_itself, 0, sizeof(mess[4].msg_itself));
    strcat (mess[4].msg_itself, mess[4].from);
    strcat (mess[4].msg_itself, ": privat: ");
    strcat (mess[4].msg_itself, mess[0].msg_itself);
    mess[4].len = len_count(mess, 4);
    strcpy(mess[4].message_str, format_msg (mess, 4));
    send (mess[4].fd, mess[4].message_str, strlen(mess[4].message_str) + 1, 0);
    return;
}


void log_handler(msg* mess) {
    char message[BUF_SIZE];
    memset (message, 0, BUF_SIZE);
    char * line = malloc(BUF_SIZE);
    mess[3].comm = 3;
    strcpy (mess[3].version, CUR_VERSION);
    strcpy (mess[3].from, "SERV");

    FILE *mylog = fopen (LOGFILE, "r");
    if (mylog == NULL) err_exit ("failed to open log file");

    while (fscanf(mylog, "%[^\n] ", line) != EOF)  {
        strcpy (mess[3].msg_itself, line);
        mess[3].len = len_count(mess, 3);
        strcpy(message, format_msg (mess, 3));
        printf("DEBUG cycle in_log_create, %s\n", message);
        usleep(100);
        send (mess[3].fd, message, strlen(message) + 1, 0);
    }
    strcpy (mess[3].msg_itself, "/ENDLOG");
    mess[3].len = len_count(mess, 3);
    strcpy(message, format_msg (mess, 3));
    sleep(2);
    printf("DEBUG afier cycle in_log_create, %s\n", message);
    send (mess[3].fd, message, strlen(message) + 1, 0);

    fclose(mylog);
    free(line);
    return;
}


void msg_handler (int fd, int epollfd, msg* mess, usr* list) {
    char buff[BUF_SIZE];
    memset(buff, 0, BUF_SIZE);
    int ret = read(fd, buff, BUF_SIZE);
    if (ret == 0) {
        printf("client closed\n");
        close(fd);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        immed_exit(mess,list,fd);
        goto sending;
    } else {
        read_msg(buff, mess, 0);
        mess[0].fd = fd;
        int comd = mess[0].comm; 
    
        switch (comd) {
            case 0:
                enter_handler (mess, list, fd);
                return;
            case 1:
                chat_handler (mess, list);
                break;
            case 2:
               private_handler (mess,list);
               return;
            case 3:
                mess[3].fd = mess[0].fd;
                strcpy (mess[3].to, mess[0].from);
                log_handler (mess);
                return; 
            case 4:
                immed_exit(mess, list, fd);
                break;
            case 5:
                register_handler (mess, list, fd);
                return;
 
        }  

//**********************
sending:
        make_send(mess,list,1);
    }
    return;
}


void exit_handler(int fd, usr* list, int epollfd) {
    char message[BUF_SIZE];
    memset(message, 0, BUF_SIZE);
    strcpy(message, SIG_MESSAGE);
    for (int i = 0; i < MAX_USERS; i++) {
        if (strlen(list[i].login) && list[i].fd) {
            send (list[i].fd, message, BUF_SIZE, 0);
        }
    }
    sleep(30);
    return;
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

/***********************************************/
 
int main(int argc, const char *argv[]) {
    int portn = PORT; 
//    mydaemon();    

    int sockfd = create_socket(portn);
    printf(" sockfd %d created \n", sockfd);
    int running = 1;
    setFdNonblock(sockfd);

    int numcl = 0;
    int maxmsg = 5;
    int maxclient = MAX_USERS;
    
    msg *mymessage = NULL;
    mymessage = calloc (maxmsg, sizeof *mymessage);

    usr *listusers = NULL;
    listusers = calloc (maxclient, sizeof *listusers);
    
    for (int i = 0; i < maxclient; i++) {
        strcpy(listusers[i].login, "\0");
        listusers[i].fd = 0;
    }
    
    for (int i = 0; i < maxmsg; i++) {
        strcpy(mymessage[i].version, "\0");
        strcpy(mymessage[i].from, "\0");
        strcpy(mymessage[i].to, "\0");
        strcpy(mymessage[i].msg_itself, "\0");
        strcpy(mymessage[i].message_str, "\0");
        mymessage[i].fd = 0;
        mymessage[i].comm = 0;
        mymessage[i].len = 0;
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

    while(running) {
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
                listusers[numcl].fd = connfd;
                msg_welcome (connfd);
                refresh_online(mymessage, listusers);
                numcl++;
            } else if  (eventfd != signal_fd) {
                msg_handler(eventfd, epollfd, mymessage, listusers);  // accepted sockets' event                  
            } else {
                exit_handler(signal_fd,listusers,epollfd); // signal
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
 
