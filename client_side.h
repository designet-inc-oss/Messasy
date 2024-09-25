/*
 * messasy
 *
 * Copyright (C) 2006-2024 DesigNET, INC.
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
 */

#ifndef _CLIENT_SIDE_H_
#define _CLIENT_SIDE_H_

/* 定数定義 */
#define LOGIN_STATE_NONE     0x0         /* 初期接続時のステータス */
#define LOGIN_STATE_AUTH     0x1         /* ログインで認証成功後のステータス */

#define MAX_CMD_LEN          128         /* コマンドライン最大長 */
#define MANAGER_NAME "messasyctl"        /* 管理コンソールの名前 */
#define NONE                   0
#define CR_FOUND               1
#define CRLF_FOUND             2

#define R_SYNTAX_ERROR         -101
#define R_TIMEOUT              -102
#define R_EOF                  -103
#define R_TOOLONG              -104

#define STATE_NONQUIT          0
#define STATE_QUIT             1

/* メッセージ定義 */
#define BANNER               "Welcome to Messasy (Version " PACKAGE_VERSION ")\r\n"
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

/* isblank の有効化 */
#ifndef isblank
    int isblank(int);
#endif

/* 管理コントロール構造体 */
struct manager_control {
    int              mc_so;       /* ソケット */
    char            *mc_dest;     /* 接続クライアントのIPアドレス */
    int              mc_state;    /* 接続ユーザの状態 */
};

/* 管理コマンド構造体 */
struct manager_command {
    char *dc_command;                                 /* コマンド名 */
    int (*dc_func)(struct manager_control *, char *); /* コマンド処理関数 */
};

/* プロトタイプ宣言 */
extern void *manager_main(void *);

#endif // _CLIENT_SIDE_H_
