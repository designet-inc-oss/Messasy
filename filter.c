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
#include <libdgstr.h>
#include <libdgconfig.h>
#include <libdgmail.h>
#include <libmilter/mfapi.h>
#include <regex.h>
#include <sys/types.h>
#include <errno.h>

#define LDAP_DEPRECATED 1

#include <ldap.h>

#include "messasy.h"
#include "msy_config.h"
#include "filter.h"
#include "utils.h"
#include "log.h"

/* プロトタイプ宣言 */
static int check_domain(char *address, struct config *cfg);
static int check_mailaddress(char *address, struct config *cfg);
static int judge_mail(struct strset *checkaddress, struct strlist **savelist_h,
                      struct strlist **savelist_t, struct strlist **ldaplist_h,
                      struct strlist **ldaplist_t, struct config *cfg,
                      unsigned int s_id);
static void complement_address(struct strset *, struct strset *, struct config *);
static int check_ldap(struct strlist *ldaplist, struct strlist **savelist_h,
                      struct strlist **savelist_t, struct config *cfg,
                      unsigned int s_id);
/*
 * check_header_regex
 *
 * ヘッダの正規表現マッチ
 *
 * 引数
 *      char *          ヘッダフィールド
 *      char *          ヘッダ値
 *      regex_t *       正規表現
 * 返り値
 *      R_POSITIVE      マッチした
 *      R_SUCCESS       マッチしなかった
 */
int
check_header_regex(char *headerf, char *headerv, regex_t *preg)
{
    struct strset ss;
    int ret;

    strset_init(&ss);

    /* ヘッダ文字列を作成 */
    if (strset_catstr(&ss, headerf) == -1 ||
        strset_catstr(&ss, ": ") == -1 ||
        strset_catstr(&ss, headerv) == -1) {
        SYSLOGERROR(ERR_LIBFUNC, "strset_catstr", E_STR);
        exit(EXIT_MILTER);
    }

    /* マッチ */
    ret = regexec(preg, ss.ss_str, 0, NULL, 0);
    strset_free(&ss);
    if (ret == 0) {
        /* マッチした */
        return R_POSITIVE;
    }

    /* マッチしなかった */
    return R_SUCCESS;
}

/*
 * check_domain
 *
 * 機能
 *    domainをチェックする処理
 *
 * 引数
 *    char   *address       チェックするメールアドレス
 *    struct config *cfg コンフィグファイル
 *
 * 返り値
 *    R_SUCCESS    マッチした時
 *    R_ERROR      マッチしなかった時
 */
int
check_domain(char *address, struct config *cfg)
{
    struct strlist *check;
    char *ptr;

    /* アドレスから@を探す */
    ptr = strchr(address, '@');
    
    check = cfg->cf_mydomain_list;
    /* リストのドメインとアドレスのドメイン部の比較 */
    while (check != NULL) {
        if (strcasecmp(check->ss_data.ss_str, ptr + 1) == 0) {
            return R_SUCCESS;
       }
        check = check->next;
    }
    /* 一件も引っかからない場合、保存しない。*/
    return R_ERROR;
}

/*
 * check_mailaddress
 *
 * 機能
 *    mailaddressをチェックする処理
 *
 * 引数
 *    char   *address       チェックするメールアドレス
 *    struct config *cfg コンフィグファイル
 *
 * 返り値
 *    R_SUCCESS    マッチした時
 *    R_ERROR      マッチしなかった時
 *
 */
int
check_mailaddress(char *address, struct config *cfg)
{
    struct strlist *list;
    char *ret, *ptr;
    int address_len, list_len;

    address_len = strlen(address);

    list = cfg->cf_savemailaddress_list;

    /* チェック処理 */
    while (list != NULL) {
        /* リストのアドレスが何もかかれていない場合〔技襪靴匿覆瓩襤*/
        if (list->ss_data.ss_str[0] == '\0') {
            list = list->next;
            continue;
        }
        /* リストのアドレスに@があるか判別 */
        ret = strchr(list->ss_data.ss_str, '@');   
        /* @が無ければ、ドメインの後方一致を確認 */
        if (ret == NULL ) {
            list_len = strlen(list->ss_data.ss_str);
            if (address_len < list_len) {
                list = list->next;
                continue;
            }  
            if (!strcasecmp(list->ss_data.ss_str, 
                            address + address_len - list_len)) {
                return R_SUCCESS;
            }
        } else {
            /* @があり、かつ最初に@がある場合ドメインの完全一致の確認 */
            if (list->ss_data.ss_str[0] == '@') {
                ptr = strchr(address, '@');
                if (strcasecmp(list->ss_data.ss_str, ptr) == 0) {
                    return R_SUCCESS;
                }
            /* @があり、@が最初に無い場合、アドレスの完全一致の確認 */
            } else {
                if (strcasecmp(list->ss_data.ss_str, address) == 0) {
                    return R_SUCCESS;
                }
            }
        }
        list = list->next; 
    }
    /* 該当しない場合、保存しない。*/
    return R_ERROR;
}

/*
 * judge_mail 
 *
 * 機能
 *    メールを保存するか判断する関数(ldap以外)
 *
 * 引数
 *    struct strlist *checklist    チェックするアドレスのリスト
 *    struct strlist **savelist_h  保存アドレスリストの先頭
 *    struct strlist **savelist_t  保存アドレスリストの末尾
 *    struct strlist **ldaplist_h  ldapチェックリストの先頭
 *    struct strlist **ldaplist_t  ldapチェックリストの末尾
 *    struct config  *cfg          コンフィグファイル
 *    unsigned int s_id            メールID 
 *
 * 返り値
 *    R_SUCCESS    成功
 *    R_ERROR      失敗 
 */
int
judge_mail(struct strset *checkaddress, struct strlist **savelist_h,
           struct strlist **savelist_t, struct strlist **ldaplist_h,
           struct strlist **ldaplist_t,  struct config *cfg,
           unsigned int s_id)
{
    int ret;     

    /* ドメインのチェック */
    ret = check_domain(checkaddress->ss_str, cfg);
    if (ret != R_SUCCESS) {
        SYSLOGINFO(JUDGE_DOMAIN_OUT, s_id, checkaddress->ss_str);
        return R_SUCCESS;
    }
    /* アドレスのチェック */
    ret = check_mailaddress(checkaddress->ss_str, cfg);
    if (ret != R_SUCCESS) {
       /* ldapのチェック(アドレスのチェックで 
        * 該当しなかった場合こちらに来る。）*/
        if (cfg->cf_ldapcheck == 1) {
            push_strlist(ldaplist_h, ldaplist_t,
                               checkaddress->ss_str);
        } else {
            SYSLOGINFO(JUDGE_ADDRESS_OUT, s_id, checkaddress->ss_str);
        }
        return R_SUCCESS;
    }
    push_strlist(savelist_h, savelist_t,
                      checkaddress->ss_str);
    SYSLOGINFO(JUDGE_CLEARED, s_id, checkaddress->ss_str);
 
    return R_SUCCESS;
}

/*
 * complement_address
 *
 * 機能
 *    "@"の無いアドレスを補完する処理
 *
 * 引数
 *    struct strset *address       チェックするメールアドレス
 *    struct strset *comp_address  補完したアドレスの格納先 
 *    struct config *cfg           コンフィグファイル
 *
 */
void
complement_address(struct strset *address, struct strset *comp_address, struct config *cfg)
{
    char *ptr;
    char *str;
    int   ret;

    str = strdup(address->ss_str);
    if (address->ss_str == NULL) {
        SYSLOGERROR(ERR_JUDGE_COMPLEMENT);
        exit (EXIT_MILTER);
    }    
    strset_set(comp_address, str);

    /* アドレスから@を探す */
    ptr = strchr(comp_address->ss_str, '@');
    /* @ が無ければ、コンフィグのDefaultDomainを足す */
    if (ptr == NULL) {
        ret = strset_catstr(comp_address, "@");
        if (ret != R_SUCCESS) {
            SYSLOGERROR(ERR_JUDGE_COMPLEMENT);
            exit (EXIT_MILTER);
        }
        ret = strset_catstr(comp_address,
                            cfg->cf_defaultdomain);
        if (ret != R_SUCCESS) {
            SYSLOGERROR(ERR_JUDGE_COMPLEMENT);
            exit (EXIT_MILTER);
        }
    }
    return;
}

/*
 * check_ldap
 *
 * 機能
 *     ldap検索をかける関数
 *
 * 引数
 *    struct strlist *ldaplist    検索をかけるリスト
 *    struct strlist **savelist_h 保存リストの先頭
 *    struct strlist **savelist_t 保存リストの末尾
 *    struct config  *cfg         コンフィグファイル
 *    unsignet int s_id           メールID 
 *
 * 返り値
 *     R_SUCCESS    成功
 *     R_ERROR      失敗 
 */
int
check_ldap(struct strlist *ldaplist, struct strlist **savelist_h,
           struct strlist **savelist_t, struct config *cfg, unsigned int s_id)
{
    LDAP *ld;
    LDAPMessage *res;
    int ret, count;
    struct strformat sf[1];
    char   *filter;
    struct strlist *list;
    struct strlist *now;

    /* ldapを開始する */
    ld = ldap_open(cfg->cf_ldapserver, cfg->cf_ldapport);
    if (ld == NULL) {
        SYSLOGERROR(ERR_LDAP_OPEN);
        return R_ERROR;
    }
    /* ldapにタイムアウトを設定する */
    ret = ldap_set_option(ld, LDAP_OPT_TIMEOUT, &cfg->cf_ldaptimeout);
    if (ret != 0) {
        SYSLOGERROR(ERR_LDAP_SET, ldap_err2string(ret));
        ldap_unbind(ld);
        return R_ERROR;
    }
    /* ldapにバインドする */
    ret = ldap_simple_bind_s(ld, cfg->cf_ldapbinddn,
                             cfg->cf_ldapbindpassword);
    if (ret != LDAP_SUCCESS) {
        SYSLOGERROR(ERR_LDAP_BIND, ldap_err2string(ret));
        ldap_unbind(ld);
        return R_ERROR;
    }
    /* ldapにサーチを掛ける */
    list = ldaplist;
    while (list != NULL) {
        /* %Mを変換する処理 */
        sf[0].sf_formatchar = 'M';
        sf[0].sf_replacestr = list->ss_data.ss_str;
        filter = str_replace_format(cfg->cf_ldapmailfilter, sf , 1);
        /* 検索 */
        ret = ldap_search_s(ld, cfg->cf_ldapbasedn, cfg->cf_ldapscope_conv,
                            filter, NULL, 0, &res);
        if (ret != LDAP_SUCCESS) {
            SYSLOGERROR(ERR_LDAP_SEARCH, ldap_err2string(ret));
            ldap_unbind(ld);
            free(filter);
            return R_ERROR;
        }
        now = list;
        list = list->next;
        count = ldap_count_entries(ld, res);
        if (count != 0) {
            attach_strlist(savelist_h, savelist_t, now);
            SYSLOGINFO(JUDGE_CLEARED, s_id, now->ss_data.ss_str);
        } else {
            SYSLOGINFO(JUDGE_LDAP_OUT, s_id, now->ss_data.ss_str);
            free(now->ss_data.ss_str);
            free(now);
        }
        ldap_msgfree(res);
        free(filter);
    }
    ldap_unbind(ld);

    return R_SUCCESS;
}

/*
 * make_savelist
 *
 * 機能
 *    保存するメールのリストを作成する
 *
 * 引数
 *    char   *from       チェックするfromメールアドレス
 *    struct strlist *rcptlist
 *    struct strlist **mlfi_addrmatched_h
 *    struct strlist **mlfi_addrmatched_t
 *    struct config *cfg コンフィグファイル
 *    unsigned int s_id メールID
 *
 * 返り値
 *    R_SUCCESS             成功 
 *    R_ERROR               失敗
 */
int
make_savelist(struct strset *from, struct strlist *rcptlist,
              struct strlist **savelist_h, struct strlist **savelist_t,
              struct config *cfg, unsigned int s_id)
{
    struct strlist *ldaplist_h, *ldaplist_t;
    struct strset checkaddress;
    int    ret;

    ldaplist_h = ldaplist_t = NULL;

    /* ポリシー判定  */
    if (FROM & cfg->cf_savepolicy_conv) {
        /* アドレス補完処理 */
        complement_address(from, &checkaddress, cfg);
        /* 保存リストとldapリストへの仕分け処理 */
        ret = judge_mail(&checkaddress, savelist_h, savelist_t,
                         &ldaplist_h, &ldaplist_t, cfg, s_id);
        strset_free(&checkaddress);
        if (ret != R_SUCCESS) {
            free_strlist(ldaplist_h);
            free_strlist(*savelist_h);
            *savelist_h = *savelist_t = NULL;
            SYSLOGERROR(ERR_JUDGE_MAIL);
            return R_ERROR;
        }
    }
    if (TO & cfg->cf_savepolicy_conv) {
        while (rcptlist != NULL) {
            /* アドレス補完処理 */
            complement_address(&rcptlist->ss_data, &checkaddress, cfg);
            /* 保存リストとldapリストへの仕分け処理 */
            ret = judge_mail(&checkaddress, savelist_h, savelist_t,
                             &ldaplist_h, &ldaplist_t, cfg, s_id);
            strset_free(&checkaddress);
            if (ret != R_SUCCESS) {
                free_strlist(ldaplist_h);
                free_strlist(*savelist_h);
                *savelist_h = *savelist_t = NULL;
                SYSLOGERROR(ERR_JUDGE_MAIL);
                return R_ERROR;
            }
            rcptlist = rcptlist->next;
        }
    }


    /* ldap判定処理 */
    if (ldaplist_h != NULL) {
        ret = check_ldap(ldaplist_h, savelist_h, savelist_t, cfg, s_id);
        if (ret != R_SUCCESS) {
            free_strlist(ldaplist_h);
            free_strlist(*savelist_h);
            *savelist_h = *savelist_t = NULL;
            SYSLOGERROR(ERR_CHECK_LDAP);
            return R_ERROR;
        }
    }
    return R_SUCCESS;
}
