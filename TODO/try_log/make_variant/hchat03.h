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
#include <time.h>

#define MAX_LEN 10
#define BUF_SIZE 1024
#define PORT 4444


typedef struct {
    char delimiters[14];    /* for correct sizeof */
    int len;
    char version [4];
    char from [MAX_LEN];
    char to [MAX_LEN];
    int  comm;       /*0 entr,1 chat,2 priv,3 log,4 exit 6 onlinelist*/
    char msg_itself [BUF_SIZE - 2 * MAX_LEN - 25];
    char message_str [BUF_SIZE];
    int fd;
} msg;

char* format_msg( msg* mess, int k);
int read_msg( char* buffer, msg* mess, int k);
int len_int(int number);
void err_exit(const char *s);
void err_scream(const char *s);
int len_count( msg* mess, int k);
