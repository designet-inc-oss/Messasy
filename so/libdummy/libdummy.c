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
 * $RCSfile: $
 * $Revision: $
 * $Date: $
 */

#define _XOPEN_SOURCE
#define _GNU_SOURCE
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
#include <tcutil.h>
#include <tcrdb.h>
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

// Messasy include file
#include "../messasy.h"
#include "../msy_config.h"
#include "../msy_readmodule.h"
#include "../utils.h"
#include "../log.h"
//#include "lib_lm.h"
#include "../lib_lm.h"

// Header for my library
#include "libdummy.h"

#define MYMODULE "dummy"
#define SUBJECT "subject"

#define HEADER_FUNC     "dummy_exec_header"
#define BODY_FUNC       "dummy_exec_body"
#define EOM_FUNC        "dummy_exec_eom"
#define ABORT_FUNC      "dummy_exec_abort"
#define MODCONF_FUNC    "dummy_exec_modconf"

// prorotype declaration of local functions
int dummy_set_extra_config (char *, struct extra_config **, size_t);
int dummy_set_module_list (char *, char *, struct modulelist **);

//extern struct modulehandle *mhandle_list;
struct modulehandle *mhandle_list;
char msy_hostname[MAX_HOSTNAME_LEN + 1];

struct cfentry dummy_cfe[] = {
    {
        "Dummy", CF_STRING, NULL,
        OFFSET(struct dummy_config, cf_dummy), NULL
    }
};

/*
 * dummy_init
 *
 * 機能:
 *    dummyモジュールの初期化関数
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
dummy_init(struct cfentry **cfe, size_t *cfesize,
           struct config **cfg, size_t *cfgsize)
{
    struct config *new_cfg;
    struct cfentry *new_cfe;
    size_t new_cfesize, new_cfgsize;
    int ret, i;

    // モジュールリストへの追加
    ret = dummy_set_module_list(MYMODULE, HEADER_FUNC, &(*cfg)->cf_exec_header);
    if (ret != 0) {
        return -1;
    }
    ret = dummy_set_module_list(MYMODULE, BODY_FUNC, &(*cfg)->cf_exec_body);
    if (ret != 0) {
        return -1;
    }
    ret = dummy_set_module_list(MYMODULE, EOM_FUNC, &(*cfg)->cf_exec_eom);
    if (ret != 0) {
        return -1;
    }
    ret = dummy_set_module_list(MYMODULE, ABORT_FUNC, &(*cfg)->cf_exec_abort);
    if (ret != 0) {
        return -1;
    }

    // cfgの拡張
    new_cfgsize = *cfgsize + sizeof(struct dummy_config);
    new_cfg = (struct config *)realloc(*cfg, new_cfgsize);
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }
    *cfg = new_cfg;

    // cfeの拡張
    new_cfesize = *cfesize + sizeof(dummy_cfe);
    new_cfe = (struct cfentry *)realloc(*cfe, new_cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }

    // dummy_cfeのコピー
    memcpy(new_cfe + *cfesize / sizeof(struct cfentry),
           &dummy_cfe, sizeof(dummy_cfe));

    // dataoffsetの更新
    for (i = 0; i < MAILDROP_CFECOUNT; i++) {
        new_cfe[(*cfesize / sizeof(struct cfentry)) + i].cf_dataoffset += *cfgsize;
    }
    *cfe = new_cfe;

    // モジュール毎のconfig構造体offsetを格納
    ret = dummy_set_extra_config(MYMODULE, &(*cfg)->cf_extraconfig, *cfgsize);
    if (ret != 0) {
        return -1;
    }

    // cfesize, cfgsizeの更新
    *cfesize = new_cfesize;
    *cfgsize = new_cfgsize;

    return 0;
}

/*
 * dummy_set_module_list
 *
 * 機能:
 *    dummyモジュール用のモジュールリスト作成
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
dummy_set_module_list (char *modname, char *funcname, struct modulelist **list)
{
    struct modulelist *new_list;

    /* module名のポインタを格納する領域の確保 */
    new_list = (struct modulelist *)malloc(sizeof(struct modulelist));
    if(new_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }

    new_list->mlist_modulename = strdup(modname);
    if(new_list->mlist_modulename == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
	free(new_list);
        return (-1);
    }
    new_list->mlist_funcname = strdup(funcname);
    if(new_list->mlist_funcname == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
	free(new_list->mlist_modulename);
	free(new_list);
        return (-1);
    }
    new_list->mlist_next = *list;
    *list = new_list;

    return 0;
}

/*
 * dummy_set_extra_config
 *
 * 機能:
 *    dummyモジュール用のextra configの作成
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
dummy_set_extra_config (char *modname, struct extra_config **ext_cfg,
                        size_t cfgsize)
{
    struct extra_config *new_cfg;

    /* 外部モジュールのconfig構造体ポインタを格納する領域の確保 */
    new_cfg = (struct extra_config *)malloc(sizeof(struct extra_config));
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }

    new_cfg->excf_modulename = strdup(modname);
    if(new_cfg->excf_modulename == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
	free(new_cfg);
        return (-1);
    }
    new_cfg->excf_config = (void *)cfgsize;
    new_cfg->excf_next = *ext_cfg;
    *ext_cfg = new_cfg;

    return 0;
}

/*
 * dummy_free_config
 *
 * 機能:
 *    dummyのconfig領域をfreeする関数
 * 引数:
 *    mP     : priv構造体をつなぐ構造体
 * 返値:
 *     0: 正常
 *    -1: 異常
 */
int
dummy_free_config(struct config *cfg)
{
    struct dummy_config *p = NULL;
    struct extra_config *exp;

    if (cfg == NULL || cfg->cf_extraconfig == NULL) {
        return (R_SUCCESS);
    }

    for (exp = cfg->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
	if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
	    p = (struct dummy_config *)(exp->excf_config);
	    break;
	}
    }

    if (p->cf_dummy != NULL) {
        free(p->cf_dummy);
    }

    return (R_SUCCESS);
}

/***** ***** ***** ***** *****
 * 内部関数
 ***** ***** ***** ***** *****/

/*
 * md_struct_init
 *
 * maildrop構造体の確保と初期化を行なう
 *
 * 引数
 *      unsigned int            セッションID
 *      struct config *         config構造体のポインタ
 *      time_t                  メール受信時刻
 *
 * 返り値
 *      struct maildrop *       maildrop構造体
 */
static struct dummy *
dummy_struct_init(unsigned int s_id, struct dummy_config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct dummy *md;
    //int ret;

    /* 領域を確保 */
    md = (struct dummy *)malloc(sizeof(struct dummy));
    if (md == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "md_struct_init", E_STR);
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct dummy));

    return md;
}

/*
 * dummy_free
 *
 * maildrop構造体を解放する
 *
 * 引数
 *      struct maildrop *       maildrop構造体のポインタ
 *
 * 返り値
 *      なし
 */
static void
dummy_free(struct dummy *md)
{
    if (md == NULL) {
        return;
    }

    if (md->dummy_str != NULL) {
        free(md->dummy_str);
        md->dummy_str = NULL;
    }

    free(md);

    return;
}


/*
 * dummy_get_priv
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
dummy_get_priv(struct mlfiPriv **priv)
{
    struct extrapriv *p = NULL;      /* 検索用 */
    struct extrapriv *mp = NULL;     /* 新規作成用 */
    struct extrapriv *p_old = NULL;  /* 検索中、ひとつ前のポインタ保存用 */

    if (*priv != NULL) {
        /* 自分のpriv構造体があるか検索 */
        for (p = (*priv)->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (strcmp(MYMODULE, p->expv_modulename) == 0) {
                /* あったらリターン */
                return (p);
            }
            /* ひとつ前のポインタ格納 */
            p_old = p;
        }
    }
    /* 自分用のextrapriv領域新規作成 */
    mp = malloc(sizeof(struct extrapriv));
    if (mp == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_get_priv", E_STR);
        return (NULL);
    }
    /* 値の格納 */
    mp->expv_modulename = MYMODULE;
    mp->expv_next = NULL;
    mp->expv_modulepriv = NULL;

    /* 何も存在していなかったら先頭にポインタを付ける */
    if (p_old == NULL) {
        (*priv)->mlfi_extrapriv = mp;

    /* 存在しているが、自分用がなかったら後ろにつける */
    } else if (p == NULL) {
        p_old->expv_next = mp;
    }
    return (mp);
}

/*
 * dummy_priv_free
 *
 * 機能:
 *    すべてのpriv構造体をfreeする関数
 * 引数:
 *     extrapriv:   引数の構造体のポインタ(参照渡し)
 * 返値:
 *    無し
 */
void
dummy_priv_free(struct extrapriv *expv)
{

    /* NULLチェック */
    if (expv != NULL) {
        /* maildrop_priv領域がある場合 */
        if (expv->expv_modulepriv != NULL) {
            /* maildrop_priv構造体のfree */
            free(expv->expv_modulepriv);
            expv->expv_modulepriv = NULL;
        }
        /* extrapriv領域のfree */
        free(expv);
        expv = NULL;
    }
    return;
}

void
dummy_abort(unsigned int s_id, struct dummy *md)
{
    return;
}

/*
 * dummy_exec_header
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
dummy_exec_header(struct mlfiPriv *priv, char *headerf, char *headerv)
{

    struct extrapriv     *expv = NULL;
    struct extra_config  *p = NULL;
    struct dummy_priv    *mypv = NULL;
    struct dummy         *mydat = NULL;
    struct dummy         *mydatp = NULL;
    //int                  ret = 0;
    //unsigned int         s_id = priv->mlfi_sid;

    /* extrapriv領域の有無 */
    expv = dummy_get_priv(&priv);
    /* dummy_get_privがエラーの時 */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "dummy_exec_header", "dummy_get_priv");
    /* dummy_priv領域がなかったら作成 */
    } else if (expv->expv_modulepriv == NULL) {
        /* dummy領域 */
        mypv = malloc(sizeof(struct dummy_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "dummy_exec_header", E_STR);
            return(-1);
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
        /* 構造体をつなげる */
        mypv->mypriv = mydat;
    }
    /* maildrop構造体のポインタを変数に格納 */
    mydatp = ((struct dummy_priv *)expv->expv_modulepriv)->mypriv;

    return SMFIS_CONTINUE;
}

/*
 * dummy_exec_body
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
dummy_exec_body(struct mlfiPriv *priv, u_char *bodyp, size_t bodylen)
{
    struct extrapriv    *expv = NULL;
    struct dummy     *mydat = NULL;
//    int                 ret = 0;
//    unsigned int        s_id = priv->mlfi_sid;

    /* extrapriv領域の有無 */
    expv = dummy_get_priv(&priv);
    /* maildrop_get_privがエラーの時 */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "dummy_exec_body", "dummy_get_priv");
        return (-1);
    }
    if (expv->expv_modulepriv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "dummy_exec_body", "dummy_get_priv is NULL");
        exit (-1);
    }
    mydat = ((struct dummy_priv *)expv->expv_modulepriv)->mypriv;
    return SMFIS_CONTINUE;
}

/*
 * dummy_exec_eom
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
dummy_exec_eom(struct mlfiPriv *priv)
{
//    struct extrapriv    *p = NULL;
//    struct extrapriv    *p_old = NULL;
//    struct dummy        *mydat= NULL;
//    int                 ret = 0;
//    unsigned int        s_id = priv->mlfi_sid;

    return (0);
}

/*
 * dummy_exec_abort
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
dummy_exec_abort(struct mlfiPriv *priv)
{
    struct extrapriv    *p = NULL;
    struct extrapriv    *p_old = NULL;
    struct dummy        *md = NULL;
    unsigned int        s_id = priv->mlfi_sid;

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
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                p_old->expv_next = p->expv_next;
                md = ((struct dummy_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                dummy_abort(s_id, md);
                dummy_priv_free(p);
            } else {
               p_old->expv_next = NULL;
            }
        /* 一つ前の領域がmlfiPriv構造体 */
        } else {
            if (p != NULL) {
                /* ひとつ前の構造体のnextにfreeする構造体のnextをつなげる */
                priv->mlfi_extrapriv = p->expv_next;
                md = ((struct dummy_priv *)p->expv_modulepriv)->mypriv;
                /* アボート */
                dummy_abort(s_id, md);
                dummy_priv_free(p);
            } else {
                priv->mlfi_extrapriv = NULL;
            }
        }
    }
    return (0);
}

int
dummy_mod_extra_config(struct config **cfg)
{
    return (R_SUCCESS);
}
