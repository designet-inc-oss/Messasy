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
#include <time.h>
#include <error.h>
#include <errno.h>
#include <pthread.h>
#include <libmilter/mfapi.h>
#include <regex.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>

#include "messasy.h"
#include "client_side.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "so/lib_lm.h"

#include "filter.h"
#include "utils.h"
#include "log.h"

/* グローバル変数 */
char msy_hostname[MAX_HOSTNAME_LEN + 1];
extern struct modulehandle *mhandle_list;
extern struct cfentry cfe;

#define MLFIPRIV        ((struct mlfiPriv *) smfi_getpriv(ctx))

/* コールバック関数一覧 */
struct smfiDesc smfilter =
{
    IDENT,
    SMFI_VERSION,       /* version code -- do not change */
    SMFIF_ADDHDRS,      /* flags */
    mlfi_connect,       /* connection info filter */
    NULL,               /* SMTP HELO command filter */
    mlfi_envfrom,       /* envelope sender filter */
    mlfi_envrcpt,       /* envelope recipient filter */
    mlfi_header,        /* header filter */
    NULL,               /* end of header */
    mlfi_body,          /* body block filter */
    mlfi_eom,           /* end of message */
    mlfi_abort,         /* message aborted */
    mlfi_close          /* connection cleanup */
};

/*
 * manager_init
 *
 * 管理インタフェースの起動
 *
 * 引数
 *      なし
 *
 * 返り値
 *      R_SUCCESS 正常
 *      R_ERROR   システムエラー
 */
int
manager_init(void)
{
    intptr_t               so;
    int                    on = 1;
    int                    ret;
    struct sockaddr_in     saddr;
    struct config          *cfg;
    pthread_t              manager;
    int                    backlog;

    char f_name[] = "manager_init";

    /* ソケットの定義 */
    so = (intptr_t)socket(AF_INET, SOCK_STREAM, 0);
    if (so < 0) {
        SYSLOGERROR(ERR_SOCK, E_STR);
        return (R_ERROR);
    }

    ret = setsockopt((int)so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret != 0) {
        SYSLOGERROR(ERR_SETSOCK_REUSE, E_STR);
    }

    cfg = config_retrieve();

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(cfg->cf_commandport);
    inet_aton(cfg->cf_listenip, &saddr.sin_addr);

    backlog = cfg->cf_commandmaxclients + 1;

    config_release(cfg);

    ret = bind((int)so, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret != 0) {
        SYSLOGERROR(ERR_BIND, E_STR);
        close(so);
        return (R_ERROR);
    }

    ret = listen((int)so, backlog);
    if (ret != 0) {
        SYSLOGERROR(ERR_LISTEN, E_STR);
        close(so);
        return (R_ERROR);
    }

    /* 管理インタフェースの起動 */
    ret = pthread_create(&manager, NULL, manager_main, (void*)so);
    if (ret != 0) {
        SYSLOGERROR(ERR_THREAD_CREATE, f_name, E_STR);
        close(so);
        return (R_ERROR);
    }

    return R_SUCCESS;
}

/*
 * mlfi_connect
 *
 * コールバック関数 (CONNECT)
 * - セッションIDを取得する
 * - 設定ファイルの内容を取得する
 * - プライベート領域を準備する
 */
sfsistat
mlfi_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr)
{
    struct mlfiPriv *priv;
    unsigned int s_id;
    struct config *config;
    int error_action;

    /* セッションIDを取得 */
    s_id = get_sessid();

    /* 設定ファイルの内容を取得 */
    config = config_retrieve();

    /* プライベート領域を確保 */
    priv = malloc(sizeof(struct mlfiPriv));
    if (priv == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "mlfi_connect", E_STR);
        exit(EXIT_MILTER);
    }
    memset(priv, 0, sizeof(struct mlfiPriv));
    priv->config = config;
    priv->mlfi_sid = s_id;
    error_action = config->cf_erroraction_conv;

    /* 受信日時を保存 */
    priv->mlfi_recvtime = time(NULL);
    if (priv->mlfi_recvtime < 0) {
        SYSLOGERROR(ERR_S_TIME, s_id, E_STR);
        config_release(config);
        free(priv);
        return error_action;
    }

    /* クライアントアドレス情報を保存 */
    memcpy(&priv->mlfi_clientaddr, hostaddr, sizeof(_SOCK_ADDR));

    /* プライベート領域をセット */
    smfi_setpriv(ctx, priv);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_envfrom
 *
 * コールバック関数 (MAIL FROM)
 * - Envelop Fromアドレスを保存する
 */
sfsistat
mlfi_envfrom(SMFICTX *ctx, char **envfrom)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    char *fromaddr;

    fromaddr = get_addrpart((unsigned char *) *envfrom);
    if (fromaddr == NULL) {
        /* メモリエラー */
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "get_addrpart", E_STR);
        exit(EXIT_MILTER);
    }
    strset_set(&(priv->mlfi_envfrom), fromaddr);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_envrcpt
 *
 * コールバック関数 (RCPT TO)
 * - Envelope Toアドレスを保存する
 */
sfsistat
mlfi_envrcpt(SMFICTX *ctx, char **rcptto)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    char *rcptaddr;

    /* Toアドレスを保存 */
    rcptaddr = get_addrpart((unsigned char *) *rcptto);

    if (rcptaddr == NULL) {
        /* メモリエラー */
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "get_addrpart", E_STR);
        exit(EXIT_MILTER);
    }
    push_strlist(&priv->mlfi_rcptto_h, &priv->mlfi_rcptto_t, rcptaddr);

    free(rcptaddr);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_header
 *
 * コールバック関数 (header)
 * - ヘッダチェック (SaveIgnoreHeader) を行なう
 * - 一時ファイルをオープンする
 * - ヘッダを書き込む
 */
sfsistat
mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    int error_action = priv->config->cf_erroraction_conv;
    int ret;
    
    /* ヘッダチェック */
    if (priv->config->cf_saveignoreheader_regex != NULL) {
        ret = check_header_regex(headerf, headerv,
                                    priv->config->cf_saveignoreheader_regex);
        if (ret == R_POSITIVE) {
            /* マッチしたので保存対象外として中止 */
            SYSLOGINFO(INFO_S_IGNOREHEADER, s_id, headerf);

            mlfi_abort(ctx);
            return SMFIS_ACCEPT;
        }
    }

    /* オープン */
    if (priv->header_existence == FALSE) {
        /* ヘッダが一度読み込まれた時 */
        priv->header_existence = TRUE;

        /* 保存対象アドレス一覧の作成 */
        ret = make_savelist(&priv->mlfi_envfrom, priv->mlfi_rcptto_h,
                            &priv->mlfi_addrmatched_h, &priv->mlfi_addrmatched_t,
                            priv->config, s_id);
        if (ret != R_SUCCESS) {
            mlfi_abort(ctx);
            return error_action;
        }

        /* 保存対象アドレスがなければ中止 */
        if (priv->mlfi_addrmatched_h == NULL) {
            SYSLOGINFO(INFO_S_NOSAVEADDRESS, s_id);
            mlfi_abort(ctx);
            return SMFIS_ACCEPT;
        }

        if (priv->header_existence == FALSE) {
            mlfi_abort(ctx);
            return error_action;
        }
    }

    /* モジュール内の関数を実行する関数の呼び出し */
    ret = msy_exec_header(priv, headerf, headerv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_body
 *
 * コールバック関数 (body)
 * - 本文を書き込む
 */
sfsistat
mlfi_body(SMFICTX *ctx, u_char *bodyp, size_t bodylen)
{
    struct mlfiPriv *priv = MLFIPRIV;

    int error_action = priv->config->cf_erroraction_conv;
    int ret;

    /* モジュール内の関数を実行する関数の呼び出し */
    ret = msy_exec_body(priv,bodyp, bodylen);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_eoh
 *
 * コールバック関数 (header終了)
 * - メール保存の終了処理を行なう
 * - リソースを解放する
 */
sfsistat
mlfi_eoh(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    int error_action = priv->config->cf_erroraction_conv;
    int ret;

    /* モジュール内の関数を実行する関数の呼び出し */
    ret = msy_exec_eoh(priv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_eom
 *
 * コールバック関数 (DATA終了)
 * - メール保存の終了処理を行なう
 * - リソースを解放する
 */
sfsistat
mlfi_eom(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;

    int error_action = priv->config->cf_erroraction_conv;
    int ret;

    /* モジュール内の関数を実行する関数の呼び出し */
    ret = msy_exec_eom(priv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return eom_cleanup(ctx);
}

/*
 * mlfi_abort
 *
 * コールバック関数 (RSET等)
 * - メール保存の中止処理を行なう
 * - リソースを解放する
 */
sfsistat
mlfi_abort(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;

    int error_action;
    int ret;

    /* モジュール内の関数を実行する関数の呼び出し */
    if (priv != NULL) {
        error_action = priv->config->cf_erroraction_conv;
    } else {
        return mlfi_cleanup(ctx);
    }
    ret = msy_exec_abort(priv);
    if (ret != R_SUCCESS) {
        mlfi_cleanup(ctx);
        mlfi_freepriv(ctx);
        return error_action;
    }

    return eom_cleanup(ctx);
}

/*
 * mlfi_close
 *
 * コールバック関数 (コネクション切断)
 * - 何もしない
 */
sfsistat
mlfi_close(SMFICTX *ctx)
{
    return mlfi_freepriv(ctx);
}

/*
 * eom_cleanup
 *
 * eomを終了する時点で、適正な情報を開放する
 * ※mlfi_eomから呼び出される
 *   (コールバック関数ではない)
 */
sfsistat
eom_cleanup(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_CONTINUE;

    if (priv == NULL) {
        return r;
    }

    strset_free(&priv->mlfi_envfrom);

    if (priv->mlfi_rcptto_h != NULL) {
        free_strlist(priv->mlfi_rcptto_h);
        priv->mlfi_rcptto_h = NULL;
    }
    if (priv->mlfi_addrmatched_h != NULL) {
        free_strlist(priv->mlfi_addrmatched_h);
        priv->mlfi_addrmatched_h = NULL;
    }

    priv->header_existence = FALSE;

    return r;
}

/*
 * mlfi_cleanup
 *
 * プライベート領域を解放する
 * ※mlfi_eom, mlfi_abortから呼び出される
 *   (コールバック関数ではない)
 */
sfsistat
mlfi_cleanup(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_CONTINUE;

    if (priv == NULL) {
        return r;
    }

    if (priv->config != NULL) {
        config_release(priv->config);
        priv->config = NULL;
    }

    strset_free(&priv->mlfi_envfrom);

    if (priv->mlfi_rcptto_h != NULL) {
        free_strlist(priv->mlfi_rcptto_h);
        priv->mlfi_rcptto_h = NULL;
    }
    if (priv->mlfi_addrmatched_h != NULL) {
        free_strlist(priv->mlfi_addrmatched_h);
        priv->mlfi_addrmatched_h = NULL;
    }

    free(priv);

    smfi_setpriv(ctx, NULL);

    return r;
}

/*
 * mlfi_freepriv
 *
 * プライベート領域を解放する
 * ※mlfi_closeから呼び出される
 *   (コールバック関数ではない)
 */
sfsistat
mlfi_freepriv(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_ACCEPT;

    if (priv == NULL) {
        return r;
    }

    if (priv->config != NULL) {
        config_release(priv->config);
        priv->config = NULL;
    }

    free(priv);

    smfi_setpriv(ctx, NULL);
 
    return r;
}

/*
 * usage
 *
 * usageメッセージを表示する
 */
void
usage(char *arg)
{
    printf("usage: %s [config file] [module config file]\n", arg);
}

/*
 * main
 */
int
main(int argc, char *argv[])
{
    char connsock[CONNSOCK_LEN];
    struct config *config;
    struct utsname utsname;

    char defaultconf[PATH_MAX + 1];
    char defaultmoduleconf[PATH_MAX + 1];
    char *configfile;
    char *module_configfile = NULL;

    int ret;

    /* オプションチェック */
    switch (argc) {
        /* 引数で指定されていない場合 */
        case 1:
            snprintf(defaultconf, PATH_MAX, "%s/messasy.conf",
                     DEFAULT_CONFDIR);
            configfile = defaultconf;
        
            snprintf(defaultmoduleconf, PATH_MAX, "%s/module.conf",
                     DEFAULT_CONFDIR);
            module_configfile = defaultmoduleconf;

            break;

        /* 引数で指定されている場合 */
        case 3:
            configfile = argv[1];
            module_configfile = argv[2];
            break;

        default:
            usage(argv[0]);
            exit(EXIT_MAIN);
    }

    /* 環境変数の設定 */
    set_environment(configfile);

    /* モジュール設定ファイルの読み込み */
    mhandle_list = NULL;
    ret = read_module_config(module_configfile);
    if (ret != R_SUCCESS) {
        free_lib_handle();
        exit(EXIT_MAIN);
    }

    /* 設定ファイル読み込み */
    ret = reload_config();
    if (ret != R_SUCCESS) {
        exit(EXIT_MAIN);
    }

    /* 管理インタフェースの起動 */
    ret = manager_init();
    if (ret != R_SUCCESS) {
        exit (EXIT_MAIN);
    }

    config = config_retrieve();

    /* ホスト名をグローバル変数に保存 */
    if (uname(&utsname) < 0) {
        SYSLOGERROR(ERR_UNAME, E_STR);
        exit(EXIT_MAIN);
    }
    strncpy(msy_hostname, utsname.nodename, MAX_HOSTNAME_LEN + 1);

    /* ソケットを設定 */
    sprintf(connsock, CONNSOCK, config->cf_listenport, config->cf_listenip);
    if (smfi_setconn(connsock) == MI_FAILURE) {
        SYSLOGERROR(ERR_SETCONN, E_STR);
        exit(EXIT_MAIN);
    }

    /* タイムアウトを設定 */
    if (smfi_settimeout(config->cf_timeout) == MI_FAILURE) {
        SYSLOGERROR(ERR_SETTIMEOUT, E_STR);
        exit(EXIT_MAIN);
    }

    config_release(config);

    /* コールバック関数を登録 */
    if (smfi_register(smfilter) == MI_FAILURE) {
        SYSLOGERROR(ERR_REGISTER, E_STR);
        exit(EXIT_MAIN);
    }

    /* libmilterに制御を引き渡す */
    if (smfi_main() == MI_FAILURE) {
        SYSLOGERROR(ERR_MLFISTART, E_STR);
        exit(EXIT_MAIN);
    }

    exit(EXIT_SUCCESS);
}
