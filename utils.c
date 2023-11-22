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
 * ��ǽ
 *      strlist���ͤ�����
 *      ��Ǽ����ʸ����ˤĤ��Ƥϡ��ΰ�� alloc ���ޤ�
 *
 * ����
 *      **listhead  strlist����Ƭ�ݥ���
 *      **listtail  strlist�������ݥ���
 *      *str        ��Ǽ����ʸ����
 *
 * �֤���
 *      R_SUCCESS   ����
 */
int
push_strlist(struct strlist **listhead, struct strlist **listtail, char *str)
{
    struct strlist *inslist;
    char *string;
    char f_name[] = "push_strlist";

    /* strlist ��¤�Τ����� */
    inslist = make_strlist();

    string = strdup(str);
    if (string == NULL) {
        SYSLOGERROR(ERR_MALLOC, f_name, E_STR);
        exit(EXIT_UTILS);
    }

    /* ʸ�����¤�Τس�Ǽ */
    strset_set(&(inslist->ss_data), string);

    /* �ꥹ�Ȥκ��� */
    if (*listhead == NULL) {
        /* ��Ƭ���ɲ� */
        *listhead = inslist;
        *listtail = inslist;
    } else {
        /* ���������� */
        (*listtail)->next = inslist;
        *listtail = inslist;
    }

    return (R_SUCCESS);
}

/*
 * uniq_push_strlist
 *
 * ��ǽ
 *      strlist���ͤ���������ʽ�ʣ�������ϥꥹ�Ȥ��ɲä��ޤ����
 *      ��Ǽ����ʸ����ˤĤ��Ƥϡ��ΰ�� alloc ���ޤ�
 *
 * ����
 *      **listhead  strlist����Ƭ�ݥ���
 *      **listtail  strlist����Ƭ�ݥ���
 *      *str        ����ʸ����
 *
 * �֤���
 *      R_SUCCESS   ����
 */
int
uniq_push_strlist(struct strlist **listhead, struct strlist **listtail, char *str)
{
    int ret;

    /* ��ʣ�θ��� */
    ret = search_strlist(*listhead, str);
    if (ret == R_FOUND) {
        return (R_SUCCESS);
    }

    /* ��ʣ���ʤ���Хꥹ�Ȥ��ɲ� */
    push_strlist(listhead, listtail, str);

    return (R_SUCCESS);
}

/*
 * search_strlist
 *
 * ��ǽ
 *      strlist����ʸ����򸡺�����ʴ������ס�
 *
 * ����
 *      *strlist   strlist����Ƭ�ݥ���
 *      *str       ����ʸ����
 *
 * �֤���
 *      R_FOUND    ���Ĥ��ä�
 *      R_NOTFOUND ���Ĥ���ʤ��ä�
 */
int
search_strlist(struct strlist *strlist, char *str)
{
    struct strlist *tmplist;
    int ret;

    /* ���� */
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
 * ��ǽ
 *      strlist�β���
 *
 * ����
 *      *strlist  strlist����Ƭ�ݥ���
 *
 * �֤���
 *      �ʤ�
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
 * ��ǽ
 *      ����strlist������
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      strlist:  ����
 */
struct strlist *
make_strlist(void)
{
    struct strlist *list;
    char f_name[] = "make_strlist";

    /* strlist������ */
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
 * ��ǽ
 *      ʸ����򥳥�ޤ��ڤ�ʬ�� strlist �ˤ���
 *
 * ����
 *      str: ʸ����
 *
 * �֤���
 *      list:  ����
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
 * ��ǽ
 *      ���ꥢ���ֹ��ȯ�Ԥ���
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      �ʤ�
 */
unsigned int
get_serialno()
{
    unsigned int retserial;

    pthread_mutex_lock(&serial_lock);
    serialno++;

    /* ���16bit���˴� */
    serialno &= 0x0000ffff;
    retserial = serialno;
    pthread_mutex_unlock(&serial_lock);

    return (retserial);
}

/*
 * get_sessid
 *
 * ��ǽ
 *      ���å����ID����������
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      �ʤ�
 */
unsigned int
get_sessid()
{
    unsigned int serial;
    unsigned int pid;
    unsigned int sessid;

    serial = get_serialno();
    pid = getpid();

    /* ���16bit PID ����16bit SerialNo */
    pid = pid << 16;
    sessid = serial + pid;

    return (sessid);
}

/*
 * replace_delimiter
 *
 * ��ǽ
 *      ʸ������˴ޤޤ�������ʸ���򡢻��ꤵ�줿ʸ�����֤�������
 * ����
 *      char *          ʸ����
 *      char            �֤������оݤ�ʸ��
 *      char            �֤����������ʸ��
 * �֤���
 *              �ʤ�
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
 * ʸ������Υե����ޥå�ʸ�� (%X) ���֤�������
 * �������ΰ�˳�Ǽ����
 *
 * ����
 *      char *                  ʸ����
 *      struct strformat *      �֤���������
 *      int                     �֤����������������Ĺ��
 * �֤���
 *      char *                  �Ѵ����ʸ����
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

        /* "%\0" �ξ�� */
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
 * ��ǽ
 *    strlist��strlist�ˤ��äĤ������
 *
 * ����
 *     struct strlist **listhead
 *     struct strlist **listtail
 *     struct strlist *attachlist
 *
 * �֤���
 *     R_ERROR    ����
 *     R_SUCCESS  ����
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
