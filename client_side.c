/*
 * messasy
 *
 * Copyright (C) 2006,2007,2008,2009 DesigNET, INC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * $RCSfile: client_side.c,v $
 * $Revision: 1.16 $
 * $Date: 2009/10/28 01:20:07 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <pthread.h>
#include <libmilter/mfapi.h>
#include <regex.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef __HAVE_LIBWRAP
#include <tcpd.h>
#endif // __HAVE_LIBWRAP

#include <libdgstr.h>
#include <libdgconfig.h>

/* add included header for make */
//#include "config.h"

#include "log.h"
#include "msy_config.h"
#include "messasy.h"
#include "client_side.h"

/* �ץ�ȥ�������� */
static int parse_arg(char *, char **, int);
static int manager_login(struct manager_control *, char *);
static int manager_quit(struct manager_control *, char *);
static int manager_reload(struct manager_control *, char *);
static void read_dust(int, int);
static int read_line(int, char *);
static void * request_handler(void *);
static int check_crlf(char, int);
static void increment_tc(void);
static void decrement_tc(void);

/* �������ޥ�ɤμ������ */
struct manager_command manager_command[] = {
   { "LOGIN",  manager_login},
   { "RELOAD", manager_reload},
   { "QUIT",   manager_quit},
};

/* �������ޥ�ɹ�¤�Τ��礭������ */
#define NUM_DAEMON_COMMAND \
               (sizeof(manager_command) / sizeof(struct manager_command))

/* ����åɵ�ư�������� */
static unsigned int thread_count;
static pthread_mutex_t tc_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * increment_tc
 *
 * ��ǽ
 *      ����åɥ����󥿤Υ��󥯥����
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      �ʤ�
 */
static void
increment_tc(void)
{
    pthread_mutex_lock(&tc_lock);
    thread_count++;
    pthread_mutex_unlock(&tc_lock);
}

/*
 * decrement_tc
 *
 * ��ǽ
 *      ����åɥ����󥿤Υǥ������
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      �ʤ�
 */
static void
decrement_tc(void)
{
    pthread_mutex_lock(&tc_lock);
    thread_count--;
    pthread_mutex_unlock(&tc_lock);
}

/*
 * parse_arg
 *
 * ��ǽ
 *      ���ϥ��ޥ�ɤΰ�������Ϥ���
 *
 * ����
 *      *string   �����оݤ�ʸ����
 *      *array[]  ���ϸ��ʸ�����Ǽ�ѿ�
 *      *num      ���Ƥ�������ο�
 *
 * �֤���
 *     count: �����ʸ�������
 */
static int
parse_arg(char *string, char *array[], int num)
{
    int i = 0;
    int count = 0;

    while (count < num) {
        /* ��Ƭ�ζ�ʸ�����ɤ����Ф� */
        for (; isblank((int)string[i]); i++);

        if (string[i] == '\0') {
            return (count);
        }
        array[count++] = &string[i];

        /* �����γ�Ǽ */
        while (!isblank(string[i])) {
            if (string[i] == '\0') {
                return (count);
            }
            i++;
        }
        string[i++] = '\0';
    }

    return (count + 1);
}

/*
 * manager_login
 *
 * ��ǽ
 *      login���ޥ�ɤ��������
 *
 * ����
 *      *mc            ��������ȥ��빽¤��
 *      *arg           �������Ϥ��줿ʸ����
 *
 * �֤���
 *      R_SUCCESS      ����������
 *      R_SYNTAX_ERROR login���ޥ�ɤν񼰥��顼
 *      R_ERROR        ǧ�ڤ˼���
 */
static int
manager_login(struct manager_control *mc, char *arg)
{
    int            ret;
    char          *param[2];
    struct config *cfg;

    ret = parse_arg(arg, param, 2);

    switch (ret) {
        case 1:
            /* ��������� */
            break;
        case 2:
            if (param[1] == '\0') {
                /* ��������ġ�����ܤΰ���������Τߤ�OK */
                break;
            }
        default:
            /* �񼰥��顼 */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
            break;
    }

    /* ����ǧ�ںѤ� */
    if (mc->mc_state &= LOGIN_STATE_AUTH) {
        write(mc->mc_so, OK_ALREADY_LOGIN_STRING, sizeof(OK_ALREADY_LOGIN_STRING) - 1);

        return (R_SUCCESS);
    }

    cfg = config_retrieve();

    if (strcmp(cfg->cf_adminpassword, param[0]) != 0) {
        write(mc->mc_so, AUTH_ERR_STRING, sizeof(AUTH_ERR_STRING) - 1);
        config_release(cfg);
        return (R_ERROR);
    }

    config_release(cfg);

    write(mc->mc_so, OK_LOGIN_STRING, sizeof(OK_LOGIN_STRING) - 1);

    /* ǧ�ںѤߥ��ơ���������Ϳ */
    mc->mc_state |= LOGIN_STATE_AUTH;

    return (R_SUCCESS);
}

/*
 * manager_quit
 *
 * ��ǽ
 *      quit���ޥ�ɤ��������
 *
 * ����
 *      *mc           ��������ȥ��빽¤��
 *      *arg          �������Ϥ��줿ʸ����
 *
 * �֤���
 *     R_ERROR        ����ʥ�������������
 *     R_SYNTAX_ERROR �񼰥��顼
 */
static int
manager_quit(struct manager_control *mc, char *arg)
{
    int    ret;
    char  *param[1];

    ret = parse_arg(arg, param, 1);

    switch (ret) {
        case 0:
            /* �������ʤ� */
            break;
        case 1:
            if (param[0] == '\0') {
                /* ������1�ġ�1���ܤΰ���������Τߤ�OK */
                break;
            }
        default:
            /* �񼰥��顼 */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
    }

    /* write �˼��Ԥ��Ƥ⽪λ */
    write(mc->mc_so, GOODBY_STRING, sizeof(GOODBY_STRING) - 1);
    return (R_ERROR);
}

/*
 * manager_reload
 *
 * ��ǽ
 *      reload���ޥ�ɤ��������
 *
 * ����
 *      *mc            ��������ȥ��빽¤��
 *      *arg           �������Ϥ��줿ʸ����
 *
 * �֤���
 *      R_SUCCESS      ����
 *      R_SYNTAX_ERROR �񼰥��顼
 *      R_ERROR        �۾�
 */
static int
manager_reload(struct manager_control *mc, char *arg)
{
    int    ret;
    char  *param[1];

    ret = parse_arg(arg, param, 1);

    switch (ret) {
        case 0:
            /* �������ʤ� */
            break;
        case 1:
            if (param[0] == '\0') {
                /* ������1�ġ�1���ܤΰ���������Τߤ�OK */
                break;
            }
        default:
            /* �񼰥��顼 */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
            break;
    }

    /* ǧ�ھ��֤γ�ǧ */
    if (!IS_AUTH(mc)) {
        write(mc->mc_so, AUTH_ERR_STRING, sizeof(AUTH_ERR_STRING) - 1);
        return (R_ERROR);
    }

    /* ����ե�����Υ���ɽ��� */
    ret = reload_config();
    if (ret != R_SUCCESS) {
        write(mc->mc_so, NG_STRING, sizeof(NG_STRING) - 1);
        return (R_ERROR);
    }

    write(mc->mc_so, OK_RELOAD_STRING, sizeof(OK_RELOAD_STRING) - 1);
    return (R_SUCCESS);
}

/*
 * check_crlf
 *
 * ��ǽ
 *      CRLF�Υ����å���Ԥ�
 * ����
 *      c     �����å�ʸ��
 *      state ���ơ�����
 * �֤���
 *      state ���ơ���������
 *        CR_FOUND   CR
 *        CRLF_FOUND CRLF
 *        NONE       CRLF�ʳ�
 */
static int
check_crlf (char c, int state)
{
    switch (c) {
        case '\r':
            return (CR_FOUND);
            break;
        case '\n':
            if (state == CR_FOUND) {
                return (CRLF_FOUND);
            }
        default:
            return (NONE);
            break;
    }
}

/*
 * read_dust
 *
 * ��ǽ
 *      read�λĳ������
 *
 * ����
 *      fd    �ե�����ǥ�������ץ�
 *
 * �֤���
 *     �ʤ�
 */
static void
read_dust(int fd, int state)
{
    char dust;
    int  readsize;

    /* \r\n �����ޤ�read���� */
    while (state != CRLF_FOUND) {
        readsize = read(fd, &dust, 1);
        if (readsize <= 0) {
            return;
        }

        state = check_crlf(dust, state);
    }
}

/*
 * read_line
 *
 * ��ǽ
 *      1���ɤ߹���
 *
 * ����
 *      fd            �ե�����ǥ�������ץ�
 *     *buf           �񤭹��ߥХåե�
 *
 * �֤���
 *      R_SUCCESS     ����
 *      R_TIMEOUT     �����ॢ����
 *      R_ERROR       read���顼
 *      R_EOF         socket��close
 *      R_TOOLONG     ʸ����Ĺ������
 */
static int
read_line (int fd, char *buf)
{
    int len;
    int state;
    int readsize;

    for (len = state = 0; len < MAX_CMD_LEN + 2 && state != CRLF_FOUND; buf++, len++) {
        readsize = read(fd, buf, 1);
        if (readsize < 0) {
            /* �����ॢ���� */
            if (errno == EWOULDBLOCK) {
                return (R_TIMEOUT);
            }
            /* ����ʳ��Υ��顼 */
            return (R_ERROR);
        }
        if (readsize == 0) {
            return (R_EOF);
        }

        state = check_crlf(*buf, state);
        if (state == CRLF_FOUND) {
            buf--;
            len--;
            break;
        }
    }

    *buf = '\0';

    if (state != CRLF_FOUND) {
        /* ����ʸ����Ĺ������ */
        read_dust(fd, state);
        return (R_TOOLONG);
    }

    return len;
}

/*
 * request_handler
 *
 * ��ǽ
 *      �������󥿥ե������ȤΤ�����Ԥ�
 *
 * ����
 *      *arg      ��������ȥ��빽¤��(void *��)
 *
 * �֤���
 *      �ʤ�
 */
static void *
request_handler(void *arg)
{
    int                     ret;
    int                     readsize;
    char                    readbuf[MAX_CMD_LEN + 2];
    char                   *bufp;
    int                     i;
    int                     len;
    int                     quit = STATE_NONQUIT;
    struct manager_control *mc;

    mc = (struct manager_control *)arg;

    while (quit == STATE_NONQUIT) {
        readsize = read_line(mc->mc_so, readbuf);
        if (readsize == R_TIMEOUT) {
            SYSLOGERROR(ERR_READ_SOCKTIMEO, mc->mc_dest);
            break;
        }
        if (readsize == R_ERROR) {
            SYSLOGERROR(ERR_READ_SOCK, mc->mc_dest, E_STR);
            break;
        }
        if (readsize == R_EOF) {
            break;
        }
        if (readsize == R_TOOLONG){
            write(mc->mc_so, TOO_LONG_STRING, sizeof(TOO_LONG_STRING) - 1);
            continue;
        }

        /* ��Ƭ�ζ�ʸ�����ɤ����Ф� */
        for (bufp = readbuf; isblank((int)*bufp) && *bufp != '\0'; bufp++);

        /* �б����ޥ������ */
        for (i = 0; i < NUM_DAEMON_COMMAND; i++) {
            len = strlen(manager_command[i].dc_command);
            if ((strncasecmp(manager_command[i].dc_command, bufp, len) == 0) &&
                (isblank((int)*(bufp + len)) || *(bufp + len) == '\0'))  {
                ret = (*manager_command[i].dc_func)(mc, bufp + len);
                if (ret == R_ERROR) {
                    quit = STATE_QUIT;
                }
                break;
            }
        }

        /* �б����ޥ�ɤ�¸�ߤ��ʤ� */
        if (i == NUM_DAEMON_COMMAND) {
             write(mc->mc_so, UNKNOWN_STRING, sizeof(UNKNOWN_STRING) - 1);
        }
    }

    /* �������󥿥ե��������ѿ��򸺤餹 */
    decrement_tc();

    close(mc->mc_so);
    free(mc->mc_dest);
    free(mc);

    ret = 0;
    pthread_exit(&ret);
    return (NULL);
}

/*
 * manager_main
 *
 * ��ǽ
 *      �������󥿥ե������Υᥤ�����
 *
 * ����
 *      *arg    listen�����å�
 *
 * �֤���
 *      �ʤ�
 */
void *
manager_main(void *arg)
{
    int                    so;
    int                    on;
    int                    ret;
    struct manager_control *mc;

    int                    nfds;
    socklen_t              slen;
    int                    fd;
    struct sockaddr_in     addr;
    char                  *client;
    struct config         *cfg;
    pthread_t             child;
    struct timeval        tv;

    char f_name[] = "manager_main";

    so = (int)arg;

    nfds = so + 1;
    slen = sizeof(struct sockaddr_in);

    while (1) {
        fd = accept(so, (struct sockaddr *)&addr, &slen);
        if (fd < 0) {
            continue;
        }

        /* ���饤�����IP����¸ */
        client = strdup(inet_ntoa(addr.sin_addr));
        if (client == NULL) {
            SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
            exit(EXIT_MANAGER);
        }

#ifdef __HAVE_LIBWRAP
        /* TCP_wrapper �Υ����å� */
        ret = hosts_ctl(MANAGER_NAME, STRING_UNKNOWN, client, STRING_UNKNOWN);
        if (ret == 0) {
            SYSLOGERROR(ERR_HOSTS_CTL, inet_ntoa(addr.sin_addr));
            close(fd);
            free(client);
            continue;
        }
#endif // __HAVE_LIBWRAP

        cfg = config_retrieve();

        if (thread_count >= cfg->cf_commandmaxclients) {
            /* ��³����Ķ���Ƥ���� */
            SYSLOGERROR(ERR_MANY_CONNECT, thread_count, cfg->cf_commandmaxclients);
            pthread_mutex_unlock(&tc_lock);
            config_release(cfg);
            free(client);
            write(fd, MANY_CONNECT_STRING, sizeof(MANY_CONNECT_STRING) - 1);
            close(fd);

            continue;
        }

        /* �������󥿥ե��������ѿ������䤹 */
        increment_tc();

        tv.tv_sec = cfg->cf_commandtimeout;
        tv.tv_usec = 0;

        config_release(cfg);

        /* KEEPALIVE������ */
        ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
        if (ret != 0) {
            SYSLOGERROR(ERR_SETSOCK_KEEP, E_STR);
        }

        /* �����ॢ���Ȥ����� */
        ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (ret != 0) {
            SYSLOGERROR(ERR_SETSOCK_RCVTIMEO, E_STR);
        }
        ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (ret != 0) {
            SYSLOGERROR(ERR_SETSOCK_SNDTIMEO, E_STR);
        }

        mc = malloc(sizeof(struct manager_control));
        if (mc == NULL) {
            SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
            exit(EXIT_MANAGER);
        }

        mc->mc_so    = fd;
        mc->mc_dest  = client;
        mc->mc_state = LOGIN_STATE_NONE;

        /* Welcome ��å����������� */
        write(fd, BANNER, sizeof(BANNER) - 1);

        /* ���ޥ�ɽ��� */
        ret = pthread_create(&child, NULL, request_handler, (void *)mc);
        if (ret != 0) {
            SYSLOGERROR(ERR_THREAD_CREATE, f_name, E_STR);
            close(fd);
            close(so);
            free(client);

            exit(EXIT_MANAGER);
        }
        pthread_detach(child);
    }

    /* ��ã���ʤ� */
    close (so);
    ret = 0;
    pthread_exit(&ret);
}
