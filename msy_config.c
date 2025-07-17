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
#include <limits.h>
#include <string.h>
#include <libdgstr.h>
#include <libmilter/mfapi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include <stddef.h> 

#define SYSLOG_NAMES
#include <libdgconfig.h>

#include <ldap.h>

#include "messasy.h"
#include "msy_config.h"
#include "utils.h"
#include "log.h"
#include "msy_readmodule.h"

/* プロトタイプ宣言 */
static char * is_timeout(int value);
static char * is_commandmaxclients(int value);
static char * is_erroraction(char *str);
static char * is_savepolicy(char *str);

static char * is_not_null(char *str);
static int conv_erroraction(struct config *cfg);
static int conv_savepolicy(struct config *cfg);
static int conv_ldapscope(struct config *cfg);
static int conv_saveignoreheader(struct config *cfg);
static int conv_config(struct config *cfg);
static int set_dlsymbol(char *, void *, struct modulelist **);

struct cfentry cfe[] = {
    {
        "ListenIP", CF_STRING,"127.0.0.1",
        MESSASY_OFFSET(struct config, cf_listenip), is_ipaddr
    },
    {
        "ListenPort", CF_INT_PLUS, "20026",
        MESSASY_OFFSET(struct config, cf_listenport),is_port
    },
    {
        "TimeOut", CF_INT_PLUS, "10",
        MESSASY_OFFSET(struct config, cf_timeout), is_timeout
    },
    {
        "SyslogFacility" , CF_STRING, "local1",
        MESSASY_OFFSET(struct config, cf_syslogfacility), is_syslog_facility
    },
    {
        "ErrorAction", CF_STRING, "tempfail",
        MESSASY_OFFSET(struct config, cf_erroraction), is_erroraction
    },
    {
        "CommandPort", CF_INT_PLUS, "17777",
        MESSASY_OFFSET(struct config, cf_commandport), is_port
    },
    {
        "AdminPassword", CF_STRING, NULL,
        MESSASY_OFFSET(struct config, cf_adminpassword), NULL
    },
    {
        "CommandMaxClients", CF_INT_PLUS, "16",
        MESSASY_OFFSET(struct config, cf_commandmaxclients), is_commandmaxclients
    },
    {
        "CommandTimeOut", CF_INT_PLUS, "300",
        MESSASY_OFFSET(struct config, cf_commandtimeout), is_timeout
    },
    {
        "SavePolicy", CF_STRING, "both",
        MESSASY_OFFSET(struct config, cf_savepolicy), is_savepolicy
    },
    {
        "MyDomain", CF_STRING, NULL,
        MESSASY_OFFSET(struct config, cf_mydomain), NULL
    },
    {
        "SaveMailAddress", CF_STRING, NULL,
        MESSASY_OFFSET(struct config, cf_savemailaddress), NULL
    },
    {
        "SaveIgnoreHeader", CF_STRING, "",
        MESSASY_OFFSET(struct config, cf_saveignoreheader), NULL
    },
    {
        "DefaultDomain", CF_STRING, "localhost.localdomain",
        MESSASY_OFFSET(struct config, cf_defaultdomain), is_not_null
    },
    {
        "LdapCheck", CF_INTEGER, "0",
        MESSASY_OFFSET(struct config, cf_ldapcheck), is_boolean
    },
    {
        "LdapServer", CF_STRING, "127.0.0.1",
        MESSASY_OFFSET(struct config, cf_ldapserver), is_ipaddr
    },
    {
        "LdapPort", CF_INT_PLUS, "389",
        MESSASY_OFFSET(struct config, cf_ldapport), is_port
    },
    {
        "LdapBindDn", CF_STRING, "",
        MESSASY_OFFSET(struct config, cf_ldapbinddn), NULL
    },
    {
        "LdapBindPassword", CF_STRING, "",
        MESSASY_OFFSET(struct config, cf_ldapbindpassword), NULL
    },
    {
        "LdapBaseDn", CF_STRING, "",
        MESSASY_OFFSET(struct config, cf_ldapbasedn), NULL
    },
    {
        "LdapMailFilter", CF_STRING, "(mail=%M)",
        MESSASY_OFFSET(struct config, cf_ldapmailfilter), NULL
    },
    {
        "LdapScope", CF_STRING, "subtree",
        MESSASY_OFFSET(struct config, cf_ldapscope), is_ldapscope
    },
    {
        "LdapTimeout", CF_INT_PLUS, "5",
        MESSASY_OFFSET(struct config, cf_ldaptimeout), is_timeout
    }
};

extern struct modulehandle *mhandle_list;

static char config_path[PATH_MAX + 1];

static pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;
static int config_reloading = 0;

struct config *cur_cfg = NULL;

/*
 * set_environment
 *
 * 環境変数を設定する
 *
 * 引数
 *      char *          設定ファイルのパス
 *
 * 返り値
 *      なし
 */
void
set_environment(char *path)
{
    /* 設定ファイルのパスを静的変数にセット */
    strncpy(config_path, path, PATH_MAX);
    config_path[PATH_MAX] = '\0';

    /* ログ出力レベルをINFOに設定 */
    dgconfig_loglevel = LOGLVL_INFO;

    /* ログ出力先を初期化 */
    dgloginit();
}

/*
 * init_config
 *
 * 機能
 *     コンフィグファイルを格納する構造体の初期化
 *
 * 引数
 *     なし
 *
 * 返り値
 *     struct config *     正常
 */
struct config *
init_config()
{
    struct config *cfg = NULL;

    /* メモリ確保 */
    cfg = (struct config *)malloc(sizeof(struct config));
    if (cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "init_config", E_STR);
        exit (EXIT_MAIN);
    }

    /* 構造体を0で埋める */
    memset(cfg, '\0', sizeof(struct config)); 

    /* mutex変数の初期化 */
    cfg->cf_ref_count_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    return cfg;
}

/*
 * free_modulelist
 *
 * 機能
 *　　　モジュールリストの開放
 *
 * 引数
 *      struct modulelist *list   開放するモジュールリスト
 *
 * 返り値
 *             なし
 *
 */
void
free_modulelist(struct modulelist *list)
{
    struct modulelist *p, *next;

    for (p = list; p != NULL; p = next) {
        next = p->mlist_next;
        free(p);
    }
}

/*
 * free_excf
 *
 * 機能
 *      extra_configの解放
 *
 * 引数
 *      struct extra_config *list   解放するモジュールリスト
 *
 * 返り値
 *             なし
 *
 */
void
free_excf(struct extra_config *list)
{
    struct extra_config *p, *next;

    for (p = list; p != NULL; p = next) {
        next = p->excf_next;
        free(p);
    }
}

/*
 * free_config
 *
 * 機能
 *　　　設定ファイルを読み込んだ構造体のメモリを開放する。
 *
 * 引数
 *      struct config *cfg       開放する設定ファイル構造体
 *
 * 返り値
 *             なし
 *
 */
void
free_config(struct config *cfg)
{
    struct modulehandle *p;
    char   funcname[MAXFUNCNAME];
    void   (*func_pointer)(struct config *);
    char  *error;

    if (cfg == NULL) {
        return;
    }


    /* extraconfigのリスト開放*/
    for (p = mhandle_list; p != NULL; p = p->mh_next) {

        /* 関数名の生成 */
        sprintf(funcname, "%s_free_config", p->mh_modulename);

        /* 関数ポインタの代入 */
        func_pointer = dlsym(p->mh_ptr, funcname);
        if ((error =dlerror()) != NULL) {
            SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, 
                        "free_config", funcname, error);
            continue;
        }

        (*func_pointer)(cfg);
    }

    /* execハンドラーの開放*/
    free_modulelist(cfg->cf_exec_header);
    free_modulelist(cfg->cf_exec_body);
    free_modulelist(cfg->cf_exec_eom);
    free_modulelist(cfg->cf_exec_abort);

    /* 上記にextraconfigのリスト開放しましたため、先頭のポイントだけを開放する*/
    free_excf(cfg->cf_extraconfig);

    if (cfg->cf_listenip != NULL) {
        free(cfg->cf_listenip);
    }
    if (cfg->cf_syslogfacility != NULL) {
        free(cfg->cf_syslogfacility);
    }
    if (cfg->cf_erroraction != NULL) {
        free(cfg->cf_erroraction);
    }
    if (cfg->cf_adminpassword != NULL) {
        free(cfg->cf_adminpassword);
    }
    if (cfg->cf_savepolicy != NULL) {
        free(cfg->cf_savepolicy);
    }
    if (cfg->cf_mydomain != NULL) {
        free(cfg->cf_mydomain);
    }
    if (cfg->cf_savemailaddress != NULL) {
        free(cfg->cf_savemailaddress);
    }
    if (cfg->cf_defaultdomain != NULL) {
        free(cfg->cf_defaultdomain);
    }
    if (cfg->cf_saveignoreheader != NULL) {
        free(cfg->cf_saveignoreheader);
    }
    if (cfg->cf_ldapserver != NULL) {
        free(cfg->cf_ldapserver);
    }
    if (cfg->cf_ldapbinddn != NULL) {
        free(cfg->cf_ldapbinddn);
    }
    if (cfg->cf_ldapbindpassword != NULL) {
        free(cfg->cf_ldapbindpassword);
    }
    if (cfg->cf_ldapbasedn != NULL) {
        free(cfg->cf_ldapbasedn);
    }
    if (cfg->cf_ldapmailfilter != NULL) {
        free(cfg->cf_ldapmailfilter);
    }
    if (cfg->cf_ldapscope != NULL) {
        free(cfg->cf_ldapscope);
    }
    if (cfg->cf_saveignoreheader_regex != NULL) {
        regfree(cfg->cf_saveignoreheader_regex);
        free(cfg->cf_saveignoreheader_regex);
    }
    if (cfg->cf_mydomain_list != NULL) {
        free_strlist(cfg->cf_mydomain_list);
    }
    if (cfg->cf_savemailaddress_list != NULL) {
        free_strlist(cfg->cf_savemailaddress_list);
    }

    free(cfg);
    return;
}

/*
 * config_retrieve
 *
 * config構造体の参照カウンタを増やし、ポインタを返す
 *
 * 引数
 *      なし
 * 返り値
 *      struct config *         config構造体のポインタ
 */
struct config *
config_retrieve()
{
    struct config *ret_ptr;

    pthread_mutex_lock(&cur_cfg->cf_ref_count_lock);

    /* 参照カウンタを増やす */
    cur_cfg->cf_ref_count++;
    /* config構造体のポインタを取得する */
    ret_ptr = cur_cfg;

    pthread_mutex_unlock(&cur_cfg->cf_ref_count_lock);

    return ret_ptr;
}

/*
 * config_release
 *
 * config構造体の参照カウンタを減らす
 *
 * 引数
 *      struct config *         config構造体のポインタ
 *                              (config_retrieve()で取得されたもの)
 * 返り値
 *      なし
 *
 */
void
config_release(struct config *cfg)
{
    pthread_mutex_lock(&cfg->cf_ref_count_lock);

    cfg->cf_ref_count--;

    if (cfg->cf_reloaded == TRUE && cfg->cf_ref_count < 1) {
        /* 他に誰も参照していなければ解放 */
        pthread_mutex_unlock(&cfg->cf_ref_count_lock);
        free_config(cfg);
        pthread_mutex_lock(&config_lock);
        config_reloading = FALSE;
        pthread_mutex_unlock(&config_lock);
    } else {
        pthread_mutex_unlock(&cfg->cf_ref_count_lock);
    }

    return;
}

/*
 * reload_config
 *
 * 設定ファイルの再読み込みを行なう
 *
 * 引数
 *      なし
 *
 * 返り値
 *      R_SUCCESS       正常
 *      R_POSITIVE      既にリロード中
 *      R_ERROR         エラー
 */
int
reload_config()
{
    struct config *cfg = NULL;
    struct config *old = NULL;
    int ret;

    if (config_reloading == TRUE) {

        /* リロード中 */
        SYSLOGERROR(ERR_CONFIG_RELOADING);
        return R_POSITIVE;
    }

    /* 設定ファイルを読み込み */
    ret = set_config(config_path, &cfg);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    if (cur_cfg != NULL) {
        config_reloading = TRUE;
    }

    /* ポインタを繋ぎ変え */
    pthread_mutex_lock(&config_lock);
    old = cur_cfg;
    cur_cfg = cfg;
    pthread_mutex_unlock(&config_lock);

    if (old != NULL) {
        pthread_mutex_lock(&old->cf_ref_count_lock);
        if (old->cf_ref_count < 1) {
            /* 他に誰も参照していなければ解放 */
            pthread_mutex_unlock(&old->cf_ref_count_lock);
            free_config(old);
            pthread_mutex_lock(&config_lock);
            config_reloading = FALSE;
            pthread_mutex_unlock(&config_lock);
        } else {
            /* 解放可能にマーク */
            old->cf_reloaded = TRUE;
            pthread_mutex_unlock(&old->cf_ref_count_lock);
        }
    }

    return R_SUCCESS;
}

/*
 * is_timeout
 *
 * 機能
 *     タイムアウト秒数のチェック
 *
 * 引数
 *     int value            チェックする値 
 *
 * 返り値
 *      NULL                正常
 *      ERR_CONF_TIMEOUT    エラーメッセージ
 */
char *
is_timeout(int value)
{
    if (value < 1 || value > MAX_TIME) {
        return ERR_CONF_TIMEOUT;
    } 
    return NULL;
}

/*
 * is_commandmaxclients
 *
 * 機能
 *     同時接続可能数のチェック
 *
 * 引数
 *     int value            チェックする値 
 *
 * 返り値
 *      NULL                正常
 *      ERR_CONF_COMMANDMAXCLIENTS    エラーメッセージ
 */
char *
is_commandmaxclients(int value)
{
    if (value < 1 || value > MAX_CONNECTION) {
        return ERR_CONF_COMMANDMAXCLIENTS;
    } 
    return NULL;
}

/*
 * is_erroraction
 *
 * 機能
 *     エラーアクションのチェック
 *
 * 引数
 *      char *str      チェックする文字列 
 *
 * 返り値
 *      NULL                    正常 
 *      ERR_CONF_ERRORACTION    エラーメッセージ
 */
char *
is_erroraction(char *str)
{
    if (strcasecmp(str, "accept") == 0) {
        return NULL;
    }
    if (strcasecmp(str, "reject") == 0) {
        return NULL;
    }
    if (strcasecmp(str, "tempfail") == 0) {
        return NULL;
    }
    return ERR_CONF_ERRORACTION;
}

/*
 * is_savepolicy 
 *
 * 機能
 *     セーブポリシーのチェック
 *
 * 引数
 *      char *str   チェックする文字列    
 *
 * 返り値
 *      NULL                   正常
 *      ERR_CONF_SAVEPOLICY    エラーメッセージ
 */
char *
is_savepolicy(char *str)
{
    if (strcasecmp(str, "both") == 0) {
        return NULL;
    }
    if (strcasecmp(str, "onlyfrom") == 0) {
        return NULL;
    }
    if (strcasecmp(str, "onlyto") == 0) {
        return NULL;
    }
    if (strcasecmp(str, "none") == 0) {
        return NULL;
    }
    return ERR_CONF_SAVEPOLICY;
}

/*
 * is_not_null
 *
 * 機能
 *    文字が入っているかのチェック 
 *
 * 引数
 *      char *str   チェックする文字列    
 *
 * 返り値
 *      NULL                 正常 
 *      ERR_CONF_DEFAULTDOMAIN    エラーメッセージ
 */
char *
is_not_null(char *str)
{
    if (str[0] == '\0') {
        return ERR_CONF_NULL;
    }
    return NULL;
}

/*
 * conv_erroraction
 *
 * 機能
 *    erroractionを変換して格納する処理
 *
 * 引数
 *    struct config *cfg   データを格納する構造体    
 *
 * 返り値
 *      R_SUCCESS    成功
 *      R_ERROR      失敗
 */
int
conv_erroraction(struct config *cfg)
{
    if (strcasecmp(cfg->cf_erroraction, "accept") == 0) {
        cfg->cf_erroraction_conv = SMFIS_ACCEPT;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_erroraction, "reject") == 0) {
        cfg->cf_erroraction_conv = SMFIS_REJECT;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_erroraction, "tempfail") == 0) {
        cfg->cf_erroraction_conv = SMFIS_TEMPFAIL;
        return R_SUCCESS;
    }

    return R_ERROR;
}

/*
 * conv_savepolicy
 *
 * 機能
 *    savepolicyを変換して格納する処理
 *
 * 引数
 *    struct config *cfg   データを格納する構造体    
 *
 * 返り値
 *      R_SUCCESS   成功
 *      R_ERROR     失敗
 */
int
conv_savepolicy(struct config *cfg)
{
    if (strcasecmp(cfg->cf_savepolicy, "both") == 0) {
        cfg->cf_savepolicy_conv = BOTH;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_savepolicy, "onlyfrom") == 0) {
        cfg->cf_savepolicy_conv = ONLYFROM;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_savepolicy, "onlyto") == 0) {
        cfg->cf_savepolicy_conv = ONLYTO;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_savepolicy, "none") == 0) {
        cfg->cf_savepolicy_conv = NONE;
        return R_SUCCESS;
    }

    return R_ERROR;
}

/*
 * conv_ldapscope
 *
 * 機能
 *    ldapscopeを変換して格納する処理
 *
 * 引数
 *    struct config *cfg   データを格納する構造体    
 *
 * 返り値
 *      R_SUCCESS    成功
 *      R_ERROR      失敗 
 */
int
conv_ldapscope(struct config *cfg)
{
    if (strcasecmp(cfg->cf_ldapscope, "onelevel") == 0) {
        cfg->cf_ldapscope_conv = LDAP_SCOPE_ONELEVEL;
        return R_SUCCESS;
    }
    if (strcasecmp(cfg->cf_ldapscope, "subtree") == 0) {
        cfg->cf_ldapscope_conv = LDAP_SCOPE_SUBTREE;
        return R_SUCCESS;
    }

    return R_ERROR;
}

/*
 * conv_saveignoreheader
 *
 * 機能
 *    saveignoreheaderを変換して格納する処理
 *
 * 引数
 *    struct config *cfg   データを格納する構造体    
 *
 * 返り値
 *      R_SUCCESS    成功
 *      R_ERROR      失敗
 */
int
conv_saveignoreheader(struct config *cfg)
{
    int ret;

    if (cfg->cf_saveignoreheader == NULL) {
        return R_SUCCESS;
    }

    if (cfg->cf_saveignoreheader[0] == '\0') {
        return R_SUCCESS;
    }

    cfg->cf_saveignoreheader_regex = (regex_t *)malloc(sizeof(regex_t));
    if (cfg->cf_saveignoreheader_regex == NULL) {
        SYSLOGERROR(ERR_MALLOC, "conv_saveignoreheader", E_STR);
        exit (EXIT_MAIN);
    }

    ret = regcomp(cfg->cf_saveignoreheader_regex, cfg->cf_saveignoreheader,
                  REG_EXTENDED);
    if (ret != 0) {
        free(cfg->cf_saveignoreheader_regex);
        cfg->cf_saveignoreheader_regex = NULL;
        return R_ERROR;
    }
    return R_SUCCESS;
}

/*
 * conv_config
 *
 * 機能
 *    configを変換して格納する処理
 *
 * 引数
 *    struct config *cfg   データを格納する構造体    
 *
 * 返り値
 *      R_SUCCESS    正常
 *      R_ERROR    異常
 */
int
conv_config(struct config *cfg)
{
    int ret;

    /* erroractionの変換 */
    ret = conv_erroraction(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_ERRORACTION);
        return R_ERROR;
    }
    /* savepolicyの変換 */
    ret = conv_savepolicy(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEPOLICY);
        return R_ERROR;
    }
    /* ldapscopenの変換 */
    ret = conv_ldapscope(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_LDAPSCOPE);
        return R_ERROR;
    }
    /* saveignoreheaderの変換 */
    ret = conv_saveignoreheader(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEIGNOREHEADER);
        return R_ERROR;
    }
    /* mydomainをリストに格納 */
    cfg->cf_mydomain_list = split_comma(cfg->cf_mydomain);
    /* savemailaddressをリストに格納 */
    cfg->cf_savemailaddress_list = split_comma(cfg->cf_savemailaddress);
    
    return R_SUCCESS;
}

/*
 * msy_module_init
 *
 * 機能
 *      各モジュールのinit関数を実行する
 *
 * 引数
 *      struct cfentry **cfe    config entry構造体(参照渡し)
 *      size_t cfesize          config entry構造体のサイズ(参照渡し)
 *      struct config  **cfg    config 構造体(参照渡し)
 *      size_t cfgsize          config 構造体のサイズ(参照渡し)
 *
 * 返り値
 *             0        正常
 *             1        異常
 *
 */
int
msy_module_init(struct cfentry **cfe, size_t *cfesize, struct config **cfg, size_t *cfgsize)
{
    struct modulehandle *p;
    char   funcname[MAXFUNCNAME];
    struct extra_config *excf;
    int check, ret;
    char  *error;
    int   (*func_pointer)(struct cfentry **, size_t *, struct config **, size_t *);

    for (p = mhandle_list; p != NULL; p = p->mh_next) {
        /* 関数名の生成 */
        sprintf(funcname, "%s_init", p->mh_modulename);

        /* 関数ポインタの代入 */
        func_pointer = dlsym(p->mh_ptr, funcname);
        if ((error = dlerror()) != NULL) {
            SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, "msy_module_init", funcname, error);
            return -1;
        }

        check = (*func_pointer)(cfe, cfesize, cfg, cfgsize);
        if (check != 0) {
            return -1;
        }
    }

    // ロードされているモジュール毎にlistを検索しdlsymを行う
    for (p = mhandle_list; p != NULL; p = p->mh_next) {
        ret = set_dlsymbol(p->mh_modulename, 
                           p->mh_ptr, &(*cfg)->cf_exec_header);
        if (ret != 0) {
            return -1;
        }
        ret = set_dlsymbol(p->mh_modulename, 
                           p->mh_ptr, &(*cfg)->cf_exec_body);
        if (ret != 0) {
            return -1;
        }
        ret = set_dlsymbol(p->mh_modulename, 
                           p->mh_ptr, &(*cfg)->cf_exec_eoh);
        if (ret != 0) {
            return -1;
        }
        ret = set_dlsymbol(p->mh_modulename, 
                           p->mh_ptr, &(*cfg)->cf_exec_eom);
        if (ret != 0) {
            return -1;
        }
        ret = set_dlsymbol(p->mh_modulename, 
                           p->mh_ptr, &(*cfg)->cf_exec_abort);
        if (ret != 0) {
            return -1;
        }
    }

    /* offsetから各モジュールのconfigポインタを格納する */
    for (excf = (*cfg)->cf_extraconfig; 
                 excf != NULL; excf = excf->excf_next) {
        excf->excf_config = (void *)((char *)*cfg + (size_t)excf->excf_config);
    }

    return 0;
}

/*
 * set_dlsymbol
 *
 * 機能
 *　　　modulelistに関数へのポインタをセット
 *
 * 引数
 *      char *mh_modulename     dlopenされているモジュールの名前
 *      void *dlptr             モジュールハンドル
 *      struct modulelist *list 各関数のモジュールリスト
 *
 * 返り値
 *             0        正常
 *             1        異常
 *
 */
int
set_dlsymbol(char *mh_modulename, void *dlptr, struct modulelist **list)
{
    struct modulelist *p;
    char  *error;
    int   (*func_pointer)(struct cfentry **, size_t *, struct config **, size_t *);

    for (p = *list; p != NULL; p = p->mlist_next) {
        if (strcmp(mh_modulename, p->mlist_modulename) == 0) {

            /* 関数ポインタの代入 */
            func_pointer = dlsym(dlptr, p->mlist_funcname);
            if ((error = dlerror()) != NULL) {
                SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, "set_dlsymbol",
                            p->mlist_funcname, error);
                return -1;
            }
            p->mlist_funcptr = (void (*)())func_pointer;
        }
    }

    return 0;
}

/*
 * set_config
 *
 * 機能
 *    configを読み込む処理(大元)
 *
 * 引数
 *     *file  設定ファイルのパス
 *    **cfg   読み込んだ設定ファイル情報の格納先
 *
 * 返り値
 *      R_SUCCESS       正常
 *      R_ERROR         エラー
 */
int
set_config(char *file, struct config **cfg)
{
    int ret;
    char *msg;
    size_t cfesize = sizeof(struct cfentry) * COUNT;
    size_t cfgsize = sizeof(struct config);
    struct cfentry *new_cfe = NULL;

    /* ログの初期化（標準エラー出力へ）*/
    dgloginit();

    /* 設定ファイルを格納する構造体の初期化 */
    *cfg = init_config();

    /* cfe構造体をヒープ領域へコピー */
    new_cfe = (struct cfentry *)malloc(cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "mlfi_connect", E_STR);
        return R_ERROR;
    }
    memcpy(new_cfe, &cfe, cfesize);

    /* 各モジュールのinit関数を実行 */
    ret = msy_module_init(&new_cfe, &cfesize, cfg, &cfgsize);
    if (ret != 0) {
        free(new_cfe);
        free(*cfg);
        *cfg = NULL;
        return R_ERROR;
    }
    
    /* コンフィグファイルの読み込み */ 
    ret = read_config(file, new_cfe, 
                      cfesize / sizeof(struct cfentry), *cfg);
    if (ret != 0) {
        if (errno == ENOMEM) {
            exit(EXIT_MANAGER);
        }
        SYSLOGWARNING(ERR_CONF_READ);
        free(new_cfe);
        free_config(*cfg);
        return R_ERROR;
    }

    free(new_cfe);

    /* コンフィグの値変更 */
    msy_module_modconfig(cfg);
        
    /* コンフィグファイルの必須値チェック */
    if ((*cfg)->cf_adminpassword == NULL) {
        SYSLOGWARNING(ERR_CONF_ADMINPASSWORD);
        free_config(*cfg);
        return R_ERROR;
    }
    if ((*cfg)->cf_mydomain == NULL) {
        SYSLOGWARNING(ERR_CONF_MYDOMAIN);
        free_config(*cfg);
        return R_ERROR;
    }
    if ((*cfg)->cf_savemailaddress == NULL) {
        SYSLOGWARNING(ERR_CONF_SAVEMAILADDRESS);
        free_config(*cfg);
        return R_ERROR;
    }
    if ((*cfg)->cf_listenport == 0) {
        SYSLOGWARNING(ERR_CONF_LISTENPORT);
        free_config(*cfg);
        return R_ERROR;
    }
    if ((*cfg)->cf_commandport == 0) {
        SYSLOGWARNING(ERR_CONF_COMMANDPORT);
        free_config(*cfg);
        return R_ERROR;
    }
    if ((*cfg)->cf_ldapport == 0) {
        SYSLOGWARNING(ERR_CONF_LDAPPORT);
        free_config(*cfg);
        return R_ERROR;
    }

    /* cf_msyhostnameの格納 */
    (*cfg)->cf_msyhostname = msy_hostname;

    /* コンフィグファイルの変換と格納 */
    ret = conv_config(*cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONVERT);
        free_config(*cfg);
        return R_ERROR;
    }

    /* シスログファシリティのチェック */
    msg = is_syslog_facility((*cfg)->cf_syslogfacility);
    if (msg != NULL) {
        SYSLOGWARNING("%s", msg);
        free_config(*cfg);
        return R_ERROR;
    }
    
    /* ログ出力先をコンフィグファイルに合わせて変更 */
    dglogchange(IDENT, (*cfg)->cf_syslogfacility);

    return R_SUCCESS;
}

/*
 * msy_module_modconfig
 *
 * 機能
 *　　　各モジュールのmodconfig関数を実行する
 *
 * 引数
 *      struct cfentry **cfe    config entry構造体(参照渡し)
 *      size_t cfesize          config entry構造体のサイズ(参照渡し)
 *      struct config  **cfg    config 構造体(参照渡し)
 *      size_t cfgsize          config 構造体のサイズ(参照渡し)
 *
 * 返り値
 *             0        正常
 *             1        異常
 *
 */
int
msy_module_modconfig(struct config **cfg)
{
    struct modulehandle *p;
    char   funcname[MAXFUNCNAME];
    int check;
    char  *error;
    int   (*func_pointer)(struct config **);

    for (p = mhandle_list; p != NULL; p = p->mh_next) {

        /* 関数名の生成 */
        sprintf(funcname, "%s_mod_extra_config", p->mh_modulename);

        /* 関数ポインタの代入 */
        func_pointer = dlsym(p->mh_ptr, funcname);
        if ((error = dlerror()) != NULL) {
            SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, 
                        "msy_module_modconfig", funcname, error);
            return -1;
        }

        check = (*func_pointer)(cfg);
        if (check != 0) {
            return -1;
        }
    }

    return 0;
}

/*
 * is_executable_file
 *
 * 機能
 *    ZipCommand チェック
 *
 * 引数
 *      char *str              コマンドパス
 *
 * 返り値
 *      NULL                   正常
 *      エラーメッセージ       異常
 */
char *
is_executable_file(char *str)
{
    struct stat st;
    char *space;
    char *cmd;

    /* 初期化*/
    space = NULL;
    cmd = NULL;

    /* コマンドをcmdにコピーする*/
    cmd = strdup(str);
    if (cmd == NULL) {
        return ERR_CONF_ALLOC;
    }

    /* コマンドファイル名を分析*/
    /* スペースを探すこと*/
    space = strchr(cmd, (int)' ');
    /* コマンドファイル名を区切り*/
    if (space != NULL) {
        *space = '\0';
    }

    /* ファイルコマンド存在チェック*/
    if (stat(cmd, &st) == -1) {
        free(cmd);
        return ERR_FILE_EXIST;
    }

    /* ファイル実行権限確認*/
    if (access(cmd, X_OK) != 0) {
        free(cmd);
        return ERR_FILE_EXECUTE_PERMITION;
    }

    /* 文字列の開放*/
    free(cmd);

    return (NULL);
}

/*
 * is_executable_file_size
 *
 * 機能
 *    実行ファイルとファイル名の長さチェック
 *
 * 引数
 *      char *str              コマンドパス
 *
 * 返り値
 *      NULL                   正常
 *      エラーメッセージ       異常
 */
char *
is_executable_file_size(char *str)
{
    struct stat st;
    char *space;
    char *cmd;

    /* コマンドの長さが1024より大きい場合 */
    if (strlen(str) > 1024) {
        return "File path is too long.";
    }

    /* 初期化*/
    space = NULL;
    cmd = NULL;

    /* コマンドをcmdにコピーする*/
    cmd = strdup(str);
    if (cmd == NULL) {
        return ERR_CONF_ALLOC;
    }

    /* コマンドファイル名を分析*/
    /* スペースを探すこと*/
    space = strchr(cmd, (int)' ');
    /* コマンドファイル名を区切り*/
    if (space != NULL) {
        *space = '\0';
    }

    /* ファイルコマンド存在チェック*/
    if (stat(cmd, &st) == -1) {
        free(cmd);
        return ERR_FILE_EXIST;
    }

    /* ファイル実行権限確認*/
    if (access(cmd, X_OK) != 0) {
        free(cmd);
        return ERR_FILE_EXECUTE_PERMITION;
    }

    /* 文字列の開放*/
    free(cmd);

    return (NULL);
}

/*
 * is_usable_password
 *
 * 機能
 *    渡したパスワードをチェックする
 *
 * 引数
 *    *password                チェックパスワード
 *
 * 返り値
 *      エラーメッセージ       異常
 *      NULL                   正常
 */
char *
is_usable_password(char *password) 
{
    char *is_null_msg;
    char string[] = CHAR_PASSWORD;
    int i,j;

    /* チェックパスワードNULL*/
    is_null_msg = is_not_null(password);
    if (is_null_msg != NULL) {
        return ERR_PASSWORD_NULL;
    }

    /* チェック許可文字*/
    for (i = 0; password[i] != '\0'; i++) {
        /* パスワードとして適切な文字が使われていることの確認 */
        for (j = 0; string[j] != '\0'; j++) {
            if (password[i] == string[j]) {
                break;
            }
        }
        /* 文字が合致することなく抜けた場合、エラー */
        if (string[j] == '\0') {
            return (ERR_INVALID_PASSWORD);
        }
    }
    /* 成功終了*/
    return NULL;
}

/*
 * cmd_strrep
 *
 * コマンド名、オプションを区切りして、リストに格納する
 *
 * 引数
 *      char *str              元の文字列
 *      char *sep              区切り文
 *      char **real            確保したコマンドの領域のポイントを保持
 *      int epoo               extend part of option
 *
 * 返り値
 *      cmd_list       正常
 *      NULL           エラー
 */
char **
cmd_strrep(char *str, char sep, char **real, int epoo)
{
    int len;
    int i;
    char *cmd;
    char **cmd_list;
    char *p;

    /* 初期化*/
    len = 1;
    i = 0;
    cmd = NULL;
    cmd_list = NULL;

    /* epoo の値エラー*/
    if (epoo < 1) {
        SYSLOGERROR(ERR_EXTEND_PART_OPTION_NUM, "cmd_strrep");
        return NULL;
    }

    /*利用しているコマンドの領域を確保する*/
    cmd = strdup(str);
    if (cmd == NULL) {
        SYSLOGERROR(ERR_MALLOC, "cmd_strrep", E_STR);        
        return NULL;
    }

    /* pポイントをcmdの先頭に遷移*/
    p = cmd;

    /* sepの数を計算する*/
    while (*p != '\0') {
        /*sepを探す*/
        if(*p == sep) {
            len++;
        }
        /* ループのポイント上がる*/
        p++;
    }

    /*文字列配列の確保*/
    cmd_list = (char **)malloc(sizeof(char *) * (len + epoo));
    if (cmd_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, cmd_strrep, E_STR);
        free(cmd);
        return NULL;
    }

    /* pポイントをcmdの先頭に示す*/
    p = cmd;
    cmd_list[i] = p;
    i++;

    /*繰り返しで文字列の分析*/
    while (*p != '\0') {
        /*sepを探す*/
        if(*p == sep) {
            *p = '\0';
            /*次の文字列に遷移*/
            p++;
            /*配列に格納する*/
            cmd_list[i] = p;
            i++;
            continue;
        }

        /* ループのポイント上がる*/
        p++;
    }

    /* 残った箇所にNULLを設定する*/
    for (i = 0; i < epoo; i++) {
        cmd_list[len + i] = NULL;
    }

    /* real command、argscommandを返す*/
    *real = cmd;
    return cmd_list;
}

