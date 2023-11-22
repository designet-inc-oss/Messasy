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
 * $RCSfile: client_side.h,v $
 * $Revision: 1.7 $
 * $Date: 2009/10/22 03:17:04 $
 */

#ifndef _CLIENT_SIDE_H_
#define _CLIENT_SIDE_H_

/* ������ */
#define LOGIN_STATE_NONE     0x0         /* �����³���Υ��ơ����� */
#define LOGIN_STATE_AUTH     0x1         /* �������ǧ��������Υ��ơ����� */

#define MAX_CMD_LEN          128         /* ���ޥ�ɥ饤�����Ĺ */
#define MANAGER_NAME "messasyctl"        /* �������󥽡����̾�� */
#define NONE                   0
#define CR_FOUND               1
#define CRLF_FOUND             2

#define R_SYNTAX_ERROR         -101
#define R_TIMEOUT              -102
#define R_EOF                  -103
#define R_TOOLONG              -104

#define STATE_NONQUIT          0
#define STATE_QUIT             1

/* ��å�������� */
#define BANNER               "Welcome to Messasy (Version " VERSION ")\r\n"
#define TOO_LONG_STRING      "-NG command too long\r\n"
#define SYNTAX_ERR_STRING    "-NG command syntax error\r\n"
#define AUTH_ERR_STRING      "-NG authentication error\r\n"
#define UNKNOWN_STRING       "-NG unknown command\r\n"
#define NG_STRING            "-NG\r\n"
#define MANY_CONNECT_STRING  "-NG Too many connections\r\n"
#define GOODBY_STRING        "+OK good-bye\r\n"
#define OK_STRING            "+OK\r\n"
#define OK_RELOAD_STRING     "+OK reloaded successfully\r\n"
#define OK_LOGIN_STRING      "+OK logged in successfully\r\n"
#define OK_ALREADY_LOGIN_STRING  "+OK already logged in\r\n"

#define IS_AUTH(mc)    (mc->mc_state & LOGIN_STATE_AUTH)

/* isblank ��ͭ���� */
#ifndef isblank
    int isblank(int);
#endif

/* ��������ȥ��빽¤�� */
struct manager_control {
    int              mc_so;       /* �����å� */
    char            *mc_dest;     /* ��³���饤����Ȥ�IP���ɥ쥹 */
    int              mc_state;    /* ��³�桼���ξ��� */
};

/* �������ޥ�ɹ�¤�� */
struct manager_command {
    char *dc_command;                                 /* ���ޥ��̾ */
    int (*dc_func)(struct manager_control *, char *); /* ���ޥ�ɽ����ؿ� */
};

/* �ץ�ȥ�������� */
extern void *manager_main(void *);

#endif // _CLIENT_SIDE_H_
