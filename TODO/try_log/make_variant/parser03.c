#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hchat03.h"

int len_int(int number) {
    int result = 0;
    while (number != 0) {
        number /= 10;
        result++;
    }
    return result;
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
    int count = 0;
    char *len_all;
recalc:
    char *message_str = calloc (1,BUF_SIZE);
    if( NULL != (len_all = malloc(5))) sprintf (len_all, "%d",aptr[i].len);
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
        count++;
        if (count == 1) {
            aptr[i].len = len;
            goto recalc;
        } else {
            err_scream ("outgoing length");
            printf("real length %d  differs from counted value %d oops\n", len, aptr[i].len);
            printf("string is  %s\n", message_str);
            return("");
        }
    }
}


int len_count( msg* mess, int k) {
    int msg_length =  strlen(mess[k].from) + strlen(mess[k].to)
                   + strlen(mess[k].msg_itself) + strlen(mess[k].version) + 15;  //+1 ??
    int tmp = len_int (msg_length);
    mess[k].len = msg_length + tmp;
    return (mess[k].len);
}


int read_msg(char *rcv_msg, msg* msgptr1, int i) {
/*  tears the received string apart*/
    int ctrl_len = 0,
        temp_len = 0;
    char *start = NULL;
    char *fin = NULL;
    char *temp = NULL;
    ctrl_len = strlen (rcv_msg);
    if (!ctrl_len) {
        err_scream ("incoming nullstring");
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
        if ((ctrl_len - temp_len) != 0) {
            err_scream ("incoming length");
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
            free(temp);
            return(1);
        }
    }
}

