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

#include "log.h"
#include "msy_config.h"
#include "messasy.h"
#include "client_side.h"

/* プロトタイプ宣言 */
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

/* 管理コマンドの実態定義 */
struct manager_command manager_command[] = {
   { "LOGIN",  manager_login},
   { "RELOAD", manager_reload},
   { "QUIT",   manager_quit},
};

/* 管理コマンド構造体の大きさ取得 */
#define NUM_DAEMON_COMMAND \
               (sizeof(manager_command) / sizeof(struct manager_command))

/* スレッド起動数カウンタ */
static unsigned int thread_count;
static pthread_mutex_t tc_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * increment_tc
 *
 * 機能
 *      スレッドカウンタのインクリメント
 *
 * 引数
 *      なし
 *
 * 返り値
 *      なし
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
 * 機能
 *      スレッドカウンタのデクリメント
 *
 * 引数
 *      なし
 *
 * 返り値
 *      なし
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
 * 機能
 *      入力コマンドの引数を解析する
 *
 * 引数
 *      *string   解析対象の文字列
 *      *array[]  解析後の文字列格納変数
 *      *num      許容する引数の数
 *
 * 返り値
 *     count: 正常（文字列数）
 */
static int
parse_arg(char *string, char *array[], int num)
{
    int i = 0;
    int count = 0;

    while (count < num) {
        /* 先頭の空文字を読み飛ばす */
        for (; isblank((int)string[i]); i++);

        if (string[i] == '\0') {
            return (count);
        }
        array[count++] = &string[i];

        /* 引数の格納 */
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
 * 機能
 *      loginコマンドを処理する
 *
 * 引数
 *      *mc            管理コントロール構造体
 *      *arg           引数で渡された文字列
 *
 * 返り値
 *      R_SUCCESS      ログイン成功
 *      R_SYNTAX_ERROR loginコマンドの書式エラー
 *      R_ERROR        認証に失敗
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
            /* 引数が一つ */
            break;
        case 2:
            if (*param[1] == '\0') {
                /* 引数が二つ、二つ目の引数が空白のみはOK */
                break;
            }
        default:
            /* 書式エラー */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
            break;
    }

    /* 既に認証済み */
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

    /* 認証済みステータスを付与 */
    mc->mc_state |= LOGIN_STATE_AUTH;

    return (R_SUCCESS);
}

/*
 * manager_quit
 *
 * 機能
 *      quitコマンドを処理する
 *
 * 引数
 *      *mc           管理コントロール構造体
 *      *arg          引数で渡された文字列
 *
 * 返り値
 *     R_ERROR        正常（ログアウト成功）
 *     R_SYNTAX_ERROR 書式エラー
 */
static int
manager_quit(struct manager_control *mc, char *arg)
{
    int    ret;
    char  *param[1];

    ret = parse_arg(arg, param, 1);

    switch (ret) {
        case 0:
            /* 引数がなし */
            break;
        case 1:
            if (*param[0] == '\0') {
                /* 引数が1つ、1つ目の引数が空白のみはOK */
                break;
            }
        default:
            /* 書式エラー */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
    }

    /* write に失敗しても終了 */
    write(mc->mc_so, GOODBY_STRING, sizeof(GOODBY_STRING) - 1);
    return (R_ERROR);
}

/*
 * manager_reload
 *
 * 機能
 *      reloadコマンドを処理する
 *
 * 引数
 *      *mc            管理コントロール構造体
 *      *arg           引数で渡された文字列
 *
 * 返り値
 *      R_SUCCESS      正常
 *      R_SYNTAX_ERROR 書式エラー
 *      R_ERROR        異常
 */
static int
manager_reload(struct manager_control *mc, char *arg)
{
    int    ret;
    char  *param[1];

    ret = parse_arg(arg, param, 1);

    switch (ret) {
        case 0:
            /* 引数がなし */
            break;
        case 1:
            if (*param[0] == '\0') {
                /* 引数が1つ、1つ目の引数が空白のみはOK */
                break;
            }
        default:
            /* 書式エラー */
            write(mc->mc_so, SYNTAX_ERR_STRING, sizeof(SYNTAX_ERR_STRING) - 1);
            return (R_SYNTAX_ERROR);
            break;
    }

    /* 認証状態の確認 */
    if (!IS_AUTH(mc)) {
        write(mc->mc_so, AUTH_ERR_STRING, sizeof(AUTH_ERR_STRING) - 1);
        return (R_ERROR);
    }

    /* 設定ファイルのリロード処理 */
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
 * 機能
 *      CRLFのチェックを行う
 * 引数
 *      c     チェック文字
 *      state ステータス
 * 返り値
 *      state ステータス情報
 *        CR_FOUND   CR
 *        CRLF_FOUND CRLF
 *        NONE       CRLF以外
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
 * 機能
 *      readの残骸を除去
 *
 * 引数
 *      fd    ファイルディスクリプタ
 *
 * 返り値
 *     なし
 */
static void
read_dust(int fd, int state)
{
    char dust;
    int  readsize;

    /* \r\n が来るまでreadする */
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
 * 機能
 *      1行読み込む
 *
 * 引数
 *      fd            ファイルディスクリプタ
 *     *buf           書き込みバッファ
 *
 * 返り値
 *      R_SUCCESS     正常
 *      R_TIMEOUT     タイムアウト
 *      R_ERROR       readエラー
 *      R_EOF         socketがclose
 *      R_TOOLONG     文字列が長すぎる
 */
static int
read_line (int fd, char *buf)
{
    int len;
    int state;
    int readsize;
    char *mv = buf; 

    for (len = state = 0; len < MAX_CMD_LEN + 1 && state != CRLF_FOUND; mv++, len++) {
        readsize = read(fd, mv, 1);
        if (readsize < 0) {
            /* タイムアウト */
            if (errno == EWOULDBLOCK) {
                return (R_TIMEOUT);
            }
            /* それ以外のエラー */
            return (R_ERROR);
        }
        if (readsize == 0) {
            return (R_EOF);
        }

        state = check_crlf(*mv, state);
        if (state == CRLF_FOUND) {
            mv--;
            len--;
            break;
        }
    }

    *mv = '\0';

    if (state != CRLF_FOUND) {
        /* 入力文字列が長すぎる */
        read_dust(fd, state);
        return (R_TOOLONG);
    }

    return len;
}

/*
 * request_handler
 *
 * 機能
 *      管理インタフェースとのやり取りを行う
 *
 * 引数
 *      *arg      管理コントロール構造体(void *型)
 *
 * 返り値
 *      なし
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

        /* 先頭の空文字を読み飛ばす */
        for (bufp = readbuf; isblank((int)*bufp) && *bufp != '\0'; bufp++);

        /* 対応コマンド走査 */
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

        /* 対応コマンドが存在しない */
        if (i == NUM_DAEMON_COMMAND) {
             write(mc->mc_so, UNKNOWN_STRING, sizeof(UNKNOWN_STRING) - 1);
        }
    }

    /* 管理インタフェース使用数を減らす */
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
 * 機能
 *      管理インタフェースのメイン処理
 *
 * 引数
 *      *arg    listenソケット
 *
 * 返り値
 *      なし
 */
void *
manager_main(void *arg)
{
    intptr_t               so;
    int                    on = 1;
    int                    ret;
    struct manager_control *mc;

    socklen_t              slen;
    int                    fd;
    struct sockaddr_in     addr;
    char                  *client;
    struct config         *cfg;
    pthread_t             child;
    struct timeval        tv;

    char f_name[] = "manager_main";

    so = (intptr_t)arg;
    slen = sizeof(struct sockaddr_in);

    while (1) {
        fd = accept((int)so, (struct sockaddr *)&addr, &slen);
        if (fd < 0) {
            continue;
        }

        /* クライアントIPの保存 */
        client = strdup(inet_ntoa(addr.sin_addr));
        if (client == NULL) {
            SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
            exit(EXIT_MANAGER);
        }

#ifdef __HAVE_LIBWRAP
        /* TCP_wrapper のチェック */
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
            /* 接続数を超えている時 */
            SYSLOGERROR(ERR_MANY_CONNECT, thread_count, cfg->cf_commandmaxclients);
            pthread_mutex_unlock(&tc_lock);
            config_release(cfg);
            free(client);
            write(fd, MANY_CONNECT_STRING, sizeof(MANY_CONNECT_STRING) - 1);
            close(fd);

            continue;
        }

        /* 管理インタフェース使用数を増やす */
        increment_tc();

        tv.tv_sec = cfg->cf_commandtimeout;
        tv.tv_usec = 0;

        config_release(cfg);

        /* KEEPALIVEを設定 */
        ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
        if (ret != 0) {
            SYSLOGERROR(ERR_SETSOCK_KEEP, E_STR);
        }

        /* タイムアウトを設定 */
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

        /* Welcome メッセージの送信 */
        write(fd, BANNER, sizeof(BANNER) - 1);

        /* コマンド処理 */
        ret = pthread_create(&child, NULL, request_handler, (void *)mc);
        if (ret != 0) {
            SYSLOGERROR(ERR_THREAD_CREATE, f_name, E_STR);
            close(fd);
            close((int)so);
            free(client);

            exit(EXIT_MANAGER);
        }
        pthread_detach(child);
    }

    /* 到達しない */
    close ((int)so);
    ret = 0;
    pthread_exit(&ret);
}
