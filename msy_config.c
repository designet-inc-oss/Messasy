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
 * $RCSfile: msy_config.c,v $
 * $Revision: 1.47 $
 * $Date: 2009/11/11 04:41:58 $
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

#define SYSLOG_NAMES
#include <libdgconfig.h>

#include <ldap.h>

/* add included header for make */
//#include "config.h"

#include "messasy.h"
#include "msy_config.h"
#include "utils.h"
#include "log.h"
#include "msy_readmodule.h"

/* �ץ�ȥ�������� */
static char * is_timeout(int value);
static char * is_commandmaxclients(int value);
static char * is_erroraction(char *str);
static char * is_savepolicy(char *str);

#ifdef OLD_CODE
static char * is_mailfolder(char *str);
static char * is_dotdelimiter(char *str);
static char * is_slashdelimiter(char *str);
#endif     /* OLD_CODE */

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
        OFFSET(struct config, cf_listenip), is_ipaddr
    },
    {
        "ListenPort", CF_INT_PLUS, "20026",
        OFFSET(struct config, cf_listenport),is_port
    },
    {
        "TimeOut", CF_INT_PLUS, "10",
        OFFSET(struct config, cf_timeout), is_timeout
    },
    {
        "SyslogFacility" , CF_STRING, "local1",
        OFFSET(struct config, cf_syslogfacility), is_syslog_facility
    },
    {
        "ErrorAction", CF_STRING, "tempfail",
        OFFSET(struct config, cf_erroraction), is_erroraction
    },
    {
        "CommandPort", CF_INT_PLUS, "17777",
        OFFSET(struct config, cf_commandport), is_port
    },
    {
        "AdminPassword", CF_STRING, NULL,
        OFFSET(struct config, cf_adminpassword), NULL
    },
    {
        "CommandMaxClients", CF_INT_PLUS, "16",
        OFFSET(struct config, cf_commandmaxclients), is_commandmaxclients
    },
    {
        "CommandTimeOut", CF_INT_PLUS, "300",
        OFFSET(struct config, cf_commandtimeout), is_timeout
    },
    {
        "SavePolicy", CF_STRING, "both",
        OFFSET(struct config, cf_savepolicy), is_savepolicy
    },
    {
        "MyDomain", CF_STRING, NULL,
        OFFSET(struct config, cf_mydomain), NULL
    },
    {
        "SaveMailAddress", CF_STRING, NULL,
        OFFSET(struct config, cf_savemailaddress), NULL
    },
    {
        "SaveIgnoreHeader", CF_STRING, "",
        OFFSET(struct config, cf_saveignoreheader), NULL
    },
    {
        "DefaultDomain", CF_STRING, "localhost.localdomain",
        OFFSET(struct config, cf_defaultdomain), is_not_null
    },

#ifdef OLD_CODE
    {
        "MailDir", CF_STRING, NULL,
        OFFSET(struct config, cf_maildir), is_writable_directory
    },
    {
        "MailFolder", CF_STRING, NULL,
        OFFSET(struct config, cf_mailfolder), is_mailfolder
    },
    {
        "DotDelimiter", CF_STRING, ",",
        OFFSET(struct config, cf_dotdelimiter), is_dotdelimiter
    },
    {
        "SlashDelimiter", CF_STRING, "_",
        OFFSET(struct config, cf_slashdelimiter), is_slashdelimiter
    },
#endif    /* OLD_CODE */

    {
        "LdapCheck", CF_INTEGER, "0",
        OFFSET(struct config, cf_ldapcheck), is_boolean
    },
    {
        "LdapServer", CF_STRING, "127.0.0.1",
        OFFSET(struct config, cf_ldapserver), is_ipaddr
    },
    {
        "LdapPort", CF_INT_PLUS, "389",
        OFFSET(struct config, cf_ldapport), is_port
    },
    {
        "LdapBindDn", CF_STRING, "",
        OFFSET(struct config, cf_ldapbinddn), NULL
    },
    {
        "LdapBindPassword", CF_STRING, "",
        OFFSET(struct config, cf_ldapbindpassword), NULL
    },
    {
        "LdapBaseDn", CF_STRING, "",
        OFFSET(struct config, cf_ldapbasedn), NULL
    },
    {
        "LdapMailFilter", CF_STRING, "(mail=%M)",
        OFFSET(struct config, cf_ldapmailfilter), NULL
    },
    {
        "LdapScope", CF_STRING, "subtree",
        OFFSET(struct config, cf_ldapscope), is_ldapscope
    },
    {
        "LdapTimeout", CF_INT_PLUS, "5",
        OFFSET(struct config, cf_ldaptimeout), is_timeout
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
 * �Ķ��ѿ������ꤹ��
 *
 * ����
 *      char *          ����ե�����Υѥ�
 *
 * �֤���
 *      �ʤ�
 */
void
set_environment(char *path)
{
    /* ����ե�����Υѥ�����Ū�ѿ��˥��å� */
    strncpy(config_path, path, PATH_MAX + 1);

    /* �����ϥ�٥��INFO������ */
    dgconfig_loglevel = LOGLVL_INFO;

    /* ������������� */
    dgloginit();
}

/*
 * init_config
 *
 * ��ǽ
 *     ����ե����ե�������Ǽ���빽¤�Τν����
 *
 * ����
 *     �ʤ�
 *
 * �֤���
 *     struct config *     ����
 */
struct config *
init_config()
{
    struct config *cfg = NULL;

    /* ������� */
    cfg = (struct config *)malloc(sizeof(struct config));
    if (cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "init_config", E_STR);
        exit (EXIT_MAIN);
    }

    /* ��¤�Τ�0������ */
    memset(cfg, '\0', sizeof(struct config)); 

    /* mutex�ѿ��ν���� */
    cfg->cf_ref_count_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    return cfg;
}

/*
 * free_modulelist
 *
 * ��ǽ
 *�������⥸�塼��ꥹ�Ȥγ���
 *
 * ����
 *      struct modulelist *list   ��������⥸�塼��ꥹ��
 *
 * �֤���
 *             �ʤ�
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
 * ��ǽ
 *      extra_config�β���
 *
 * ����
 *      struct extra_config *list   ��������⥸�塼��ꥹ��
 *
 * �֤���
 *             �ʤ�
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
 * ��ǽ
 *����������ե�������ɤ߹������¤�ΤΥ���������롣
 *
 * ����
 *      struct config *cfg       ������������ե����빽¤��
 *
 * �֤���
 *             �ʤ�
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


    /* extraconfig�Υꥹ�ȳ���*/
    for (p = mhandle_list; p != NULL; p = p->mh_next) {

        /* �ؿ�̾������ */
        sprintf(funcname, "%s_free_config", p->mh_modulename);

        /* �ؿ��ݥ��󥿤����� */
        func_pointer = dlsym(p->mh_ptr, funcname);
        if ((error =dlerror()) != NULL) {
            SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, 
                        "free_config", funcname, error);
            continue;
        }

        (*func_pointer)(cfg);
    }

    /* exec�ϥ�ɥ顼�γ���*/
    free_modulelist(cfg->cf_exec_header);
    free_modulelist(cfg->cf_exec_body);
    free_modulelist(cfg->cf_exec_eom);
    free_modulelist(cfg->cf_exec_abort);

    /* �嵭��extraconfig�Υꥹ�ȳ������ޤ������ᡢ��Ƭ�Υݥ���Ȥ�����������*/
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

#ifdef OLD_CODE
    if (cfg->cf_maildir != NULL) {
        free(cfg->cf_maildir);
    }
    if (cfg->cf_mailfolder != NULL) {
        free(cfg->cf_mailfolder);
    }
    if (cfg->cf_dotdelimiter != NULL) {
        free(cfg->cf_dotdelimiter);
    }
    if (cfg->cf_slashdelimiter != NULL) {
        free(cfg->cf_slashdelimiter);
    }
#endif    /* OLD_CODE */

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
 * config��¤�Τλ��ȥ����󥿤����䤷���ݥ��󥿤��֤�
 *
 * ����
 *      �ʤ�
 * �֤���
 *      struct config *         config��¤�ΤΥݥ���
 */
struct config *
config_retrieve()
{
    struct config *ret_ptr;

    pthread_mutex_lock(&cur_cfg->cf_ref_count_lock);

    /* ���ȥ����󥿤����䤹 */
    cur_cfg->cf_ref_count++;
    /* config��¤�ΤΥݥ��󥿤�������� */
    ret_ptr = cur_cfg;

    pthread_mutex_unlock(&cur_cfg->cf_ref_count_lock);

    return ret_ptr;
}

/*
 * config_release
 *
 * config��¤�Τλ��ȥ����󥿤򸺤餹
 *
 * ����
 *      struct config *         config��¤�ΤΥݥ���
 *                              (config_retrieve()�Ǽ������줿���)
 * �֤���
 *      �ʤ�
 *
 */
void
config_release(struct config *cfg)
{
    pthread_mutex_lock(&cfg->cf_ref_count_lock);

    cfg->cf_ref_count--;

    if (cfg->cf_reloaded == TRUE && cfg->cf_ref_count < 1) {
        /* ¾��ï�⻲�Ȥ��Ƥ��ʤ���в��� */
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
 * ����ե�����κ��ɤ߹��ߤ�Ԥʤ�
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      R_SUCCESS       ����
 *      R_POSITIVE      ���˥������
 *      R_ERROR         ���顼
 */
int
reload_config()
{
    struct config *cfg = NULL;
    struct config *old = NULL;
    int ret;

    if (config_reloading == TRUE) {

        /* ������� */
        SYSLOGERROR(ERR_CONFIG_RELOADING);
        return R_POSITIVE;
    }

    /* ����ե�������ɤ߹��� */
    ret = set_config(config_path, &cfg);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    if (cur_cfg != NULL) {
        config_reloading = TRUE;
    }

    /* �ݥ��󥿤�Ҥ��Ѥ� */
    pthread_mutex_lock(&config_lock);
    old = cur_cfg;
    cur_cfg = cfg;
    pthread_mutex_unlock(&config_lock);

    if (old != NULL) {
        pthread_mutex_lock(&old->cf_ref_count_lock);
        if (old->cf_ref_count < 1) {
            /* ¾��ï�⻲�Ȥ��Ƥ��ʤ���в��� */
            pthread_mutex_unlock(&old->cf_ref_count_lock);
            free_config(old);
            pthread_mutex_lock(&config_lock);
            config_reloading = FALSE;
            pthread_mutex_unlock(&config_lock);
        } else {
            /* ������ǽ�˥ޡ��� */
            old->cf_reloaded = TRUE;
            pthread_mutex_unlock(&old->cf_ref_count_lock);
        }
    }

    return R_SUCCESS;
}

/*
 * is_timeout
 *
 * ��ǽ
 *     �����ॢ�����ÿ��Υ����å�
 *
 * ����
 *     int value            �����å������� 
 *
 * �֤���
 *      NULL                ����
 *      ERR_CONF_TIMEOUT    ���顼��å�����
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
 * ��ǽ
 *     Ʊ����³��ǽ���Υ����å�
 *
 * ����
 *     int value            �����å������� 
 *
 * �֤���
 *      NULL                ����
 *      ERR_CONF_COMMANDMAXCLIENTS    ���顼��å�����
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
 * ��ǽ
 *     ���顼���������Υ����å�
 *
 * ����
 *      char *str      �����å�����ʸ���� 
 *
 * �֤���
 *      NULL                    ���� 
 *      ERR_CONF_ERRORACTION    ���顼��å�����
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
 * ��ǽ
 *     �����֥ݥꥷ���Υ����å�
 *
 * ����
 *      char *str   �����å�����ʸ����    
 *
 * �֤���
 *      NULL                   ����
 *      ERR_CONF_SAVEPOLICY    ���顼��å�����
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

#ifdef OLD_CODE

/*
 * is_mailforder
 *
 * ��ǽ
 *    �᡼��ե�������Υ����å� 
 *
 * ����
 *      char *str   �����å�����ʸ����    
 *
 * �֤���
 *      NULL                   ����
 *      ERR_CONF_MAILFOLDER    ���顼��å�����
 */
char *
is_mailfolder(char *str)
{
    char string[] = CHAR_MAILFOLDER;
    int  i, j;

    /* ʸ�������Ƭ����.�פǤʤ����Ȥγ�ǧ */
    if (str[0] != '.') {
        return ERR_CONF_MAILFOLDER);
    }

    for (i = 0; str[i] != '\0'; i++) {
        /*��.�פ�Ϣ³���Ƥ��ʤ����Ȥγ�ǧ */
        if ((str[i] == '.') && (str[i+1] == '.')) {
            return ERR_CONF_MAILFOLDER);
        }
        /* �᡼��ե������̾���Ȥ���Ŭ�ڤ�ʸ�����Ȥ��Ƥ��뤳�Ȥγ�ǧ */
        for (j = 0; string[j] != '\0'; j++) {
            if (str[i] == string[j]) {
                break;
            }
        }
        /* ʸ�������פ��뤳�Ȥʤ�ȴ������硢���顼 */
        if (string[j] == '\0') {
            return ERR_CONF_MAILFOLDER);
        }
    }
    /* ʸ����κǸ夬�.�Ǥʤ����Ȥγ�ǧ */
    if (str[i-1] == '.') {
        return ERR_CONF_MAILFOLDER);
    }
    return NULL;
} 

/*
 * is_dotdelimiter
 *
 * ��ǽ
 *    .���֤�����ʸ���Υ����å� 
 *
 * ����
 *      char *str   �����å�����ʸ����    
 *
 * �֤���
 *      NULL                     ���� 
 *      ERR_CONF_DOTDELIMITER    ���顼��å�����
 */
char *
is_dotdelimiter(char *str)
{
    char string[] = CHAR_DOT_DELIMITER;
    int i;

    /* ��ʸ���Ǥ��뤫 */
    if (str[1] != '\0') {
        return ERR_CONF_DOTDELIMITER);
    }

    /* ʸ�������å� */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ���פ��뤳�Ȥʤ�ȴ���Ƥ��ޤä����ϡ���ȿ����ʸ�� */
    if (string[i] == '\0' ) {
        return ERR_CONF_DOTDELIMITER);
    }
    return NULL;
} 

/*
 * is_slashdelimiter
 *
 * ��ǽ
 *    /���֤�����ʸ���Υ����å� 
 *
 * ����
 *      char *str   �����å�����ʸ����    
 *
 * �֤���
 *      NULL                       ����
 *      ERR_CONF_SLASHDELIMITER    ���顼��å�����
 */
char *
is_slashdelimiter(char *str)
{
    char string[] = CHAR_SLASH_DELIMITER;
    int i;

    /* ��ʸ���Ǥ��뤫 */
    if (str[1] != '\0') {
        return ERR_CONF_SLASHDELIMITER);
    }

    /* ʸ�������å� */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ���פ��뤳�Ȥʤ�ȴ���Ƥ��ޤä����ϡ���ȿ����ʸ�� */
    if (string[i] == '\0' ) {
        return ERR_CONF_SLASHDELIMITEn;
    }
    return NULL;
}

#endif    /* OLD_CODE */

/*
 * is_not_null
 *
 * ��ǽ
 *    ʸ�������äƤ��뤫�Υ����å� 
 *
 * ����
 *      char *str   �����å�����ʸ����    
 *
 * �֤���
 *      NULL                 ���� 
 *      ERR_CONF_DEFAULTDOMAIN    ���顼��å�����
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
 * ��ǽ
 *    erroraction���Ѵ����Ƴ�Ǽ�������
 *
 * ����
 *    struct config *cfg   �ǡ������Ǽ���빽¤��    
 *
 * �֤���
 *      R_SUCCESS    ����
 *      R_ERROR      ����
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
 * ��ǽ
 *    savepolicy���Ѵ����Ƴ�Ǽ�������
 *
 * ����
 *    struct config *cfg   �ǡ������Ǽ���빽¤��    
 *
 * �֤���
 *      R_SUCCESS   ����
 *      R_ERROR     ����
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
 * ��ǽ
 *    ldapscope���Ѵ����Ƴ�Ǽ�������
 *
 * ����
 *    struct config *cfg   �ǡ������Ǽ���빽¤��    
 *
 * �֤���
 *      R_SUCCESS    ����
 *      R_ERROR      ���� 
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
 * ��ǽ
 *    saveignoreheader���Ѵ����Ƴ�Ǽ�������
 *
 * ����
 *    struct config *cfg   �ǡ������Ǽ���빽¤��    
 *
 * �֤���
 *      R_SUCCESS    ����
 *      R_ERROR      ����
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

#ifdef OLD_CODE
        SYSLOGWARNING(ERR_CONF_CONV_SAVEIGNOREHEADER);
#endif    /* OLD_CODE */

        free(cfg->cf_saveignoreheader_regex);
        cfg->cf_saveignoreheader_regex = NULL;
        return R_ERROR;
    }
    return R_SUCCESS;
}

/*
 * conv_config
 *
 * ��ǽ
 *    config���Ѵ����Ƴ�Ǽ�������
 *
 * ����
 *    struct config *cfg   �ǡ������Ǽ���빽¤��    
 *
 * �֤���
 *      R_SUCCESS    ����
 *      R_ERROR    �۾�
 */
int
conv_config(struct config *cfg)
{
    int ret;

    /* erroraction���Ѵ� */
    ret = conv_erroraction(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_ERRORACTION);
        return R_ERROR;
    }
    /* savepolicy���Ѵ� */
    ret = conv_savepolicy(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEPOLICY);
        return R_ERROR;
    }
    /* ldapscopen���Ѵ� */
    ret = conv_ldapscope(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_LDAPSCOPE);
        return R_ERROR;
    }
    /* saveignoreheader���Ѵ� */
    ret = conv_saveignoreheader(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEIGNOREHEADER);
        return R_ERROR;
    }
    /* mydomain��ꥹ�Ȥ˳�Ǽ */
    cfg->cf_mydomain_list = split_comma(cfg->cf_mydomain);
    /* savemailaddress��ꥹ�Ȥ˳�Ǽ */
    cfg->cf_savemailaddress_list = split_comma(cfg->cf_savemailaddress);
    
    return R_SUCCESS;
}

/*
 * msy_module_init
 *
 * ��ǽ
 *      �ƥ⥸�塼���init�ؿ���¹Ԥ���
 *
 * ����
 *      struct cfentry **cfe    config entry��¤��(�����Ϥ�)
 *      size_t cfesize          config entry��¤�ΤΥ�����(�����Ϥ�)
 *      struct config  **cfg    config ��¤��(�����Ϥ�)
 *      size_t cfgsize          config ��¤�ΤΥ�����(�����Ϥ�)
 *
 * �֤���
 *             0        ����
 *             1        �۾�
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
        /* �ؿ�̾������ */
        sprintf(funcname, "%s_init", p->mh_modulename);

        /* �ؿ��ݥ��󥿤����� */
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

    // ���ɤ���Ƥ���⥸�塼�����list�򸡺���dlsym��Ԥ�
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

    /* offset����ƥ⥸�塼���config�ݥ��󥿤��Ǽ���� */
    for (excf = (*cfg)->cf_extraconfig; 
                 excf != NULL; excf = excf->excf_next) {
        excf->excf_config = (void *)((char *)*cfg + (size_t)excf->excf_config);
    }

    return 0;
}

/*
 * set_dlsymbol
 *
 * ��ǽ
 *������modulelist�˴ؿ��ؤΥݥ��󥿤򥻥å�
 *
 * ����
 *      char *mh_modulename     dlopen����Ƥ���⥸�塼���̾��
 *      void *dlptr             �⥸�塼��ϥ�ɥ�
 *      struct modulelist *list �ƴؿ��Υ⥸�塼��ꥹ��
 *
 * �֤���
 *             0        ����
 *             1        �۾�
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

            /* �ؿ��ݥ��󥿤����� */
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
 * ��ǽ
 *    config���ɤ߹������(�縵)
 *
 * ����
 *     *file  ����ե�����Υѥ�
 *    **cfg   �ɤ߹��������ե��������γ�Ǽ��
 *
 * �֤���
 *      R_SUCCESS       ����
 *      R_ERROR         ���顼
 */
int
set_config(char *file, struct config **cfg)
{
    int ret;
    char *msg;
    size_t cfesize = sizeof(struct cfentry) * COUNT;
    size_t cfgsize = sizeof(struct config);
    struct cfentry *new_cfe = NULL;

    /* ���ν������ɸ�२�顼���Ϥء�*/
    dgloginit();

    /* ����ե�������Ǽ���빽¤�Τν���� */
    *cfg = init_config();

    /* cfe��¤�Τ�ҡ����ΰ�إ��ԡ� */
    new_cfe = (struct cfentry *)malloc(cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "mlfi_connect", E_STR);
        return R_ERROR;
    }
    memcpy(new_cfe, &cfe, cfesize);

    /* �ƥ⥸�塼���init�ؿ���¹� */
    ret = msy_module_init(&new_cfe, &cfesize, cfg, &cfgsize);
    if (ret != 0) {
        free(new_cfe);
        free(*cfg);
        *cfg = NULL;
        return R_ERROR;
    }
    
    /* ����ե����ե�������ɤ߹��� */ 
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

    /* ����ե��������ѹ� */
    msy_module_modconfig(cfg);
        
    /* ����ե����ե������ɬ���ͥ����å� */
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

    /* cf_msyhostname�γ�Ǽ */
    (*cfg)->cf_msyhostname = msy_hostname;

    /* ����ե����ե�������Ѵ��ȳ�Ǽ */
    ret = conv_config(*cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONVERT);
        free_config(*cfg);
        return R_ERROR;
    }

    /* �������ե�����ƥ��Υ����å� */
    msg = is_syslog_facility((*cfg)->cf_syslogfacility);
    if (msg != NULL) {
        SYSLOGWARNING("%s", msg);
        free_config(*cfg);
        return R_ERROR;
    }
    
    /* ��������򥳥�ե����ե�����˹�碌���ѹ� */
    dglogchange(IDENT, (*cfg)->cf_syslogfacility);

    return R_SUCCESS;
}

/*
 * msy_module_modconfig
 *
 * ��ǽ
 *�������ƥ⥸�塼���modconfig�ؿ���¹Ԥ���
 *
 * ����
 *      struct cfentry **cfe    config entry��¤��(�����Ϥ�)
 *      size_t cfesize          config entry��¤�ΤΥ�����(�����Ϥ�)
 *      struct config  **cfg    config ��¤��(�����Ϥ�)
 *      size_t cfgsize          config ��¤�ΤΥ�����(�����Ϥ�)
 *
 * �֤���
 *             0        ����
 *             1        �۾�
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

        /* �ؿ�̾������ */
        sprintf(funcname, "%s_mod_extra_config", p->mh_modulename);

        /* �ؿ��ݥ��󥿤����� */
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
 * ��ǽ
 *    ZipCommand �����å�
 *
 * ����
 *      char *str              ���ޥ�ɥѥ�
 *
 * �֤���
 *      NULL                   ����
 *      ���顼��å�����       �۾�
 */
char *
is_executable_file(char *str)
{
    struct stat st;
    char *space;
    char *cmd;

    /* �����*/
    space = NULL;
    cmd = NULL;

    /* ���ޥ�ɤ�cmd�˥��ԡ�����*/
    cmd = strdup(str);
    if (cmd == NULL) {
        return ERR_CONF_ALLOC;
    }

    /* ���ޥ�ɥե�����̾��ʬ��*/
    /* ���ڡ�����õ������*/
    space = strchr(cmd, (int)' ');
    /* ���ޥ�ɥե�����̾����ڤ�*/
    if (space != NULL) {
        *space = '\0';
    }

    /* �ե����륳�ޥ��¸�ߥ����å�*/
    if (stat(cmd, &st) == -1) {
        free(cmd);
        return ERR_FILE_EXIST;
    }

    /* �ե�����¹Ը��³�ǧ*/
    if (access(cmd, X_OK) != 0) {
        free(cmd);
        return ERR_FILE_EXECUTE_PERMITION;
    }

    /* ʸ����γ���*/
    free(cmd);

    return (NULL);
}

/*
 * is_usable_password
 *
 * ��ǽ
 *    �Ϥ����ѥ���ɤ�����å�����
 *
 * ����
 *    *password                �����å��ѥ����
 *
 * �֤���
 *      ���顼��å�����       �۾�
 *      NULL                   ����
 */
char *
is_usable_password(char *password) 
{
    char *is_null_msg;
    char string[] = CHAR_PASSWORD;
    int i,j;

    /* �����å��ѥ����NULL*/
    is_null_msg = is_not_null(password);
    if (is_null_msg != NULL) {
        return ERR_PASSWORD_NULL;
    }

    /* �����å�����ʸ��*/
    for (i = 0; password[i] != '\0'; i++) {
        /* �ѥ���ɤȤ���Ŭ�ڤ�ʸ�����Ȥ��Ƥ��뤳�Ȥγ�ǧ */
        for (j = 0; string[j] != '\0'; j++) {
            if (password[i] == string[j]) {
                break;
            }
        }
        /* ʸ�������פ��뤳�Ȥʤ�ȴ������硢���顼 */
        if (string[j] == '\0') {
            return (ERR_INVALID_PASSWORD);
        }
    }
    /* ������λ*/
    return NULL;
}

/*
 * cmd_strrep
 *
 * ���ޥ��̾�����ץ�������ڤꤷ�ơ��ꥹ�Ȥ˳�Ǽ����
 *
 * ����
 *      char *str              ����ʸ����
 *      char *sep              ���ڤ�ʸ
 *      char **real            ���ݤ������ޥ�ɤ��ΰ�Υݥ���Ȥ��ݻ�
 *      int epoo               extend part of option
 *
 * �֤���
 *      cmd_list       ����
 *      NULL           ���顼
 */
char **
cmd_strrep(char *str, char sep, char **real, int epoo)
{
    int len;
    int i;
    char *cmd;
    char **cmd_list;
    char *p;

    /* �����*/
    len = 1;
    i = 0;
    cmd = NULL;
    cmd_list = NULL;

    /* epoo ���ͥ��顼*/
    if (epoo < 1) {
        SYSLOGERROR(ERR_EXTEND_PART_OPTION_NUM, "cmd_strrep");
        return NULL;
    }

    /*���Ѥ��Ƥ��륳�ޥ�ɤ��ΰ����ݤ���*/
    cmd = strdup(str);
    if (cmd == NULL) {
        SYSLOGERROR(ERR_MALLOC, "cmd_strrep", E_STR);        
        return NULL;
    }

    /* p�ݥ���Ȥ�cmd����Ƭ������*/
    p = cmd;

    /* sep�ο���׻�����*/
    while (*p != '\0') {
        /*sep��õ��*/
        if(*p == sep) {
            len++;
        }
        /* �롼�פΥݥ���Ⱦ夬��*/
        p++;
    }

    /*ʸ��������γ���*/
    cmd_list = (char **)malloc(sizeof(char *) * (len + epoo));
    if (cmd_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, cmd_strrep, E_STR);
        free(cmd);
        return NULL;
    }

    /* p�ݥ���Ȥ�cmd����Ƭ�˼���*/
    p = cmd;
    cmd_list[i] = p;
    i++;

    /*�����֤���ʸ�����ʬ��*/
    while (*p != '\0') {
        /*sep��õ��*/
        if(*p == sep) {
            *p = '\0';
            /*����ʸ���������*/
            p++;
            /*����˳�Ǽ����*/
            cmd_list[i] = p;
            i++;
            continue;
        }

        /* �롼�פΥݥ���Ⱦ夬��*/
        p++;
    }

    /* �Ĥä��ս��NULL�����ꤹ��*/
    for (i = 0; i < epoo; i++) {
        cmd_list[len + i] = NULL;
    }

    /* real command��argscommand���֤�*/
    *real = cmd;
    return cmd_list;
}

