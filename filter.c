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
 * $RCSfile: filter.c,v $
 * $Revision: 1.51 $
 * $Date: 2009/10/29 10:56:27 $
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

/* �ץ�ȥ�������� */
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
 * �إå�������ɽ���ޥå�
 *
 * ����
 *      char *          �إå��ե������
 *      char *          �إå���
 *      regex_t *       ����ɽ��
 * �֤���
 *      R_POSITIVE      �ޥå�����
 *      R_SUCCESS       �ޥå����ʤ��ä�
 */
int
check_header_regex(char *headerf, char *headerv, regex_t *preg)
{
    struct strset ss;
    int ret;

    strset_init(&ss);

    /* �إå�ʸ�������� */
    if (strset_catstr(&ss, headerf) == -1 ||
        strset_catstr(&ss, ": ") == -1 ||
        strset_catstr(&ss, headerv) == -1) {
        SYSLOGERROR(ERR_LIBFUNC, "strset_catstr", E_STR);
        exit(EXIT_MILTER);
    }

    /* �ޥå� */
    ret = regexec(preg, ss.ss_str, 0, NULL, 0);
    strset_free(&ss);
    if (ret == 0) {
        /* �ޥå����� */
        return R_POSITIVE;
    }

    /* �ޥå����ʤ��ä� */
    return R_SUCCESS;
}

/*
 * check_domain
 *
 * ��ǽ
 *    domain������å��������
 *
 * ����
 *    char   *address       �����å�����᡼�륢�ɥ쥹
 *    struct config *cfg ����ե����ե�����
 *
 * �֤���
 *    R_SUCCESS    �ޥå�������
 *    R_ERROR      �ޥå����ʤ��ä���
 */
int
check_domain(char *address, struct config *cfg)
{
    struct strlist *check;
    char *ptr;

    /* ���ɥ쥹����@��õ�� */
    ptr = strchr(address, '@');
    
    check = cfg->cf_mydomain_list;
    /* �ꥹ�ȤΥɥᥤ��ȥ��ɥ쥹�Υɥᥤ��������� */
    while (check != NULL) {
        if (strcasecmp(check->ss_data.ss_str, ptr + 1) == 0) {
            return R_SUCCESS;
       }
        check = check->next;
    }
    /* ������ä�����ʤ���硢��¸���ʤ���*/
    return R_ERROR;
}

/*
 * check_mailaddress
 *
 * ��ǽ
 *    mailaddress������å��������
 *
 * ����
 *    char   *address       �����å�����᡼�륢�ɥ쥹
 *    struct config *cfg ����ե����ե�����
 *
 * �֤���
 *    R_SUCCESS    �ޥå�������
 *    R_ERROR      �ޥå����ʤ��ä���
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

    /* �����å����� */
    while (list != NULL) {
        /* �ꥹ�ȤΥ��ɥ쥹�����⤫����Ƥ��ʤ����̵�뤷�ƿʤ��*/
        if (list->ss_data.ss_str[0] == '\0') {
            list = list->next;
            continue;
        }
        /* �ꥹ�ȤΥ��ɥ쥹��@�����뤫Ƚ�� */
        ret = strchr(list->ss_data.ss_str, '@');   
        /* @��̵����С��ɥᥤ��θ������פ��ǧ */
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
            /* @�����ꡢ���ĺǽ��@��������ɥᥤ��δ������פγ�ǧ */
            if (list->ss_data.ss_str[0] == '@') {
                ptr = strchr(address, '@');
                if (strcasecmp(list->ss_data.ss_str, ptr) == 0) {
                    return R_SUCCESS;
                }
            /* @�����ꡢ@���ǽ��̵����硢���ɥ쥹�δ������פγ�ǧ */
            } else {
                if (strcasecmp(list->ss_data.ss_str, address) == 0) {
                    return R_SUCCESS;
                }
            }
        }
        list = list->next; 
    }
    /* �������ʤ���硢��¸���ʤ���*/
    return R_ERROR;
}

/*
 * judge_mail 
 *
 * ��ǽ
 *    �᡼�����¸���뤫Ƚ�Ǥ���ؿ�(ldap�ʳ�)
 *
 * ����
 *    struct strlist *checklist    �����å����륢�ɥ쥹�Υꥹ��
 *    struct strlist **savelist_h  ��¸���ɥ쥹�ꥹ�Ȥ���Ƭ
 *    struct strlist **savelist_t  ��¸���ɥ쥹�ꥹ�Ȥ�����
 *    struct strlist **ldaplist_h  ldap�����å��ꥹ�Ȥ���Ƭ
 *    struct strlist **ldaplist_t  ldap�����å��ꥹ�Ȥ�����
 *    struct config  *cfg          ����ե����ե�����
 *    unsigned int s_id            �᡼��ID 
 *
 * �֤���
 *    R_SUCCESS    ����
 *    R_ERROR      ���� 
 */
int
judge_mail(struct strset *checkaddress, struct strlist **savelist_h,
           struct strlist **savelist_t, struct strlist **ldaplist_h,
           struct strlist **ldaplist_t,  struct config *cfg,
           unsigned int s_id)
{
    int ret;     

    /* �ɥᥤ��Υ����å� */
    ret = check_domain(checkaddress->ss_str, cfg);
    if (ret != R_SUCCESS) {
        SYSLOGINFO(JUDGE_DOMAIN_OUT, s_id, checkaddress->ss_str);
        return R_SUCCESS;
    }
    /* ���ɥ쥹�Υ����å� */
    ret = check_mailaddress(checkaddress->ss_str, cfg);
    if (ret != R_SUCCESS) {
       /* ldap�Υ����å�(���ɥ쥹�Υ����å��� 
        * �������ʤ��ä���礳�������롣��*/
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
 * ��ǽ
 *    "@"��̵�����ɥ쥹���䴰�������
 *
 * ����
 *    struct strset *address       �����å�����᡼�륢�ɥ쥹
 *    struct strset *comp_address  �䴰�������ɥ쥹�γ�Ǽ�� 
 *    struct config *cfg           ����ե����ե�����
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

    /* ���ɥ쥹����@��õ�� */
    ptr = strchr(comp_address->ss_str, '@');
    /* @ ��̵����С�����ե�����DefaultDomain��­�� */
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
 * ��ǽ
 *     ldap�����򤫤���ؿ�
 *
 * ����
 *    struct strlist *ldaplist    �����򤫤���ꥹ��
 *    struct strlist **savelist_h ��¸�ꥹ�Ȥ���Ƭ
 *    struct strlist **savelist_t ��¸�ꥹ�Ȥ�����
 *    struct config  *cfg         ����ե����ե�����
 *    unsignet int s_id           �᡼��ID 
 *
 * �֤���
 *     R_SUCCESS    ����
 *     R_ERROR      ���� 
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

    /* ldap�򳫻Ϥ��� */
    ld = ldap_open(cfg->cf_ldapserver, cfg->cf_ldapport);
    if (ld == NULL) {
        SYSLOGERROR(ERR_LDAP_OPEN);
        return R_ERROR;
    }
    /* ldap�˥����ॢ���Ȥ����ꤹ�� */
    ret = ldap_set_option(ld, LDAP_OPT_TIMEOUT, &cfg->cf_ldaptimeout);
    if (ret != 0) {
        SYSLOGERROR(ERR_LDAP_SET, ldap_err2string(ret));
        ldap_unbind(ld);
        return R_ERROR;
    }
    /* ldap�˥Х���ɤ��� */
    ret = ldap_simple_bind_s(ld, cfg->cf_ldapbinddn,
                             cfg->cf_ldapbindpassword);
    if (ret != LDAP_SUCCESS) {
        SYSLOGERROR(ERR_LDAP_BIND, ldap_err2string(ret));
        ldap_unbind(ld);
        return R_ERROR;
    }
    /* ldap�˥�������ݤ��� */
    list = ldaplist;
    while (list != NULL) {
        /* %M���Ѵ�������� */
        sf[0].sf_formatchar = 'M';
        sf[0].sf_replacestr = list->ss_data.ss_str;
        filter = str_replace_format(cfg->cf_ldapmailfilter, sf , 1);
        /* ���� */
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
 * ��ǽ
 *    ��¸����᡼��Υꥹ�Ȥ��������
 *
 * ����
 *    char   *from       �����å�����from�᡼�륢�ɥ쥹
 *    struct strlist *rcptlist
 *    struct strlist **mlfi_addrmatched_h
 *    struct strlist **mlfi_addrmatched_t
 *    struct config *cfg ����ե����ե�����
 *    unsigned int s_id �᡼��ID
 *
 * �֤���
 *    R_SUCCESS             ���� 
 *    R_ERROR               ����
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

    /* �ݥꥷ��Ƚ��  */
    if (FROM & cfg->cf_savepolicy_conv) {
        /* ���ɥ쥹�䴰���� */
        complement_address(from, &checkaddress, cfg);
        /* ��¸�ꥹ�Ȥ�ldap�ꥹ�Ȥؤλ�ʬ������ */
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
            /* ���ɥ쥹�䴰���� */
            complement_address(&rcptlist->ss_data, &checkaddress, cfg);
            /* ��¸�ꥹ�Ȥ�ldap�ꥹ�Ȥؤλ�ʬ������ */
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


    /* ldapȽ����� */
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
