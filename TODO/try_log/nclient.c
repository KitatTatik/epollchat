#include <stdlib.h>
#include <curses.h>
#include <menu.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define PORT 4444
#define MAX_EVENTS 16
#define SERVER "127.0.0.4"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD   4
#define MAX_LEN 10
#define CUR_VERSION "0.2"

typedef struct {
    char delimiters[14];    /* for correct sizeof only */
    int len;
    char version [4];
    char from [MAX_LEN];
    char to [MAX_LEN];
    int  comm;       /*0 entr,1 chat,2 priv,3 logg,4 exit 6 list online*/
    char msg_itself [BUF_SIZE - 2 * MAX_LEN - 25];
    char message_str [BUF_SIZE];
} msg;


char *choices[] = {
                        "Sign up      ", "Sign in      ",
                        (char *)NULL,
                  };

void err_exit(const char *s) {
    printf("error: %s\n",s);
    exit(0);
}

void err_scream(const char *s) {
    printf("error: %s\n",s);
    return;
}

int len_int(int number) {
    int result = 0;
    while (number != 0) {
        number /= 10;
        result++;
    }
    return result;
}

int create_socket(int port_number) {
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    if ( inet_pton(AF_INET, SERVER, &server_addr.sin_addr.s_addr) == 0 )  err_exit("server");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)  err_exit("socket");
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)  err_exit("connect");
    return sockfd;
}

char* get_password(WINDOW * win, char * password, int max_len, int hidden)
{
    int i = 0;
    int ch;
    while (((ch = wgetch(win)) != 10) && (i < max_len-1)) {
        if (ch == KEY_BACKSPACE) {
            int x, y;
            if (i==0) continue;
            getyx(win, y, x);
            mvwaddch(win, y, x-1, ' ');
            wrefresh(win);
            wmove(win, y, x-1);
            i--;
            continue;
        }
        password[i++] = ch;
        if (hidden) {
            wechochar(win, '*'); 
        } else {
            wechochar(win, ch); //for overt text
        }
    }
    password[i] = 0;
    wechochar(win, '\n');
    return(password);
}

int format_msg(msg *ptr) {
    char *len_all;
    if( NULL != (len_all = malloc(5))) sprintf (len_all, "%d", ptr->len);
    memset(ptr->message_str,0, strlen(ptr->message_str));
    strcat (ptr->message_str, "@@");
    strcat (ptr->message_str, len_all);
    free(len_all);
    strcat (ptr->message_str, "@#");
    strcat (ptr->message_str, ptr->version);
    strcat (ptr->message_str, "@#");
    strcat (ptr->message_str, ptr->from);
    strcat (ptr->message_str, "@#");
    strcat (ptr->message_str, ptr->to);
    strcat (ptr->message_str, "@#");
    if( NULL != (len_all = malloc(2))) sprintf (len_all, "%d", ptr->comm);
    strcat (ptr->message_str, len_all);
    free(len_all);
    strcat (ptr->message_str, "@#");
    strcat (ptr->message_str, ptr->msg_itself);
    strcat (ptr->message_str, "##");
    strcat (ptr->message_str, "\0");
    int len = strlen (ptr->message_str) ;   //+1 ??
    if (len == ptr->len) {
        return (0);
    } else {
        err_scream ("outgoing length");
        printf("real length %d  differs from counted value %d oops\n", len, ptr->len);
        return(1);
    }
}

int read_msg(char *rcv_msg, msg* msgptr1) {
/*  tears the received string apart*/
    int ctrl_len = 0,
        temp_len = 0;
    char *start = NULL;
    char *fin = NULL;
    char *temp = NULL;
    ctrl_len = strlen (rcv_msg);
    if (!ctrl_len) {
        err_scream ("incoiming nullstring\n");
        return 0;
    } else {
        if (NULL == (start = strstr (rcv_msg,"@@"))) {
            err_scream ("header lost\n");
            printf("message was %s\n", rcv_msg);
            return 0;
        } //RETODO if not found
        fin = strstr (rcv_msg,"@#");   
        temp_len = fin - start - 2;
        temp = calloc (1, temp_len + 1);
        memcpy (temp, start + 2, temp_len);
        temp_len = atoi (temp);
     //   printf("len diff is %d\n", ctrl_len - temp_len);
        if ((ctrl_len - temp_len) != 0) {
            err_scream ("incoiming length\n");
     //       printf("error in receiving length %d\n", ctrl_len -temp_len);
            return 0;
        } else {
            msgptr1->len = temp_len;
            start = strstr(rcv_msg,"@#") + 2;
            fin = strstr(start,"@#") ;
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1->version,temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1->from,temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1->to, temp);
            start = fin + 2;
            fin = strstr(start,"@#");
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            msgptr1->comm = atoi(temp);
            start = fin + 2;
            if(NULL == (fin = strstr (start,"##"))) {
                err_scream ("footer lost");
                return 0;  //RETODO if not found
            }
            temp_len = fin - start;
            temp = calloc (1, temp_len + 1);
            memcpy (temp, start, temp_len);
            strcpy(msgptr1->msg_itself, temp);
            free(temp);
            return(0);
        }
    }
}               

int login_screen (char *login, char *password) {
	ITEM **my_items;
	int c;	
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    refresh();
    MENU * my_menu;
    WINDOW * my_menu_win;
    WINDOW * wnd0;
    WINDOW * wnd;
    int n_choices, i;
    n_choices = ARRAY_SIZE(choices);
    my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for(i = 0; i < n_choices; ++i) {
        my_items[i] = new_item(choices[i], choices[i]);
    }
	my_menu = new_menu((ITEM **)my_items);
	menu_opts_off(my_menu, O_SHOWDESC);
    my_menu_win = newwin(10, 70, 8, 1);
    keypad(my_menu_win, TRUE);
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, 6, 68, 3, 1));
	set_menu_format(my_menu, 5, 3);
	set_menu_mark(my_menu, " # ");
    int lines = getmaxx(stdscr);
	attron(COLOR_PAIR(2));
	mvprintw(lines - 2, 0, "Use Arrow Keys to navigate, Enter to make choice");
	attroff(COLOR_PAIR(2));
	refresh();

	post_menu(my_menu);
    int32_t user_selection=0;
	wrefresh(my_menu_win);
    wnd0 = newwin(5, 23, 2, 2);
    wbkgd(wnd0, COLOR_PAIR(1));
    wattron(wnd0, A_BOLD);
    keypad(wnd0, TRUE);
    wprintw(wnd0, "LOGIN\n");
    get_password (wnd0, login, MAX_LEN, 0);
    wprintw(wnd0, "LOGIN READ");
    wrefresh(wnd0);
    wnd = newwin(5, 23, 2, 22);
    wbkgd(wnd, COLOR_PAIR(1));
    wattron(wnd, A_BOLD);
    keypad(wnd, TRUE);
    wprintw(wnd, "PASSWORD:\n");
    get_password(wnd, password, MAX_LEN, 1);
    wattron(wnd, A_BOLD);
    wprintw(wnd, "PASSWORD READ");
    wrefresh(wnd);
    delwin(wnd);

	while((c = getch()) != KEY_F(1))
	{   switch(c)
	    {	case KEY_LEFT:
				menu_driver(my_menu, REQ_LEFT_ITEM);
				break;
		    case KEY_RIGHT:
				menu_driver(my_menu, REQ_RIGHT_ITEM);
				break;
            case 10: /* Enter */
                move(20, 0);
                clrtoeol();
            //    mvprintw(20, 0, "Item selected is : %s", item_name(current_item(my_menu)));
                move(18, 0);
                if (strstr(item_name(current_item(my_menu)),"in") == NULL) {
                    mvprintw(18, 0, "Sorry registration temporary unavailable\n");
                    wrefresh(my_menu_win);
                } else {
                   mvprintw(18, 0, "Trying to connect\n");
                   unpost_menu(my_menu);
                   free_menu(my_menu);
                   for(i = 0; i < n_choices; ++i)
                   free_item(my_items[i]);
                   endwin();
                   return (0);
                }
                pos_menu_cursor(my_menu);
                break;
		}
               wrefresh(my_menu_win);
	}	
    wrefresh(my_menu_win);
    unpost_menu(my_menu);
    free_menu(my_menu);
    for(i = 0; i < n_choices; ++i) {
        free_item(my_items[i]);
    }
	endwin();
    return(0);
}

int out_msg_create (msg* mptr, char* login, char* string, int k) {
    int msg_length = 0,
               tmp = 0;
    strcpy(mptr->version,CUR_VERSION);
    strcpy(mptr->from, login);
    strcpy(mptr->to, "SERV");
    mptr->comm = k;
    memset(mptr->msg_itself, 0, sizeof(mptr->msg_itself));
    strcpy(mptr->msg_itself, string);
    msg_length =  strlen(mptr->from) + strlen(mptr->to)
               + strlen(mptr->msg_itself) + strlen(mptr->version) + 15;  //+1 ??
    tmp = len_int (msg_length);
    mptr->len = msg_length + tmp;

    return(0);
}


int main(int argc, const char *argv[]) {
    int port =  PORT;
    msg* ptr = (msg*) calloc (1, BUF_SIZE * 2);
    char login[MAX_LEN];
    char password[MAX_LEN];

    WINDOW *top;
    WINDOW *bottom;
    WINDOW *right;
    WINDOW *low;

    int line = 1; // Line position of top
    int input = 1; //  of bottom
    int lineright = 1; // on  online users list
    int linelow = 1; // on the lowest win for log scrolling
    int maxx,maxy; // Screen dimensions

    char* enter = calloc(1, BUF_SIZE);
    char* log_enter = calloc(1, BUF_SIZE);
    
    login_screen (login,password);
    char sbuffer[BUF_SIZE], rbuffer[BUF_SIZE];
    memset(sbuffer, 0, BUF_SIZE);
    memset(rbuffer, 0, BUF_SIZE);

    struct epoll_event event, events[MAX_EVENTS];
    int i, num_ready, bytes_read;
    int running = 1;
    int com = 1;
    int bsize = BUF_SIZE;
    memset(enter,0, sizeof(enter));
    int epfd = epoll_create1(0);
    if (epfd == -1) err_exit ("epoll_create");
    int sock = create_socket(port);

    initscr();  
    getmaxyx(stdscr,maxy,maxx);
    top = newwin(maxy/2, maxx - maxx/5, 0, 0);
    bottom = newwin(maxy/4, maxx - maxx/5, maxy /2, 0);
    right = newwin(maxy, maxx, 0, maxx - maxx/5);
    low = newwin(maxy - maxy/4 -maxy/2, maxx - maxx/5, maxy * 3/4, 0);

    scrollok(top,TRUE);
    scrollok(bottom,TRUE);

    box(top,'|','=');
//    box(bottom,'|',' ');
    box(right,'|','-');
    box(low,'|','=');

    wsetscrreg(top, 2, maxy/2 - 2);
    wsetscrreg(bottom, maxy/2 + 1, maxy * 3/4 - 2);

    start_color();
    cbreak();
    echo();
    keypad(stdscr, TRUE);
    
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    refresh();

    event.events = EPOLLIN;
    event.data.fd = sock;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event)) err_exit (" socket descriptor");

    event.events = EPOLLIN;
    event.data.fd = 0;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event)) err_exit (" stdin descriptor");
    
    out_msg_create (ptr, login, password, 0);
    format_msg(ptr);
    strcpy(enter, ptr->message_str);
    strcpy(sbuffer, enter);
    memset(enter,0, bsize);
    send (sock, sbuffer, strlen(sbuffer) + 1,0);
    bytes_read = read (sock, rbuffer, BUF_SIZE);
    rbuffer[bytes_read] = '\0';
    read_msg(rbuffer, ptr);
    if (strstr(ptr->msg_itself,"Not found") != NULL) {
        printf("%s\n", "Please check you autorising info and rerun your client");
        sleep(2);
        endwin();
        return -1;
    }
    memset (enter, 0, bsize);
    strcat (enter, ptr->msg_itself);
    memset(rbuffer, 0, bsize);
    wattron(top, COLOR_PAIR(1));
    mvwprintw(top,line,3,enter);
    wattroff(top,COLOR_PAIR(1));
    if(line != maxy/2 - 2) {
        line++;
    } else {
        scroll(top);
    }
    bytes_read = read (sock, rbuffer, BUF_SIZE);
    rbuffer[bytes_read] = '\0';
    read_msg(rbuffer, ptr);
    memset (enter, 0, bsize);
    strcat (enter, ptr->msg_itself);
    if (ptr->comm == 6) {
        werase(right);
        wattron(right,COLOR_PAIR(2));
        mvwprintw(right, lineright, 5, enter);
        mvwprintw(right, maxy - 5, 1, "type /LOG to see log");
        mvwprintw(right, maxy - 4, 1, "type /EXIT to exit ");
        wattroff(right,COLOR_PAIR(2));
        box(right,'|','-');
        wrefresh(right);
    }
    while (running) {
        wrefresh(top);
        wrefresh(bottom);
        wrefresh(right);
        wrefresh(low);

        num_ready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (i = 0; i < num_ready; i++) {
            if (0 != events[i].data.fd) {
                memset (rbuffer, 0, bsize);
                bytes_read = read (events[i].data.fd, rbuffer, bsize);
                rbuffer[bytes_read] = '\0';
                read_msg(rbuffer, ptr);
                memset (enter, 0, bsize);
                strcpy (enter, ptr->msg_itself);
                if (ptr->comm == 6) {
                    werase(right);
                    wattron(right,COLOR_PAIR(2));
                    mvwprintw(right, lineright, 5, enter);
                    mvwprintw(right, maxy - 5, 1, "type /LOG to see log");
                    mvwprintw(right, maxy - 4, 1, "type /EXIT to exit");
                  //  wrefresh(right);
                    wattroff(right,COLOR_PAIR(2));
                    box(right,'|','-');
                    wrefresh(right);
                } else if (ptr->comm == 3) {
                    do {
                        bytes_read = read (events[i].data.fd, enter, BUF_SIZE);
                        if(bytes_read >= 0) {
                            read_msg(enter, ptr);
                            mvwprintw(low, linelow, 1, ptr->msg_itself);
                            wrefresh(low);
                            if(linelow < maxy - 3) {
                                linelow++;
                            } else {
                                sleep(2);
                                werase(low);
                                box(low,'|','=');
                                linelow = 1;
                                wrefresh(low);
                            }
                        }
                    } while((strstr(enter,"/ENDLOG") == NULL));
                    wrefresh(low);
                    wrefresh(bottom);
                } else {
                    if ((strstr(ptr->msg_itself,"s the chat") != NULL)
                       || (strstr(ptr->msg_itself,"your daemon speaking") != NULL)) {
                        wattron(top,COLOR_PAIR(2));
                        mvwprintw(top, line, 8, enter);
                    } else {
                        mvwprintw(top, line, 3, enter);
                    }
                    wattroff(top,COLOR_PAIR(2));
                    if(line != maxy/2 - 2) {
                        line++;
                    } else {
                        scroll(top);
                    }  
                    wrefresh(top);
                }
            } else {
                memset (sbuffer, 0, bsize);
                memset (enter, 0, bsize);
                mvwgetstr(bottom, input, 2, enter);
                com = 1;
                if (!strcmp( enter, "/CLEARLOG")) {
                    werase(low);
                    box(low,'|','=');
                    linelow = 1;
                    wrefresh(low);
                   goto skipsend;
                }

                if (!strcmp( enter, "/LOG")) com = 3;
                if (!strcmp( enter, "/EXIT")) com = 4;
                out_msg_create (ptr, login, enter, com);
                format_msg(ptr);
                strcpy(enter, ptr->message_str);
                strcpy(sbuffer, enter);
                send (sock, sbuffer, strlen(sbuffer) + 1, 0);
skipsend:
                memset(enter,0, bsize);
                if(input != maxy/4 - 2)
                    input++;
                else
                    scroll(bottom);
                wrefresh(top);
                wrefresh(bottom);
                wrefresh(right);

                if (com == 4) {
                    sleep(3);
                    endwin();
                    return -1;
                }

            }
        }
    }
    endwin();
    free(enter);
    return 0;
}


