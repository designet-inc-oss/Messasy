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
 * $RCSfile: utils.c,v $
 * $Revision: 1.19 $
 * $Date: 2009/10/27 08:31:08 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libdgstr.h>
#include <libdgconfig.h>
#include <pthread.h>
#include <libmilter/mfapi.h>
#include <sys/types.h>
#include <unistd.h>

#include "messasy.h"
#include "utils.h"
#include "log.h"

static unsigned int serialno;
static pthread_mutex_t serial_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * push_strlist
 *
 * 機能
 *      strlistに値を挿入
 *      格納する文字列については、領域を alloc します
 *
 * 引数
 *      **listhead  strlistの先頭ポインタ
 *      **listtail  strlistの末尾ポインタ
 *      *str        格納する文字列
 *
 * 返り値
 *      R_SUCCESS   正常
 */
int
push_strlist(struct strlist **listhead, struct strlist **listtail, char *str)
{
    struct strlist *inslist;
    char *string;
    char f_name[] = "push_strlist";

    /* strlist 構造体の生成 */
    inslist = make_strlist();

    string = strdup(str);
    if (string == NULL) {
        SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
        exit(EXIT_UTILS);
    }

    /* 文字列を構造体へ格納 */
    strset_set(&(inslist->ss_data), string);

    /* リストの作成 */
    if (*listhead == NULL) {
        /* 先頭に追加 */
        *listhead = inslist;
        *listtail = inslist;
    } else {
        /* 末尾に挿入 */
        (*listtail)->next = inslist;
        *listtail = inslist;
    }

    return (R_SUCCESS);
}

/*
 * uniq_push_strlist
 *
 * 機能
 *      strlistに値を挿入する（重複した場合はリストに追加しません）
 *      格納する文字列については、領域を alloc します
 *
 * 引数
 *      **listhead  strlistの先頭ポインタ
 *      **listtail  strlistの先頭ポインタ
 *      *str        検索文字列
 *
 * 返り値
 *      R_SUCCESS   正常
 */
int
uniq_push_strlist(struct strlist **listhead, struct strlist **listtail, char *str)
{
    int ret;

    /* 重複の検索 */
    ret = search_strlist(*listhead, str);
    if (ret == R_FOUND) {
        return (R_SUCCESS);
    }

    /* 重複がなければリストに追加 */
    push_strlist(listhead, listtail, str);

    return (R_SUCCESS);
}

/*
 * search_strlist
 *
 * 機能
 *      strlistから文字列を検索する（完全一致）
 *
 * 引数
 *      *strlist   strlistの先頭ポインタ
 *      *str       検索文字列
 *
 * 返り値
 *      R_FOUND    見つかった
 *      R_NOTFOUND 見つからなかった
 */
int
search_strlist(struct strlist *strlist, char *str)
{
    struct strlist *tmplist;
    int ret;

    /* 検索 */
    for (tmplist = strlist; tmplist != NULL; tmplist = tmplist->next) {
        ret = strcmp(str, tmplist->ss_data.ss_str);
        if (ret == 0) {
            return (R_FOUND);
        }
    }

    return (R_NOTFOUND);
}

/*
 * free_strlist
 *
 * 機能
 *      strlistの解放
 *
 * 引数
 *      *strlist  strlistの先頭ポインタ
 *
 * 返り値
 *      なし
 */
void
free_strlist(struct strlist *strlist)
{
    struct strlist *tmplist;
    struct strlist *next;

    tmplist = strlist;
    while (tmplist != NULL) {
        next = tmplist->next;
        strset_free(&(tmplist->ss_data));
        free(tmplist);
        tmplist = next;
    }
}

/*
 * make_strlist
 *
 * 機能
 *      空のstrlistを生成
 *
 * 引数
 *      なし
 *
 * 返り値
 *      strlist:  正常
 */
struct strlist *
make_strlist(void)
{
    struct strlist *list;
    char f_name[] = "make_strlist";

    /* strlistの生成 */
    list = malloc(sizeof(struct strlist));
    if (list == NULL) {
        SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
        exit(EXIT_UTILS);
    }

    list->next = NULL;

    return list;
}

/*
 * split_comma
 *
 * 機能
 *      文字列をコンマで切り分け strlist にする
 *
 * 引数
 *      str: 文字列
 *
 * 返り値
 *      list:  正常
 */
struct strlist *
split_comma (char *str)
{
    char *head;
    char *end = NULL;
    struct strlist *listhead = NULL; 
    struct strlist *listtail = NULL; 

    for (head = str, end = strchr(head, ',');
        end != NULL;
        head = end + 1, end = strchr(head, ',')) {

        *end = '\0';
        uniq_push_strlist(&listhead, &listtail, head);
        *end = ',';
    }
    uniq_push_strlist(&listhead, &listtail, head);

    return listhead;
}

/*
 * get_serialno
 *
 * 機能
 *      シリアル番号を発行する
 *
 * 引数
 *      なし
 *
 * 返り値
 *      なし
 */
unsigned int
get_serialno()
{
    unsigned int retserial;

    pthread_mutex_lock(&serial_lock);
    serialno++;

    /* 上位16bitを破棄 */
    serialno &= 0x0000ffff;
    retserial = serialno;
    pthread_mutex_unlock(&serial_lock);

    return (retserial);
}

/*
 * get_sessid
 *
 * 機能
 *      セッションIDを生成する
 *
 * 引数
 *      なし
 *
 * 返り値
 *      なし
 */
unsigned int
get_sessid()
{
    unsigned int serial;
    unsigned int pid;
    unsigned int sessid;

    serial = get_serialno();
    pid = getpid();

    /* 上位16bit PID 下位16bit SerialNo */
    pid = pid << 16;
    sessid = serial + pid;

    return (sessid);
}

/*
 * replace_delimiter
 *
 * 機能
 *      文字列中に含まれる特定の文字を、指定された文字に置き換える
 * 引数
 *      char *          文字列
 *      char            置き換え対象の文字
 *      char            置き換えた後の文字
 * 返り値
 *              なし
 */
void
replace_delimiter(char *str, char old_delim, char new_delim)
{
    char *p = str;
    while ((p = strchr(p, old_delim)) != NULL) {
        *p++ = new_delim;
    }
    return;
}

/*
 * str_replace_format
 *
 * 文字列中のフォーマット文字 (%X) を置き換え、
 * 新たな領域に格納する
 *
 * 引数
 *      char *                  文字列
 *      struct strformat *      置き換え一覧
 *      int                     置き換え一覧の配列の長さ
 * 返り値
 *      char *                  変換後の文字列
 */
char *
str_replace_format(char *str, struct strformat *sf, int sfcount)
{
    struct strset ss;
    char *leader, *chaser;
    int ret, i;

    strset_init(&ss);

    chaser = str;
    while ((leader = strchr(chaser, '%')) != NULL) {
        ret = strset_catnstr(&ss, chaser, leader - chaser);
        if (ret != R_SUCCESS) {
            SYSLOGERROR(ERR_LIBFUNC, "strset_catnstr", E_STR);
            exit(EXIT_MILTER);
        }

        /* "%\0" の場合 */
        if (*(leader + 1) == '\0') {
            chaser = leader;

            break;
        }

        if (*(leader + 1) == '%') {
            /* "%%" -> "%" */
            ret = strset_catstr(&ss, "%");
            if (ret != R_SUCCESS) {
                SYSLOGERROR(ERR_LIBFUNC, "strset_catstr", E_STR);
                exit(EXIT_MILTER);
            }
        } else {
            for (i = 0; i < sfcount; i++) {
                /* search format char */
                if (sf[i].sf_formatchar != *(leader + 1)) {
                    continue;
                }
                ret = strset_catstr(&ss, sf[i].sf_replacestr);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_LIBFUNC, "strset_catstr", E_STR);
                    exit(EXIT_MILTER);
                }
                break;
            }
            if (i == sfcount) {
                /* not found */
                ret = strset_catnstr(&ss, leader, 2);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_LIBFUNC, "strset_catnstr", E_STR);
                    exit(EXIT_MILTER);
                }
            }
        }
        chaser = leader + 2;
    }

    /* copy the rest */
    ret = strset_catstr(&ss, chaser);
    if (ret != R_SUCCESS) {
        SYSLOGERROR(ERR_LIBFUNC, "strset_catstr", E_STR);
        exit(EXIT_MILTER);
    }

    return ss.ss_str;
}

/*
 * attch_strlist
 *
 * 機能
 *    strlistをstrlistにくっつける処理
 *
 * 引数
 *     struct strlist **listhead
 *     struct strlist **listtail
 *     struct strlist *attachlist
 *
 * 返り値
 *     R_ERROR    失敗
 *     R_SUCCESS  成功
 */
int
attach_strlist(struct strlist **listhead, struct strlist **listtail,
               struct strlist *attachlist)
{
    attachlist->next = NULL;

    if (*listhead == NULL) {
        *listhead = attachlist;
        *listtail = attachlist;
    } else {
        (*listtail)->next = attachlist;
        *listtail = attachlist;
    }
    return (R_SUCCESS);
}
