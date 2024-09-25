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
#include <fcntl.h>


/* Messasy include file */
#include "messasy.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "utils.h"
#include "log.h"
#include "../lib_lm.h"

/* Header for my library */
#include "liblda.h"

#define MYMODULE "lda"
#define HEADER_FUNC     "lda_exec_header"
#define BODY_FUNC       "lda_exec_body"
#define EOM_FUNC        "lda_exec_eom"
#define ABORT_FUNC      "lda_exec_abort"
#define MODCONF_FUNC    "lda_exec_modconf"

static int lda_set_extra_config (char *, struct extra_config **, size_t);
static int lda_set_module_list (char *, char *, struct modulelist **);
static struct lda *md_struct_init(unsigned int, struct lda_config *,
                                        time_t, struct strset *,
                                        struct strlist *, struct strlist *);
static void md_list2str(unsigned int, struct strset *, struct strlist *);
static void md_free(struct lda *);

/* extern struct modulehandle *mhandle_list; */
struct modulehandle *mhandle_list;
char msy_hostname[MAX_HOSTNAME_LEN + 1];
struct cfentry lda_cfe[] = {
    {
        "LDACommand", CF_STRING, NULL,
        MESSASY_OFFSET(struct lda_config, cf_ldacommand), is_executable_file_size
    }
};

void exec_sig_catch(int sig)
{
    exit(-1);
}


/*
 * lda_init
 *
 * 機能:
 *    ldaモジュールの初期化関数
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
lda_init(struct cfentry **cfe, size_t *cfesize,
           struct config **cfg, size_t *cfgsize)
{
    struct config *new_cfg;
    struct cfentry *new_cfe;
    size_t new_cfesize, new_cfgsize;
    int ret, i;
    struct modulelist *tmp_list;

    /* モジュールリストへの追加 */
    ret = lda_set_module_list(MYMODULE, HEADER_FUNC, &(*cfg)->cf_exec_header);
    if (ret != 0) {
        return -1;
    }
    ret = lda_set_module_list(MYMODULE, BODY_FUNC, &(*cfg)->cf_exec_body);
    if (ret != 0) {
        /* ヘッダのメモリ開放*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    ret = lda_set_module_list(MYMODULE, EOM_FUNC, &(*cfg)->cf_exec_eom);
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
    ret = lda_set_module_list(MYMODULE, ABORT_FUNC, &(*cfg)->cf_exec_abort);
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
    new_cfgsize = *cfgsize + sizeof(struct lda_config);
    new_cfg = (struct config *)realloc(*cfg, new_cfgsize);
    if(new_cfg == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }
    memset((char *)new_cfg + *cfgsize, '\0', new_cfgsize - *cfgsize);
    *cfg = new_cfg;

    /* cfeの拡張 */
    new_cfesize = *cfesize + sizeof(lda_cfe);
    new_cfe = (struct cfentry *)realloc(*cfe, new_cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }

    /* lda_cfeのコピー */
    memcpy(new_cfe + *cfesize / sizeof(struct cfentry),
           &lda_cfe, sizeof(lda_cfe));

    /* dataoffsetの更新 */
    for (i = 0; i < MAILDROP_CFECOUNT; i++) {
        new_cfe[(*cfesize / sizeof(struct cfentry)) + i].cf_dataoffset += *cfgsize;
    }
    *cfe = new_cfe;

    /* モジュール毎のconfig構造体offsetを格納 */
    ret = lda_set_extra_config(MYMODULE, &(*cfg)->cf_extraconfig, *cfgsize);
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
 * lda_set_module_list
 *
 * 機能:
 *    ldaモジュール用のモジュールリスト作成
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
lda_set_module_list(char *modname, char *funcname, struct modulelist **list)
{
    struct modulelist *new_list;

    /* module名のポインタを格納する領域の確保 */
    new_list = (struct modulelist *)malloc(sizeof(struct modulelist));
    if(new_list == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }

    new_list->mlist_modulename = modname;
    new_list->mlist_funcname = funcname;
    new_list->mlist_next = *list;
    *list = new_list;

    return 0;
}

/*
 * lda_set_extra_config
 *
 * 機能:
 *    ldaモジュール用のextra configの作成
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
lda_set_extra_config(char *modname, struct extra_config **ext_cfg,
                        size_t cfgsize)
{
    struct extra_config *new_cfg;

    /* 外部モジュールのconfig構造体ポインタを格納する領域の確保 */
    new_cfg = (struct extra_config *)malloc(sizeof(struct extra_config));
    if(new_cfg == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }

    new_cfg->excf_modulename = modname;

    new_cfg->excf_config = (void *)cfgsize;
    new_cfg->excf_next = *ext_cfg;
    *ext_cfg = new_cfg;

    return 0;
}

/*
 * lda_free_config
 *
 * 機能:
 *    ldaのconfig領域をfreeする関数
 * 引数:
 *    mP     : priv構造体をつなぐ構造体
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
lda_free_config(struct config *cfg)
{
    struct lda_config *p = NULL;
    struct extra_config *exp;

    if (cfg == NULL || cfg->cf_extraconfig == NULL) {
        return -1;
    }

    for (exp = cfg->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
    if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
        p = (struct lda_config *)(exp->excf_config);
            break;
        }
    }

    /* pが見つかった場合*/
    if (p != NULL) {
        if (p->cf_ldacommand != NULL) {
            free(p->cf_ldacommand);
        }
    } else {
        return -1;
    }

    return 0;
}

/******************************
 * 内部関数
 ******************************/

/*
 * md_struct_init
 *
 * lda構造体の確保と初期化を行なう
 *
 * 引数
 *      unsigned int            セッションID
 *      struct config *         config構造体のポインタ
 *      time_t                  メール受信時刻
 *
 * 返り値
 *      struct lda *       lda構造体
 */
static struct lda *
md_struct_init(unsigned int s_id, struct lda_config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct lda *md;
    int ret;

    /* 領域を確保 */
    md = (struct lda *)malloc(sizeof(struct lda));
    if (md == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct lda));

    /* 受信日時を保存*/
    md->md_recvtime = time;

    /* カスタムヘッダを作成*/
    /* FROMヘッダの初期化*/
    strset_init(&md->md_header_from);

    /* 値格納*/
    ret = strset_catstrset(&md->md_header_from, from);
    if (ret == -1) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
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
 * lda_get_priv
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
lda_get_priv(struct mlfiPriv **priv)
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
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }
    /* MYMODULEの値をそのまま参考するので、開放しない */
    mp->expv_modulename = MYMODULE;
    /* NEXTに初期化*/
    mp->expv_next = NULL;
    /* プライベートの初期化*/
    mp->expv_modulepriv = NULL;

    /* 先頭の処理 */
    if (p_old == NULL) {
        (*priv)->mlfi_extrapriv = mp;

    /* 末尾に追加 */
    } else if (p == NULL) {
        p_old->expv_next = mp;
    }
    return mp;
}

/*
 * lda_priv_free
 *
 * 機能:
 *    すべてのpriv構造体をfreeする関数
 * 引数:
 *     extrapriv:   引数の構造体のポインタ(参照渡し)
 * 返値:
 *    無し
 */
void
lda_priv_free(struct extrapriv *expv)
{
    /* NULLチェック */
    if (expv != NULL) {
        /* lda_priv領域がある場合 */
        if (expv->expv_modulepriv != NULL) {
            /* lda_priv構造体のfree */
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
 * lda_abort
 *
 * メール保存処理を中止する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct lda *       lda構造体
 *
 * 返り値
 *      無し
 */
void
lda_abort(unsigned int s_id, struct lda *md)
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
 * lda_exec_header
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
lda_exec_header(struct mlfiPriv *priv, char *headerf, char *headerv)
{
    /* 変数宣言*/
    struct extrapriv     *expv;
    struct extra_config  *p;
    struct lda_priv *mypv;
    struct lda      *mydat;
    struct lda      *mydatp;
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
    expv = lda_get_priv(&priv);

    /* lda_get_privがエラーの時 */
    if (expv == NULL) {
        SYSLOGERROR("%s:%s:Cannot get private data.", MYMODULE, __func__);
        return -1;
    }

    /* lda_priv領域がなかったら作成 */
    if (expv->expv_modulepriv == NULL) {
        /* lda領域 */
        mypv = malloc(sizeof(struct lda_priv));
        if (mypv == NULL) {
            SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
            exit(EXIT_MILTER);
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
        mydat = lda_open(s_id, ((struct lda_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "lda_open");
            free(mypv);
            return -1;
        }
        
        /* 構造体をつなげる */
        mypv->mypriv = mydat;
    }

    /* lda構造体のポインタを変数に格納 */
    mydatp = ((struct lda_priv *)expv->expv_modulepriv)->mypriv;

    /* ヘッダを書き込み*/
    ret = lda_write_header(s_id, mydatp, headerf, headerv);
    if (ret != R_SUCCESS) {
        SYSLOGERROR("%s:%s:Cannot write header:%s", MYMODULE, __func__);
        return -1;
    }

    return 0;
}

/*
 * lda_exec_body
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
lda_exec_body(struct mlfiPriv *priv, u_char *bodyp, size_t bodylen)
{
    /* 変数宣言*/
    struct extrapriv     *expv;
    struct lda      *mydat;
    struct lda_priv *mypv;
    struct lda      *mydatp;
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
    expv = lda_get_priv(&priv);
    /* lda_get_privがエラーの時 */
    if (expv == NULL) {
         SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "lda_get_priv");
        return -1;
    }

    /* lda_priv領域がなかったら作成 */
    if (expv->expv_modulepriv == NULL) {
        /* lda領域 */
        mypv = malloc(sizeof(struct lda_priv));
        if (mypv == NULL) {
             SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
             exit(EXIT_MILTER);
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
        mydat = lda_open(s_id, ((struct lda_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "lda_open");
            free(mypv);
            return -1;
        }
        
        /* 構造体をつなげる */
        mypv->mypriv = mydat;
    }

    /* メールデータを取得*/
    mydat = ((struct lda_priv *)expv->expv_modulepriv)->mypriv;

    /* ボディ書き込み */
    ret = lda_write_body(s_id, mydat, bodyp, bodylen);
    if (ret != R_SUCCESS) {
        SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "lda_write_body");
        return -1;
    }

    return 0;
}

/*
 * lda_exec_eom
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
lda_exec_eom(struct mlfiPriv *priv)
{
    /* 変数宣言*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct lda           *mydat;
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
                mydat = ((struct lda_priv *)p->expv_modulepriv)->mypriv;
                /* クローズ*/
                ret = lda_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, lda_close);
                    return -1;
                }
                /* 一つ前の構造体のnextにfreeする構造体のnextをつなげる*/
                p_old->expv_next = p->expv_next;
                /* プライベートデータを開放する*/
                lda_priv_free(p);

            }
        /* Pが先頭の場合*/
        } else {
            if (p != NULL) {
                mydat = ((struct lda_priv *)p->expv_modulepriv)->mypriv;
                /* クローズ*/
                ret = lda_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, lda_close, lda_close);
                    return -1;
                }
                /* 一つ前のmlfi構造体にfreeする構造体のnextをつなげる*/
                priv->mlfi_extrapriv = p->expv_next;
                /* プライベート情報開放*/
                lda_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * lda_exec_abort
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
lda_exec_abort(struct mlfiPriv *priv)
{
    /* 変数宣言*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct lda     *md;
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
                md = ((struct lda_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                lda_abort(s_id, md);
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                p_old->expv_next = p->expv_next;
                lda_priv_free(p);
            }

        /* Pが先頭の場合*/
        } else {
            if (p != NULL) {
                md = ((struct lda_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                lda_abort(s_id, md);
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                priv->mlfi_extrapriv = p->expv_next;
                lda_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * lda_open
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
 *      struct lda *       正常
 *      NULL                    エラー (一時ファイルのオープンに失敗)
 */
#define TMP_DIR "/tmp"
#define RETRY_LIMIT 3
struct lda *
lda_open(unsigned int s_id, struct lda_config *config,
                time_t time, struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h, char *msy_hostname)
{
    struct lda *md;       // lda構造体
    mode_t old_umask;
    int retry_count = 0;

    /* lda構造体を初期化 */
    md = md_struct_init(s_id, config, time, from, to_h, saveaddr_h);

    /* 一時ファイルのパスを作成 */
    size_t path_len = strlen(TMP_DIR) + 1 + 6 + 1; // "/tmp/XXXXXX"

    // メモリの割り当て
    md->md_tempfilepath = (char *)malloc(path_len);
    if (md->md_tempfilepath == NULL) {
        SYSLOGERROR("%s:%s:Cannot allocate memory:%s", MYMODULE, __func__, strerror(errno));
        exit(EXIT_MILTER);
    }

    // 一時ファイルが重複して作成失敗した場合に備えたリトライ
    while (retry_count < RETRY_LIMIT) {
        // 一時ファイルパスのテンプレートを作成
        // XXXXXXを渡すのはmkstemの仕様に基づく
        snprintf(md->md_tempfilepath, path_len, "%s/XXXXXX", TMP_DIR);

        /* 一時ファイルをオープン */
        old_umask = umask(0077);
        md->md_tempfile_fd = mkstemp(md->md_tempfilepath);
        umask(old_umask);

        if (md->md_tempfile_fd >= 0) {
            // 成功した場合、ループを抜ける
            break;
        }

        retry_count++;
    }

    if (md->md_tempfile_fd < 0) {
        SYSLOGERROR("%s:%s:Cannot create tmp file", MYMODULE, __func__);
        free(md->md_tempfilepath); 
        return NULL;
    }

    return md;
}

/*
 * lda_write_header
 *
 * ヘッダを一時ファイルに出力する
 * カスタムヘッダを初めに書き込む
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct lda *       lda構造体
 *      char *                  ヘッダフィールド (コールバックに渡されたまま)
 *      char *                  ヘッダ値 (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
lda_write_header(unsigned int s_id, struct lda *md,
                        char *headerf, char *headerv)
{
    char *header, *p;
    int header_len;
    ssize_t written_len;
    int ret;

    if (!md->md_writing_header) {
        /* はじめにカスタムヘッダを書き込む */
        md->md_writing_header = 1;
        ret = lda_write_header(s_id, md, CUSTOMHDR_FROM,
                                    md->md_header_from.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
        ret = lda_write_header(s_id, md, CUSTOMHDR_TO,
                                    md->md_header_to.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* ヘッダの書き込み */
    header_len = strlen(headerf) + ((headerv == NULL)?0:strlen(headerv)) + 3; /* 文字列 + ': ' + '\n' */
    header = (char *)malloc(header_len + 1);    /* '\0' */
    if (header == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "lda_write_header", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(header, "%s: %s\n", headerf, (headerv == NULL)?"":headerv);

    p = header;
    written_len = 0;
    while (written_len < header_len) {
        written_len = write(md->md_tempfile_fd, p, header_len);
        if (written_len < 0) {
            SYSLOGERROR("%s:%s:Cannot write tmp file:%s", MYMODULE, __func__, E_STR);
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
 * lda_write_body
 *
 * メールボディを一時ファイルに出力する
 * ヘッダとボディの区切りを初めに書き込む
 * 改行文字はCRLFをLFに統一する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct lda *       lda構造体
 *      unsigned char *         ボディ (コールバックに渡されたまま)
 *      size_t                  長さ (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
lda_write_body(unsigned int s_id, struct lda *md,
                    unsigned char *bodyp, size_t len)
{
    ssize_t written_len;
    int ret;
    int i;

    if (!md->md_writing_body) {
        /* はじめにヘッダとボディの区切り文字を書き込む */
        md->md_writing_body = 1;
        ret = lda_write_body(s_id, md, (unsigned char *) "\n", 1);
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
                    SYSLOGERROR("%s:%s:Cannot write tmp file:%s", MYMODULE, __func__, E_STR);
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
            SYSLOGERROR("%s:%s:Cannot write tmp file:%s", MYMODULE, __func__, E_STR);
            return R_ERROR;
        }
    }

    return R_SUCCESS;
}

/*
 * lda_close
 *
 * 一時ファイルをクローズし、保存する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct lda *           lda構造体
 *      struct config *         config構造体
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
#define MAX_COMMAND_LENGTH 1280
int
lda_close(unsigned int s_id, struct lda *md, struct config * config)
{
    int status;
    size_t ret_s;
    int ret;

    struct lda_config *ldacf = NULL;
    struct extra_config *exp = NULL;


    /* 一時ファイルをクローズ */
    if (md->md_tempfile_fd > 0) {
        /* 改行文字扱いでないCRが残っている場合は書き込む */
        if (md->md_cr) {
            ret_s = write(md->md_tempfile_fd, "\r", 1);
            if (ret_s < 0) {
                SYSLOGERROR("%08X:%s:%s:Faild to write temp file(file: %s: %s)",s_id, MYMODULE, __func__, md->md_tempfilepath, E_STR);
                return R_ERROR;
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
            ldacf = (struct lda_config *)(exp->excf_config);
            break;
        }
    }

    /* configが無かったらエラー*/
    if (ldacf == NULL) {
        SYSLOGERROR("%08X:%s:%s:Cannot read configuration.",s_id, MYMODULE, __func__);
        return R_ERROR;
    }


    struct strlist *current_addr = md->md_saveaddr_h;
    FILE *mail_file;

    // 一時ファイルを開く
    mail_file = fopen(md->md_tempfilepath, "r");
    if (mail_file == NULL) {
        SYSLOGERROR("%08X:%s:%s:Cannot open tmp file.",s_id, MYMODULE, __func__);
        return R_ERROR;
    }

    // コマンドのフォーマットを整形するための変数
    char command[1024];
    char *orig_cmd = ldacf->cf_ldacommand;
    char *end = command + MAX_COMMAND_LENGTH - 1;
    char *writer = command;
    int m_found = 0;

    // コマンドの設定が\0に到達、書き込み領域のポインタが末尾を超えるまで
    while (*orig_cmd && writer < end) {
        // %mの判定
        if (orig_cmd[0] == '%' && orig_cmd[1] == 'm') {
            // %mが既に見つかっているケース
            if (m_found > 0) {
                // ポインタを2進めて重複した%mをクリアする
                // sprintfの誤動作とバッファオーバフローの対策
                orig_cmd += 2;
                SYSLOGERROR("%08X:%s:%s:Command contains multiple %m. The second and subsequent %m will be replaced with blanks.",s_id, MYMODULE, __func__);
                continue;
            }

            // %mが初めて見つかった場合、を%とsを代入
            writer[0] = '%';
            writer[1] = 's';

            // ポインタを進める
            writer += 2;
            orig_cmd += 2;

            // %mが見つかったフラグon
            m_found = 1;
            continue;
        } 
        *writer = *orig_cmd;
        writer++;
        orig_cmd++;
    }
    *writer = '\0';

    while (current_addr != NULL) {
        pid_t pid;
        int pipefd[2];

        char exec_command[1024];
        char *addr_ptr = current_addr->ss_data.ss_str;
        int addr_len = current_addr->ss_data.ss_len;

        // %mがあればアドレスを置換して実行するコマンドを作成する
        if (m_found > 0) {
            // command + addrの長さの検査
            // ファイルサイズ(1024) + メールアドレス(256) - \0を超えている場合は処理しない
            if (MAX_COMMAND_LENGTH - 1 < strlen(command) + 256) {
                SYSLOGERROR("%08X:%s:%s:filepath  after command replacement exceeds 1024+256 bytes(command: %s, replace_addr: %s)",
                            s_id, MYMODULE, __func__, command, current_addr->ss_data.ss_str);
                continue;
            }
            // %m(%s)をメールアドレスに置換
            sprintf(exec_command, command, current_addr->ss_data.ss_str);
        } else {
            // なければcommandをそのままコピーして利用する
            strcpy(exec_command, command);
        }

         // パイプの作成
        if (pipe2(pipefd, O_CLOEXEC) == -1) {
            SYSLOGERROR("%08X:%s:%s:Faild to create pipe.",s_id, MYMODULE, __func__);
            continue;
        }

        // 子プロセスの作成
        pid = fork();

        if (pid == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            fclose(mail_file);
            return R_ERROR;
        } else if (pid == 0) {
            // 子プロセス
            signal(SIGALRM, exec_sig_catch);
            alarm(60);

            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            // コマンドの実行
            execl("/bin/sh", "sh", "-c", exec_command, (char *)NULL);
            exit(1);
        } else {
            // 親プロセス
            close(pipefd[0]);

            // メールデータの書き込み
            char buffer[4096];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), mail_file)) > 0) {
                if (write(pipefd[1], buffer, bytes_read) != bytes_read) {
                    SYSLOGERROR("%08X:%s:%s:Faild to write pipe.",s_id, MYMODULE, __func__);
                    break;
                }
            }

            close(pipefd[1]);

            // 子プロセスの終了を待つ
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                SYSLOGERROR("%08X:%s:%s:Command failed: %s",s_id, MYMODULE, __func__, exec_command);
            }

            // ファイルポインタを先頭に戻す
            rewind(mail_file);
        }

        current_addr = current_addr->next;
    }

    // 不要になったリソースの解放
    fclose(mail_file);
    unlink(md->md_tempfilepath);
    md_free(md);
    md = NULL;

    return 0;
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
                SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "strset_catstr");
                exit(EXIT_MILTER);
            }
        }
        ret = strset_catstrset(&str, &(p->ss_data));
        if (ret < 0) {
            SYSLOGERROR("%s:%s:Faild to function:%s.", MYMODULE, __func__, "strset_catstr");
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
 * lda構造体を解放する
 *
 * 引数
 *      struct lda *       lda構造体のポインタ
 *
 * 返り値
 *      なし
 */
static void
md_free(struct lda *md)
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
lda_mod_extra_config(struct config **cfg)
{
    return R_SUCCESS;
}
