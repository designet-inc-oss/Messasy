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

#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <dlfcn.h>
#include <regex.h>
#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <libmilter/mfapi.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stddef.h>

/* Messasy include file */
#include "messasy.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "utils.h"
#include "log.h"
#include "../lib_lm.h"

/* Header for my library */
#include "libgzip.h"

/* 比較文字の定義 */
#define CHAR_MAILDROP_MAILFOLDER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.%,_&-+ "
#define CHAR_MAILDROP_DOT_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "
#define CHAR_MAILDROP_SLASH_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "

#define MYMODULE "gzip"

#define HEADER_FUNC     "gzip_exec_header"
#define BODY_FUNC       "gzip_exec_body"
#define EOM_FUNC        "gzip_exec_eom"
#define ABORT_FUNC      "gzip_exec_abort"
#define MODCONF_FUNC    "gzip_exec_modconf"

/* プロトタイプ宣言 */
static int gzip_set_extra_config (char *, struct extra_config **, size_t);
static int gzip_set_module_list (char *, char *, struct modulelist **);

static char * is_mailfolder(char *str);
static char * is_dotdelimiter(char *str);
static char * is_slashdelimiter(char *str);
static struct gzip *md_struct_init(unsigned int, struct gzip_config *,
                                        time_t, struct strset *,
                                        struct strlist *, struct strlist *);
//static int md_makesavefilename(unsigned int, struct gzip *, char *, int, char *);
static int md_makesavefilename(struct gzip *, char *,
                               int, struct config *, struct stat);
static int md_makedirlist(unsigned int, struct gzip *, struct strlist **);
static int md_makedirbylist(unsigned int, struct gzip *, struct strlist *);
static int md_makemaildir_tree(unsigned int, char *, int);
static void md_makemaildir(unsigned int, char *);
static int md_mkdir(unsigned int, char *);
static void md_makesavefile(unsigned int, struct gzip *,
                            char *, struct strlist *);
static void md_list2str(unsigned int, struct strset *, struct strlist *);
static void md_free(struct gzip *);



/* extern struct modulehandle *mhandle_list; */
struct modulehandle *mhandle_list;
char msy_hostname[MAX_HOSTNAME_LEN + 1];

struct cfentry gzip_cfe[] = {
    {
        "GzipCommand", CF_STRING, "/usr/bin/gzip",
        MESSASY_OFFSET(struct gzip_config, cf_gzipcommand), is_executable_file
    },
    {
        "GzipMailDir", CF_STRING, NULL,
        MESSASY_OFFSET(struct gzip_config, cf_gzipmaildir), is_writable_directory
    },
    {
        "GzipMailFolder", CF_STRING, NULL,
        MESSASY_OFFSET(struct gzip_config, cf_gzipmailfolder), is_mailfolder
    },
    {
        "GzipDotDelimiter", CF_STRING, ",",
        MESSASY_OFFSET(struct gzip_config, cf_gzipdotdelimiter), is_dotdelimiter
    },
    {
        "GzipSlashDelimiter", CF_STRING, "_",
        MESSASY_OFFSET(struct gzip_config, cf_gzipslashdelimiter), is_slashdelimiter
    }
};

/*
 * gzip_init
 *
 * 機能:
 *    gzipモジュールの初期化関数
 *
 * 引数:
 *    struct cfentry **cfe      config entry 構造体
 *    size_t cfesize            config entry 構造体のサイズ
 *    struct config  **cfg      config 構造体
 *    size_t cfgsize            config 構造体のサイズ
 *
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_init(struct cfentry **cfe, size_t *cfesize,
           struct config **cfg, size_t *cfgsize)
{
    struct config *new_cfg;
    struct cfentry *new_cfe;
    size_t new_cfesize, new_cfgsize;
    int ret, i;
    struct modulelist *tmp_list;

    /* モジュールリストへの追加 */
    ret = gzip_set_module_list(MYMODULE, HEADER_FUNC, &(*cfg)->cf_exec_header);
    if (ret != 0) {
        return -1;
    }
    ret = gzip_set_module_list(MYMODULE, BODY_FUNC, &(*cfg)->cf_exec_body);
    if (ret != 0) {
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    ret = gzip_set_module_list(MYMODULE, EOM_FUNC, &(*cfg)->cf_exec_eom);
    if (ret != 0) {
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* ボディのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    ret = gzip_set_module_list(MYMODULE, ABORT_FUNC, &(*cfg)->cf_exec_abort);
    if (ret != 0) {
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* ボディのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eomのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        return -1;
    }

    /* cfgの拡張 */
    new_cfgsize = *cfgsize + sizeof(struct gzip_config);
    new_cfg = (struct config *)realloc(*cfg, new_cfgsize);
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "gzip_set_module_list", strerror(errno));
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* ボディのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eomのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abortのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    memset((char *)new_cfg + *cfgsize, '\0', new_cfgsize - *cfgsize);
    *cfg = new_cfg;

    /* cfeの拡張 */
    new_cfesize = *cfesize + sizeof(gzip_cfe);
    new_cfe = (struct cfentry *)realloc(*cfe, new_cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "gzip_set_module_list", strerror(errno));
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* ボディのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eomのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abortのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        return -1;
    }

    /* gzip_cfeのコピー */
    memcpy(new_cfe + *cfesize / sizeof(struct cfentry),
           &gzip_cfe, sizeof(gzip_cfe));

    /* dataoffsetの更新 */
    for (i = 0; i < MAILDROP_CFECOUNT; i++) {
        new_cfe[(*cfesize / sizeof(struct cfentry)) + i].cf_dataoffset += *cfgsize;
    }
    *cfe = new_cfe;

    /* モジュール毎のconfig構造体offsetを格納 */
    ret = gzip_set_extra_config(MYMODULE, &(*cfg)->cf_extraconfig, *cfgsize);
    if (ret != 0) {
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* ボディのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eomのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abortのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        /* reallocしたコンフィグ領域を残って、他のタスクを使用ためです。*/

        return -1;
    }

    /* cfesize, cfgsizeの更新 */
    *cfesize = new_cfesize;
    *cfgsize = new_cfgsize;

    return 0;
}

/*
 * gzip_set_module_list
 *
 * 機能:
 *    gzipモジュール用のモジュールリスト作成
 *
 * 引数:
 *    char *modname             モジュール名
 *    char *funcname            関数名
 *    struct modulelist **list  モジュールリスト
 *
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_set_module_list(char *modname, char *funcname, struct modulelist **list)
{
    struct modulelist *new_list;

    /* module名のポインタを格納する領域の確保 */
    new_list = (struct modulelist *)malloc(sizeof(struct modulelist));
    if(new_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, "gzip_set_module_list", strerror(errno));
        return -1;
    }

    new_list->mlist_modulename = modname;
    new_list->mlist_funcname = funcname;
    new_list->mlist_next = *list;
    *list = new_list;

    return 0;
}

/*
 * gzip_set_extra_config
 *
 * 機能:
 *    gzipモジュール用のextra configの作成
 *
 * 引数:
 *    char *modname                     モジュール名
 *    struct extra_config **ext_cfg     extra config リスト
 *    size_t cfgsize                    config 構造体のサイズ(extra config までのoffset)
 *
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_set_extra_config(char *modname, struct extra_config **ext_cfg,
                        size_t cfgsize)
{
    struct extra_config *new_cfg;

    /* 外部モジュールのconfig構造体ポインタを格納する領域の確保 */
    new_cfg = (struct extra_config *)malloc(sizeof(struct extra_config));
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "gzip_set_module_list", strerror(errno));
        return -1;
    }

    new_cfg->excf_modulename = modname;

    new_cfg->excf_config = (void *)cfgsize;
    new_cfg->excf_next = *ext_cfg;
    *ext_cfg = new_cfg;

    return 0;
}

/*
 * is_mailforder
 *
 * 機能
 *    メールフォルダーのチェック
 *
 * 引数
 *      char *str   チェックする文字列
 *
 * 返り値
 *      NULL                   正常
 *      ERR_CONF_MAILFOLDER    エラーメッセージ
 */
char *
is_mailfolder(char *str)
{
    char string[] = CHAR_MAILDROP_MAILFOLDER;
    int  i, j;

    /* 文字列の先頭が「.」でないことの確認 */
    if (str[0] != '.') {
        return ERR_MAILDROP_MAILFOLDER;
    }

    for (i = 0; str[i] != '\0'; i++) {
        /*「.」が連続していないことの確認 */
        if ((str[i] == '.') && (str[i+1] == '.')) {
            return ERR_MAILDROP_MAILFOLDER;
        }
        /* メールフォルダの名前として適切な文字が使われていることの確認 */
        for (j = 0; string[j] != '\0'; j++) {
            if (str[i] == string[j]) {
                break;
            }
        }
        /* 文字が合致することなく抜けた場合、エラー */
        if (string[j] == '\0') {
            return ERR_MAILDROP_MAILFOLDER;
        }
    }
    /* 文字列の最後が｀でないことの確認 */
    if (str[i-1] == '.') {
        return ERR_MAILDROP_MAILFOLDER;
    }
    return NULL;
}

/*
 * is_dotdelimiter
 *
 * 機能
 *    .の置き換え文字のチェック
 *
 * 引数
 *      char *str   チェックする文字列
 *
 * 返り値
 *      NULL                     正常
 *      ERR_CONF_DOTDELIMITER    エラーメッセージ
 */
char *
is_dotdelimiter(char *str)
{
    char string[] = CHAR_MAILDROP_DOT_DELIMITER;
    int i;

    /* 一文字であるか */
    if (str[1] != '\0') {
        return ERR_MAILDROP_DOTDELIMITER;
    }

    /* 文字チェック */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* 合致することなく抜けてしまった場合は、違反した文字 */
    if (string[i] == '\0' ) {
        return ERR_MAILDROP_DOTDELIMITER;
    }
    return NULL;
}

/*
 * is_slashdelimiter
 *
 * 機能
 *    /の置き換え文字のチェック
 *
 * 引数
 *      char *str   チェックする文字列
 *
 * 返り値
 *      NULL                       正常
 *      ERR_CONF_SLASHDELIMITER    エラーメッセージ
 */
char *
is_slashdelimiter(char *str)
{
    char string[] = CHAR_MAILDROP_SLASH_DELIMITER;
    int i;

    /* 一文字であるか */
    if (str[1] != '\0') {
        return ERR_MAILDROP_SLASHDELIMITER;
    }

    /* 文字チェック */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* 合致することなく抜けてしまった場合は、違反した文字 */
    if (string[i] == '\0' ) {
        return ERR_MAILDROP_SLASHDELIMITER;
    }
    return NULL;
}

/*
 * gzip_free_config
 *
 * 機能:
 *    gzipのconfig領域をfreeする関数
 * 引数:
 *    mP     : priv構造体をつなぐ構造体
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_free_config(struct config *cfg)
{
    struct gzip_config *p = NULL;
    struct extra_config *exp;

    if (cfg == NULL || cfg->cf_extraconfig == NULL) {
        return -1;
    }

    for (exp = cfg->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
	if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
	    p = (struct gzip_config *)(exp->excf_config);
	    break;
	}
    }

    /* pが見つかった場合*/
    if (p != NULL) {
        if (p->cf_gzipcommand != NULL) {
            free(p->cf_gzipcommand);
        }
        if (p->cf_gzipmaildir != NULL) {
            free(p->cf_gzipmaildir);
        }
        if (p->cf_gzipmailfolder != NULL) {
            free(p->cf_gzipmailfolder);
        }
        if (p->cf_gzipdotdelimiter != NULL) {
            free(p->cf_gzipdotdelimiter);
        }
        if (p->cf_gzipslashdelimiter != NULL) {
            free(p->cf_gzipslashdelimiter);
        }
    } else {
        return -1;
    }

    return 0;
}

/***** ***** ***** ***** *****
 * 内部関数
 ***** ***** ***** ***** *****/

/*
 * md_struct_init
 *
 * gzip構造体の確保と初期化を行なう
 *
 * 引数
 *      unsigned int            セッションID
 *      struct config *         config構造体のポインタ
 *      time_t                  メール受信時刻
 *
 * 返り値
 *      struct gzip *       gzip構造体
 */
static struct gzip *
md_struct_init(unsigned int s_id, struct gzip_config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct gzip *md;
    int ret;

    /* 領域を確保 */
    md = (struct gzip *)malloc(sizeof(struct gzip));
    if (md == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "md_struct_init", E_STR);
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct gzip));

    /* 受信日時を保存*/
    md->md_recvtime = time;

    /* MailDir, MailFolderの値を保存*/
    strset_set(&md->md_maildir, config->cf_gzipmaildir);
    strset_set(&md->md_mailfolder, config->cf_gzipmailfolder);

    /* dotdelmiter, slashdelimiterの値保存*/
    md->md_dotdelimiter = *(config->cf_gzipdotdelimiter);
    md->md_slashdelimiter = *(config->cf_gzipslashdelimiter);

    /* カスタムヘッダを作成*/
    /* FROMヘッダの初期化*/
    strset_init(&md->md_header_from);
    /* 値格納*/
    ret = strset_catstrset(&md->md_header_from, from);
    if (ret == -1) {
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
        exit(EXIT_MILTER);
    }
    /* TOヘッダの初期化*/
    strset_init(&md->md_header_to);
    /* 値格納*/
    md_list2str(s_id, &md->md_header_to, to_h);

    /* 保存アドレス一覧の格納*/
    md->md_saveaddr_h = saveaddr_h;

    return md;
}


/*
 * gzip_get_priv
 *
 * 機能:
 *    extrapriv領域がなければ作成し、
 *    あれば自分用の領域のextrapriv領域ポインタを返す関数
 * 引数:
 *    priv: mlfiPriv構造体のポインタ(参照渡し)
 * 返値:
 *   自分用のextrapriv構造体のポインタ
 */
struct extrapriv *
gzip_get_priv(struct mlfiPriv **priv)
{
    struct extrapriv *p = NULL;      /* 検索用 */
    struct extrapriv *mp = NULL;     /* 新規作成用 */
    struct extrapriv *p_old = NULL;  /* 検索中、ひとつ前のポインタ保存用 */

    if (*priv != NULL) {
        /* 自分のpriv構造体があるか検索 */
        for (p = (*priv)->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (strcmp(MYMODULE, p->expv_modulename) == 0) {
                /* あったらリターン */
                return p;
            }
            /* ひとつ前のポインタ格納 */
            p_old = p;
        }
    }
    /* 自分用のextrapriv領域新規作成 */
    mp = malloc(sizeof(struct extrapriv));
    if (mp == NULL) {
        SYSLOGERROR(ERR_MALLOC, "gzip_get_priv", E_STR);
        return NULL;
    }
    /* 値の格納 */
    /* MYMODULEの値をそのまま参考するので、開放するときはしないでください*/
    mp->expv_modulename = MYMODULE;
    /* NEXTに初期化*/
    mp->expv_next = NULL;
    /* プライベートの初期化*/
    mp->expv_modulepriv = NULL;

    /* 何も存在していなかったら先頭にポインタを付ける */
    if (p_old == NULL) {
        (*priv)->mlfi_extrapriv = mp;

    /* 存在しているが、自分用がなかったら後ろにつける */
    } else if (p == NULL) {
        p_old->expv_next = mp;
    }
    return mp;
}

/*
 * gzip_priv_free
 *
 * 機能:
 *    すべてのpriv構造体をfreeする関数
 * 引数:
 *     extrapriv:   引数の構造体のポインタ(参照渡し)
 * 返値:
 *    無し
 */
void
gzip_priv_free(struct extrapriv *expv)
{
    /* NULLチェック */
    if (expv != NULL) {
        /* gzip_priv領域がある場合 */
        if (expv->expv_modulepriv != NULL) {
            /* gzip_priv構造体のfree */
            /* private 変数開放*/
            free(expv->expv_modulepriv);
            expv->expv_modulepriv = NULL;
        }
        /* extrapriv領域のfree */
        free(expv);
        expv = NULL;
    }

    return;
}


/*
 * gzip_abort
 *
 * メール保存処理を中止する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct gzip *       gzip構造体
 *
 * 返り値
 *      無し
 */
void
gzip_abort(unsigned int s_id, struct gzip *md)
{
    /* プライベート情報がNULLの場合*/
    if (md == NULL) {
        return;
    }

    if (md->md_tempfile_fd > 0) {
        /* ファイルがオープンされている場合はクローズする */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }
    unlink(md->md_tempfilepath);
    return;
}

/*
 * gzip_exec_header
 *
 * 機能:
 *    mlfi_headerで呼ばれる関数
 *    priv領域の確保・ヘッダ情報のメモリ格納する関数
 * 引数:
 *    priv   : priv構造体をつなぐ構造体
 *    headerf: ヘッダの項目名
 *    headerv: ヘッダの項目に対する値
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_exec_header(struct mlfiPriv *priv, char *headerf, char *headerv)
{
    /* 変数宣言*/
    struct extrapriv     *expv;
    struct extra_config  *p;
    struct gzip_priv *mypv;
    struct gzip      *mydat;
    struct gzip      *mydatp;
    int                  ret;
    unsigned int         s_id;

    /* 初期化*/
    expv = NULL;
    p = NULL;
    mypv = NULL;
    mydat = NULL;
    mydatp = NULL;
    ret = 0;
    s_id = priv->mlfi_sid;


    /* extrapriv領域の有無 */
    expv = gzip_get_priv(&priv);

    /* gzip_get_privがエラーの時 */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_header", "gzip_get_priv");
        return -1;
    }

    /* gzip_priv領域がなかったら作成 */
    if (expv->expv_modulepriv == NULL) {
        /* gzip領域 */
        mypv = malloc(sizeof(struct gzip_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "gzip_exec_header", E_STR);
            return -1;
        }

        /* 2つをつなげる */
        expv->expv_modulepriv = mypv;

        /* 自分のconfig構造体検索 */ 
        if (priv->config->cf_extraconfig != NULL) { 
            for (p = priv->config->cf_extraconfig; p != NULL; p = p->excf_next) {
                if (!strcmp(MYMODULE, p->excf_modulename)) {
                    break;
                }
            }
        }

        
        /* オープン  メール保存処理を開始 */
        mydat = gzip_open(s_id, ((struct gzip_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR(ERR_S_FOPEN, s_id, "gzip_exec_header", "gzip_open");
            free(mypv);
            return -1;
        }
        
        /* 構造体をつなげる */
        mypv->mypriv = mydat;
    }

    /* gzip構造体のポインタを変数に格納 */
    mydatp = ((struct gzip_priv *)expv->expv_modulepriv)->mypriv;

    /* ヘッダを書き込み*/
    ret = gzip_write_header(s_id, mydatp, headerf, headerv);
    if (ret != R_SUCCESS) {
        SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_header", "gzip_write_header");
        return -1;
    }

    return 0;
}

/*
 * gzip_exec_body
 *
 * 機能:
 *    mlfi_bodyで呼ばれる関数
 *    priv領域の確保・ヘッダ情報のメモリ格納する関数
 * 引数:
 *    *priv  : priv構造体をつなぐ構造体(参照渡し)
 *    *bodyp : mlfi_bodyが取得したボディ部
 *    bodylen: bodypのサイズ
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_exec_body(struct mlfiPriv *priv, u_char *bodyp, size_t bodylen)
{
    /* 変数宣言*/
    struct extrapriv     *expv;
    struct gzip      *mydat;
    struct gzip_priv *mypv;
    struct gzip      *mydatp;
    int                   ret;
    unsigned int          s_id;
    struct extra_config  *p;

    /* 初期化*/
    expv = NULL;
    p = NULL;
    mypv = NULL;
    mydat = NULL;
    mydatp = NULL;
    ret = 0;
    s_id = priv->mlfi_sid;


    /* extrapriv領域の有無 */
    expv = gzip_get_priv(&priv);
    /* gzip_get_privがエラーの時 */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_body", "gzip_get_priv");
        return -1;
    }

    /* gzip_priv領域がなかったら作成 */
    if (expv->expv_modulepriv == NULL) {
        /* gzip領域 */
        mypv = malloc(sizeof(struct gzip_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "gzip_exec_body", E_STR);
             return -1;
        }

        /* 2つをつなげる */
        expv->expv_modulepriv = mypv;

        /* 自分のconfig構造体検索 */ 
        if (priv->config->cf_extraconfig != NULL) { 
            for (p = priv->config->cf_extraconfig; p != NULL; p = p->excf_next) {
                if (!strcmp(MYMODULE, p->excf_modulename)) {
                    break;
                }
            }
        }

       
        /* オープン  メール保存処理を開始 */
        mydat = gzip_open(s_id, ((struct gzip_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR(ERR_S_FOPEN, s_id, "gzip_exec_header", "gzip_open");
            free(mypv);
            return -1;
        }
        
        /* 構造体をつなげる */
        mypv->mypriv = mydat;
    }

    /* メールデータを取得*/
    mydat = ((struct gzip_priv *)expv->expv_modulepriv)->mypriv;

    /* ボディ書き込み */
    ret = gzip_write_body(s_id, mydat, bodyp, bodylen);
    if (ret != R_SUCCESS) {
        SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_body", "gzip_write_body");
        return -1;
    }

    return 0;
}

/*
 * gzip_exec_eom
 *
 * 機能:
 *    mlfi_eomで呼ばれる関数
 *    mlfi_headerで格納したヘッダ情報を構造体に格納し、
 *    DBに登録する関数
 * 引数:
 *    priv: priv構造体をつなぐ構造体(参照渡し)
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
gzip_exec_eom(struct mlfiPriv *priv)
{
    /* 変数宣言*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct gzip     *mydat;
    int                  ret;
    unsigned int         s_id;

    /* 初期化*/
    p = NULL;
    p_old = NULL;
    mydat= NULL;
    ret = 0;
    s_id = priv->mlfi_sid;

    /* 自分の領域有無チェック*/
    if (priv != NULL) {
        /* 自分のpriv構造体があるか検索*/
        for (p = priv->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (!strcmp(MYMODULE, p->expv_modulename)) {
                break;
            }
            p_old = p;
        }
 
        /* 一つ前の領域がextrapriv構造体*/
        if (p_old != NULL) {
            if (p != NULL) {
                mydat = ((struct gzip_priv *)p->expv_modulepriv)->mypriv;
                /* クローズ*/
                ret = gzip_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_eom", "gzip_close");
                    return -1;
                }
                /* 一つ前の構造体のnextにfreeする構造体のnextをつなげる*/
                p_old->expv_next = p->expv_next;
                /* プライベートデータを開放する*/
                gzip_priv_free(p);

            }
        /* Pが先頭の場合*/
        } else {
            if (p != NULL) {
                mydat = ((struct gzip_priv *)p->expv_modulepriv)->mypriv;
                /* クローズ*/
                ret = gzip_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_EXEC_FUNC, "gzip_exec_eom", "gzip_close");
                    return -1;
                }
                /* 一つ前のmlfi構造体にfreeする構造体のnextをつなげる*/
                priv->mlfi_extrapriv = p->expv_next;
                /* プライベート情報開放*/
                gzip_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * gzip_exec_abort
 *
 * 機能:
 *    mlfi_abortやexec_eomで呼ばれる関数
 *    priv構造体を全てfreeする関数
 *
 * 引数:
 *    priv: priv構造体をつなぐ構造体
 *
 * 返値:
 *    0(R_SUCCESS): 正常
 */
int
gzip_exec_abort(struct mlfiPriv *priv)
{
    /* 変数宣言*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct gzip     *md;
    unsigned int         s_id;

    /* 初期化*/
    p = NULL;
    p_old = NULL;
    md = NULL;
    s_id = priv->mlfi_sid;

    /* 自分の領域有無チェック */
    if (priv != NULL) {
        /* 自分のpriv構造体があるか検索 */
        for (p = priv->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (!strcmp(MYMODULE, p->expv_modulename)) {
                break;
            }
            p_old = p;
        }
        /* 一つ前の領域がextrapriv構造体 */
        if (p_old != NULL) {
            if (p != NULL) {
                md = ((struct gzip_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                gzip_abort(s_id, md);
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                p_old->expv_next = p->expv_next;
                gzip_priv_free(p);
            }

        /* Pが先頭の場合*/
        } else {
            if (p != NULL) {
                md = ((struct gzip_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                gzip_abort(s_id, md);
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                priv->mlfi_extrapriv = p->expv_next;
                gzip_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * gzip_open
 *
 * ファイル書き込みの準備を行なう
 * - 必要なディレクトリの作成
 * - 一時ファイルのオープン
 * - カスタムヘッダの値作成
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct config *         config構造体
 *      time_t                  受信日時
 *      struct strset *         From
 *      struct strlist *        To一覧の先頭ポインタ
 *      struct strlist *        保存アドレス一覧の先頭ポインタ
 *
 * 返り値
 *      struct gzip *       正常
 *      NULL                    エラー (一時ファイルのオープンに失敗)
 */
struct gzip *
gzip_open(unsigned int s_id, struct gzip_config *config,
                time_t time, struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h, char *msy_hostname)
{
    struct gzip *md;
    mode_t old_umask;
    int temppathlen;

    /* gzip構造体を初期化 */
    md = md_struct_init(s_id, config, time, from, to_h, saveaddr_h);

    /* MailDir配下にサブディレクトリを作成する */
    md_makemaildir(s_id, config->cf_gzipmaildir);

    /* 一時ファイルのパスを作成 */
    temppathlen = strlen(config->cf_gzipmaildir) +
                    strlen(msy_hostname) + TEMPFILEPATH_LEN;
    md->md_tempfilepath = (char *)malloc(temppathlen);
    if (md->md_tempfilepath == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "gzip_open", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(md->md_tempfilepath, TEMPFILEPATH,
            config->cf_gzipmaildir, md->md_recvtime, msy_hostname);

    /* 一時ファイルをオープン */
    old_umask = umask(0077);
    md->md_tempfile_fd = mkstemp(md->md_tempfilepath);
    umask(old_umask);
    if (md->md_tempfile_fd < 0) {
        SYSLOGERROR(ERR_S_MKSTEMP, s_id, md->md_tempfilepath, E_STR);
        return NULL;
    }
    return md;
}

/*
 * gzip_write_header
 *
 * ヘッダを一時ファイルに出力する
 * カスタムヘッダを初めに書き込む
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct gzip *       gzip構造体
 *      char *                  ヘッダフィールド (コールバックに渡されたまま)
 *      char *                  ヘッダ値 (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
gzip_write_header(unsigned int s_id, struct gzip *md,
                        char *headerf, char *headerv)
{
    char *header, *p;
    int header_len;
    ssize_t written_len;
    int ret;

    if (!md->md_writing_header) {
        /* はじめにカスタムヘッダを書き込む */
        md->md_writing_header = 1;
        ret = gzip_write_header(s_id, md, CUSTOMHDR_FROM,
                                    md->md_header_from.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
        ret = gzip_write_header(s_id, md, CUSTOMHDR_TO,
                                    md->md_header_to.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* ヘッダの書き込み */
    header_len = strlen(headerf) + ((headerv == NULL)?0:strlen(headerv)) + 3; /* 文字列 + ': ' + '\n' */
    header = (char *)malloc(header_len + 1);    /* '\0' */
    if (header == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "gzip_write_header", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(header, "%s: %s\n", headerf, (headerv == NULL)?"":headerv);

    p = header;
    written_len = 0;
    while (written_len < header_len) {
        written_len = write(md->md_tempfile_fd, p, header_len);
        if (written_len < 0) {
            SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            free(header);
            return R_ERROR;
        }
        p += written_len;
        header_len -= written_len;
    }

    free(header);

    return R_SUCCESS;
}

/*
 * gzip_write_body
 *
 * メールボディを一時ファイルに出力する
 * ヘッダとボディの区切りを初めに書き込む
 * 改行文字はCRLFをLFに統一する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct gzip *       gzip構造体
 *      unsigned char *         ボディ (コールバックに渡されたまま)
 *      size_t                  長さ (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
gzip_write_body(unsigned int s_id, struct gzip *md,
                    unsigned char *bodyp, size_t len)
{
    ssize_t written_len;
    int ret;
    int i;

    if (!md->md_writing_body) {
        /* はじめにヘッダとボディの区切り文字を書き込む */
        md->md_writing_body = 1;
        ret = gzip_write_body(s_id, md, (unsigned char *) "\n", 1);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* 改行文字をLFに統一しながら、本文を書き込む */
    for (i = 0; i < len; i++, bodyp++) {
        if (md->md_cr) {
            if (*bodyp != '\n') {
                written_len = write(md->md_tempfile_fd, "\r", 1);
                if (written_len < 0) {
                    SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
                    return R_ERROR;
                }
            }
            md->md_cr = 0;
        }
        if (*bodyp == '\r') {
            md->md_cr = 1;
            continue;
        }
        written_len = write(md->md_tempfile_fd, bodyp, 1);
        if (written_len < 0) {
            SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            return R_ERROR;
        }
    }

    return R_SUCCESS;
}

/*
 * gzip_close
 *
 * 一時ファイルをクローズし、保存する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct gzip *           gzip構造体
 *      struct config *         config構造体
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
gzip_close(unsigned int s_id, struct gzip *md, struct config * config)
{
    pid_t pid, wpid;
    struct stat stbuf;
    int status;
    struct strlist *list_h;
    char filename[NAME_MAX + 6 + GZIP_SUFFIX_LEN];
    char *tempstr;
    int tempstr_len;
    size_t ret_s;
    int ret;
    int len;

    struct gzip_config *gzcf;
    struct extra_config *exp;
    char  *command_real;
    char **command_args;

    /* 初期化*/
    tempstr_len = 0;
    tempstr = NULL;
    gzcf = NULL;
    exp = NULL;
    command_real = NULL;
    command_args = NULL;
    len = 0;

    /* 一時ファイルをクローズ */
    if (md->md_tempfile_fd > 0) {
        /* 改行文字扱いでないCRが残っている場合は書き込む */
        if (md->md_cr) {
            ret_s = write(md->md_tempfile_fd, "\r", 1);
            if (ret_s < 0) {
                SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            }
            md->md_cr = 0;
        }
        /* クローズ */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }

    /* extraconfigを取得*/
    if (config == NULL || config->cf_extraconfig == NULL) {
        SYSLOGERROR(ERR_GZIP_CONF, s_id);
        return R_ERROR;
    }

    for (exp = config->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
        if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
            gzcf = (struct gzip_config *)(exp->excf_config);
            break;
        }
    }
    /* みつから無かったら、エラー*/
    if (gzcf == NULL) {
        SYSLOGERROR(ERR_GZIP_CONF, s_id);
        return R_ERROR;
    }

    /* 一時ファイルのiノード番号を取る */
    ret = stat(md->md_tempfilepath, &stbuf);
    if (ret < 0) {
        SYSLOGERROR(ERR_S_STAT, s_id, md->md_tempfilepath, E_STR);
        return R_ERROR;
    }

    /* 子プロセス作成*/
    pid = fork();
    if (pid == -1) {
        SYSLOGERROR(ERR_S_FORK, gzcf->cf_gzipcommand);
        return R_ERROR;

    /* 子プロセス*/
    } else if (pid == 0) {

        /* convert command*/
        command_args = cmd_strrep(gzcf->cf_gzipcommand, ' ', &command_real,
                                  EXTEND_PART_OPTION_NUM_GZIP);
        if (command_args == NULL) {
            SYSLOGWARNING(ERR_MEMORY_ALLOC);
            exit(1);
        }
        /* GzipCommandの長さを計算*/
        for(len = 0; command_args[len] != NULL; len++) {
        }

        /* パラメータのリスト作成*/
        /* 子プロセスだから、親プロセスのGzipcommandに影響しない*/
        command_args[len] = md->md_tempfilepath;

        /* 圧縮コマンド実行*/
        ret = execv(command_args[0], command_args);
        /* エラー処理*/
        if (ret == -1) {
            SYSLOGERROR(ERR_RUN_COMMAND, s_id, gzcf->cf_gzipcommand);
            exit(1);
        }

        /* may not reach*/
        exit(1);

    /* 親プロセス*/
    } else {
        /* 子プロセス待つ*/
        wpid = waitpid(pid, &status, WUNTRACED);
        if (wpid < 0) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, gzcf->cf_gzipcommand, status);
            /* 一時ファイル削除 */
            unlink(md->md_tempfilepath);
            return R_ERROR;
        }
        /* 子プロセスのステータス確認*/
        if (!WIFEXITED(status)) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, gzcf->cf_gzipcommand, status);
            /* 一時ファイル削除 */
            unlink(md->md_tempfilepath);
            return R_ERROR;
        }
        /* 子プロセスが正常に終了されない場合*/
        if (WEXITSTATUS(status)) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, gzcf->cf_gzipcommand, status);
            /* 一時ファイル削除 */
            unlink(md->md_tempfilepath);
            return R_ERROR;
        }
    }

    /* コマンドの実行結果チェック*/
    /* ziptempfilepath領域確保*/
    tempstr_len = strlen(md->md_tempfilepath) +
                     strlen(msy_hostname) + ZIPTEMPFILEPATH_LEN;
    tempstr = malloc(tempstr_len);
    if (tempstr == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "gzip_close", E_STR);
        return R_ERROR;
    } 

    /* ziptempfile存在チェック*/
    /* 一時ファイルのiノード番号を取る */
    sprintf(tempstr, ZIPTEMPFILEPATH, md->md_tempfilepath);
    ret = stat(tempstr, &stbuf);
    if (ret < 0) {
        SYSLOGERROR(ERR_S_STAT, s_id, tempstr, E_STR);
        /* 元のtempfile削除*/
        if (stat(md->md_tempfilepath, &stbuf) == 0) {
            /* 一時ファイル削除 */
            unlink(md->md_tempfilepath);
        }
        free(tempstr);
        /* エラーを返す*/
        return R_ERROR;
    }

    /* 既存のtempfilepathの領域開放*/
    free(md->md_tempfilepath);

    /* ziptempfilepath保存*/
    md->md_tempfilepath = tempstr;

    /* 保存先のファイル名を作成 */
    ret = md_makesavefilename(md, filename, sizeof(filename), config, stbuf);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* 必要なディレクトリ一覧を作成 */
    ret = md_makedirlist(s_id, md, &list_h);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* 必要なディレクトリを作成 */
    ret = md_makedirbylist(s_id, md, list_h);
    if (ret != R_SUCCESS) {
        free_strlist(list_h);
        return R_ERROR;
    }

    /* ファイルコピー */
    md_makesavefile(s_id, md, filename, list_h);

    /* 一時ファイル削除 */
    unlink(md->md_tempfilepath);

    free_strlist(list_h);

    md_free(md);
    md = NULL;

    return 0;
}


/***** ***** ***** ***** *****
 * 内部関数
 ***** ***** ***** ***** *****/

/*
 * md_makedirlist
 *
 * 作成する必要のあるディレクトリ一覧を作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct gzip *       gzip構造体
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
static int
md_makedirlist(unsigned int s_id, struct gzip *md, struct strlist **list_h)
{
    struct strlist *list_t, *p;
    char mailaddr[MAX_ADDRESS_LEN + 1];
    struct strformat sf[6];
    char year[5], month[3], day[3];
    char *addr_p, *domain_p, *tmp;
    struct strset path;
    struct tm lt, *ret_t;
    int ret;

    /* 受信時刻から置換文字列を作成 */
    ret_t = localtime_r(&md->md_recvtime, &lt);
    if (ret_t == NULL) {
        SYSLOGERROR(ERR_S_LTIME, s_id, E_STR);
        return R_ERROR;
    }
    strftime(year,  5, "%Y", &lt);
    strftime(month, 3, "%m", &lt);
    strftime(day,   3, "%d", &lt);

    sf[0].sf_formatchar = 'y';
    sf[0].sf_replacestr = year;
    sf[1].sf_formatchar = 'm';
    sf[1].sf_replacestr = month;
    sf[2].sf_formatchar = 'd';
    sf[2].sf_replacestr = day;

    /* 保存対象アドレス毎にディレクトリ名を作成する */
    *list_h = list_t = NULL;
    p = md->md_saveaddr_h;
    while (p != NULL) {
        /* アドレスとドメインから置換文字列を作成する */
        strncpy(mailaddr, p->ss_data.ss_str, MAX_ADDRESS_LEN + 1);
        ret = check_7bit(mailaddr);
        if (ret != 0) {
            /* 8bit文字が含まれるためUNKNOWNに */
            addr_p = UNKNOWN;
            domain_p = UNKNOWN;
        } else {
            replace_delimiter(mailaddr, DOT, md->md_dotdelimiter);
            replace_delimiter(mailaddr, SLASH, md->md_slashdelimiter);

            domain_p = strchr(mailaddr, '@');
            if (domain_p == NULL) {
                /* アドレス一覧の作成時にドメインが補完されるので、
                 * ここには入らないはず */
                domain_p = UNKNOWN;
            } else {
                domain_p++;
            }
            addr_p = mailaddr;
        }

        sf[3].sf_formatchar = 'D';
        sf[3].sf_replacestr = domain_p;
        sf[4].sf_formatchar = 'M';
        sf[5].sf_formatchar = 'f';
        sf[4].sf_replacestr = sf[5].sf_replacestr = addr_p;

        /* MailFolderのフォーマット文字を置換する */
        tmp = str_replace_format(md->md_mailfolder.ss_str, sf, 6);

        /* MailDir, MailFolder (置換後) を連結する */
        strset_init(&path);
        if (strset_catstrset(&path, &md->md_maildir) == -1 ||
            strset_catstr(&path, "/") == -1 ||
            strset_catstr(&path, tmp) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
        free(tmp);

        /* ディレクトリ一覧に追加する
         * まったく同一のパスが既に一覧にある場合は無視する */
        uniq_push_strlist(list_h, &list_t, path.ss_str);


        strset_free(&path);
        p = p->next;
    }

    return R_SUCCESS;
}

/*
 * md_makedirbylist
 *
 * ディレクトリ一覧を元に、Maildir形式のディレクトリを作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct gzip *       gzip構造体
 *      struct strlist *        ディレクトリ一覧の先頭ポインタ
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
static int
md_makedirbylist(unsigned int s_id, struct gzip *md, struct strlist *list)
{
    struct strlist *p;
    int ret;

    p = list;
    while (p != NULL) {
        ret = md_makemaildir_tree(s_id, p->ss_data.ss_str,
                                    md->md_maildir.ss_len);

        if (ret != R_SUCCESS) {
            return R_ERROR;
        }

        p = p->next;
    }

    return R_SUCCESS;
}

/*
 * md_makemaildir_tree
 *
 * 指定されたディレクトリに至るディレクトリツリーをMaildir形式で作成する
 *
 * /home/archive/Maildir/.2009.10.01
 * → /home/archive/Maildir/.2009/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10.01/{new,cur,tmp}
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *                              (最も深いディレクトリを指定する)
 *      int                     ベースディレクトリの長さ
 *                              (Maildir形式のツリーの起点を指定する)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー (引数エラー)
 */
static int
md_makemaildir_tree(unsigned int s_id, char *targetdir, int basedir_len)
{
    char *subtop, *dot;

    /* ポインタをフォルダ先頭のドットに移動させる
     * /path/to/basedir/.folder
     *                  ^-subtop */
    subtop = targetdir + basedir_len + 1;
    if (strchr(subtop, SLASH) != NULL) {
        /* 万一フォルダ配下にスラッシュが含まれていた場合は
         * 対応していないのでエラーを返す */
        return R_ERROR;
    }

    /* サブディレクトリを作成する */
    while ((dot = strchr(subtop, DOT)) != NULL) {
        /* ドットを\0に一時的に置き換えてディレクトリを作成 */
        *dot = '\0';
        md_makemaildir(s_id, targetdir);
        *dot = DOT;
        subtop = dot + 1;
    }

    /* 最終的なディレクトリを作成 */
    md_makemaildir(s_id, targetdir);

    return R_SUCCESS;
}

/*
 * md_makemaildir
 *
 * 指定されたディレクトリを作成し、その配下に
 *   /new, /cur, /tmp
 * の3つのディレクトリを作成する
 * ※ディレクトリの作成に失敗した場合もエラーとしない
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *
 * 返り値
 *      なし
 */
static void
md_makemaildir(unsigned int s_id, char *dirname)
{
    /* 作成するサブディレクトリ一覧 */
    char *subdirs[] = {
                       "/new",
                       "/cur",
                       "/tmp",
                       NULL
                      };

    struct strset createpath;
    char *tmp;
    int ret, i;

    /* ベースディレクトリを作成 */
    md_mkdir(s_id, dirname);

    strset_init(&createpath);
    for (i = 0; subdirs[i] != NULL; i++) {
        /* MailDirをコピー */
        tmp = strdup(dirname);
        if (tmp == NULL) {
            SYSLOGERROR(ERR_S_MALLOC, s_id, "md_makemaildir", E_STR);
            exit(EXIT_MILTER);
        }
        strset_set(&createpath, tmp);

        /* サブディレクトリ名 (.../new, .../cur, .../tmp) を付加 */
        ret = strset_catstr(&createpath, subdirs[i]);
        if (ret == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
         
        /* サブディレクトリを作成 */
        md_mkdir(s_id, createpath.ss_str);

        strset_free(&createpath);
    }

    return;
}

/*
 * md_mkdir
 *
 * 指定されたディレクトリを作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *
 * 返り値
 *      R_SUCCESS               正常 (既にディレクトリが存在した場合も)
 *      R_ERROR                 エラー
 */
static int
md_mkdir(unsigned int s_id, char *dirname)
{
    struct stat stbuf;

    if (stat(dirname, &stbuf) < 0) {
        if (errno != ENOENT) {
            SYSLOGERROR(ERR_S_STAT, s_id, dirname, E_STR);
            return R_ERROR;
        }
        if (mkdir(dirname, 0700) < 0) {
            SYSLOGERROR(ERR_S_MKDIR, s_id, dirname, E_STR);
            return R_ERROR;
        }

        /* 作成した */
        return R_SUCCESS;

    } else {
        if (!S_ISDIR(stbuf.st_mode)) {
            SYSLOGERROR(ERR_S_NDIR, s_id, dirname, E_STR);
            return R_ERROR;
        }

        /* 既に存在した */
        return R_SUCCESS;
    }

    /* 念のため */
    return R_SUCCESS;
}

/*
 * md_makesavefilename
 *
 * 保存ファイル名 ("/new/.....") を作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct gzip *           gzip構造体
 *      char *                  ファイル名の格納先
 *      config *                設定変数
 *      int                     格納先の長さ
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
static int
md_makesavefilename(struct gzip *md, char *filename,
                    int filename_len, struct config * config, struct stat stbuf)
{
    /* ファイルのパス (/new/....) を作成する */
    snprintf(filename, filename_len, SAVEZIPFILENAME,
                md->md_recvtime, stbuf.st_ino, config->cf_msyhostname);

    return R_SUCCESS;
}


/*
 * md_makesavefile
 *
 * 一覧に含まれるディレクトリ配下に、保存ファイルをリンクする
 * ※リンクに失敗した場合はエラーとしない
 *
 * 引数
 *      unsigned int            セッションID
 *      struct gzip *       gzip構造体
 *      char *                  保存ファイル名
 *      struct strlist *        ディレクトリ一覧
 *
 * 返り値
 *      なし (リンクに失敗した場合も)
 */
static void
md_makesavefile(unsigned int s_id, struct gzip *md,
                            char *filename, struct strlist *dirlist)
{
    struct strlist *p;
    struct strset path;
    int ret;

    p = dirlist;
    while (p != NULL) {
        /* リンク先のファイルのフルパスを作成する */
        strset_init(&path);
        if (strset_catstr(&path, p->ss_data.ss_str) == -1 ||
            strset_catstr(&path, filename) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }

        /* ハードリンクを作成する */
        ret = link(md->md_tempfilepath, path.ss_str);
        if (ret < 0) {
            /* 失敗した場合はログ出力のみ */
            SYSLOGERROR(ERR_S_LINK, s_id, p->ss_data.ss_str, E_STR);
        }
        strset_free(&path);
        p = p->next;
    }

    return;
}

/*
 * md_list2str
 *
 * strlist形式の一覧からカンマ区切りの文字列を作成する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct strset *         格納先のstrset構造体のポインタ
 *      struct strlist *        一覧の先頭ポインタ
 *
 * 返り値
 *      なし
 */
static void
md_list2str(unsigned int s_id, struct strset *target, struct strlist *list_h)
{
    struct strset str;
    struct strlist *p;
    int ret;

    strset_init(&str);
    strset_init(target);

    p = list_h;
    while (p != NULL) {
        if (p != list_h) {
            /* 2つ目以降は ", " で繋げる */
            ret = strset_catstr(&str, ", ");
            if (ret < 0) {
                SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
                exit(EXIT_MILTER);
            }
        }
        ret = strset_catstrset(&str, &(p->ss_data));
        if (ret < 0) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
            exit(EXIT_MILTER);
        }
        p = p->next;
    }

    strset_set(target, str.ss_str);

    return;
}

/*
 * md_free
 *
 * gzip構造体を解放する
 *
 * 引数
 *      struct gzip *       gzip構造体のポインタ
 *
 * 返り値
 *      なし
 */
static void
md_free(struct gzip *md)
{
    if (md == NULL) {
        return;
    }

    if (md->md_tempfilepath != NULL) {
        free(md->md_tempfilepath);
        md->md_tempfilepath = NULL;
    }
    strset_free(&md->md_header_from);
    strset_free(&md->md_header_to);

    free(md);

    return;
}

int
gzip_mod_extra_config(struct config **cfg)
{
    return R_SUCCESS;
}
