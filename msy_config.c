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

/* ¥×¥í¥È¥¿¥¤¥×Àë¸À */
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
 * ´Ä¶­ÊÑ¿ô¤òÀßÄê¤¹¤ë
 *
 * °ú¿ô
 *      char *          ÀßÄê¥Õ¥¡¥¤¥ë¤Î¥Ñ¥¹
 *
 * ÊÖ¤êÃÍ
 *      ¤Ê¤·
 */
void
set_environment(char *path)
{
    /* ÀßÄê¥Õ¥¡¥¤¥ë¤Î¥Ñ¥¹¤òÀÅÅªÊÑ¿ô¤Ë¥»¥Ã¥È */
    strncpy(config_path, path, PATH_MAX + 1);

    /* ¥í¥°½ÐÎÏ¥ì¥Ù¥ë¤òINFO¤ËÀßÄê */
    dgconfig_loglevel = LOGLVL_INFO;

    /* ¥í¥°½ÐÎÏÀè¤ò½é´ü²½ */
    dgloginit();
}

/*
 * init_config
 *
 * µ¡Ç½
 *     ¥³¥ó¥Õ¥£¥°¥Õ¥¡¥¤¥ë¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ¤Î½é´ü²½
 *
 * °ú¿ô
 *     ¤Ê¤·
 *
 * ÊÖ¤êÃÍ
 *     struct config *     Àµ¾ï
 */
struct config *
init_config()
{
    struct config *cfg = NULL;

    /* ¥á¥â¥ê³ÎÊÝ */
    cfg = (struct config *)malloc(sizeof(struct config));
    if (cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "init_config", E_STR);
        exit (EXIT_MAIN);
    }

    /* ¹½Â¤ÂÎ¤ò0¤ÇËä¤á¤ë */
    memset(cfg, '\0', sizeof(struct config)); 

    /* mutexÊÑ¿ô¤Î½é´ü²½ */
    cfg->cf_ref_count_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    return cfg;
}

/*
 * free_modulelist
 *
 * µ¡Ç½
 *¡¡¡¡¡¡¥â¥¸¥å¡¼¥ë¥ê¥¹¥È¤Î³«Êü
 *
 * °ú¿ô
 *      struct modulelist *list   ³«Êü¤¹¤ë¥â¥¸¥å¡¼¥ë¥ê¥¹¥È
 *
 * ÊÖ¤êÃÍ
 *             ¤Ê¤·
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
 * µ¡Ç½
 *      extra_config¤Î²òÊü
 *
 * °ú¿ô
 *      struct extra_config *list   ²òÊü¤¹¤ë¥â¥¸¥å¡¼¥ë¥ê¥¹¥È
 *
 * ÊÖ¤êÃÍ
 *             ¤Ê¤·
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
 * µ¡Ç½
 *¡¡¡¡¡¡ÀßÄê¥Õ¥¡¥¤¥ë¤òÆÉ¤ß¹þ¤ó¤À¹½Â¤ÂÎ¤Î¥á¥â¥ê¤ò³«Êü¤¹¤ë¡£
 *
 * °ú¿ô
 *      struct config *cfg       ³«Êü¤¹¤ëÀßÄê¥Õ¥¡¥¤¥ë¹½Â¤ÂÎ
 *
 * ÊÖ¤êÃÍ
 *             ¤Ê¤·
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


    /* extraconfig¤Î¥ê¥¹¥È³«Êü*/
    for (p = mhandle_list; p != NULL; p = p->mh_next) {

        /* ´Ø¿ôÌ¾¤ÎÀ¸À® */
        sprintf(funcname, "%s_free_config", p->mh_modulename);

        /* ´Ø¿ô¥Ý¥¤¥ó¥¿¤ÎÂåÆþ */
        func_pointer = dlsym(p->mh_ptr, funcname);
        if ((error =dlerror()) != NULL) {
            SYSLOGERROR(ERR_CREATE_FUNC_HANDLE, 
                        "free_config", funcname, error);
            continue;
        }

        (*func_pointer)(cfg);
    }

    /* exec¥Ï¥ó¥É¥é¡¼¤Î³«Êü*/
    free_modulelist(cfg->cf_exec_header);
    free_modulelist(cfg->cf_exec_body);
    free_modulelist(cfg->cf_exec_eom);
    free_modulelist(cfg->cf_exec_abort);

    /* ¾åµ­¤Ëextraconfig¤Î¥ê¥¹¥È³«Êü¤·¤Þ¤·¤¿¤¿¤á¡¢ÀèÆ¬¤Î¥Ý¥¤¥ó¥È¤À¤±¤ò³«Êü¤¹¤ë*/
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
 * config¹½Â¤ÂÎ¤Î»²¾È¥«¥¦¥ó¥¿¤òÁý¤ä¤·¡¢¥Ý¥¤¥ó¥¿¤òÊÖ¤¹
 *
 * °ú¿ô
 *      ¤Ê¤·
 * ÊÖ¤êÃÍ
 *      struct config *         config¹½Â¤ÂÎ¤Î¥Ý¥¤¥ó¥¿
 */
struct config *
config_retrieve()
{
    struct config *ret_ptr;

    pthread_mutex_lock(&cur_cfg->cf_ref_count_lock);

    /* »²¾È¥«¥¦¥ó¥¿¤òÁý¤ä¤¹ */
    cur_cfg->cf_ref_count++;
    /* config¹½Â¤ÂÎ¤Î¥Ý¥¤¥ó¥¿¤ò¼èÆÀ¤¹¤ë */
    ret_ptr = cur_cfg;

    pthread_mutex_unlock(&cur_cfg->cf_ref_count_lock);

    return ret_ptr;
}

/*
 * config_release
 *
 * config¹½Â¤ÂÎ¤Î»²¾È¥«¥¦¥ó¥¿¤ò¸º¤é¤¹
 *
 * °ú¿ô
 *      struct config *         config¹½Â¤ÂÎ¤Î¥Ý¥¤¥ó¥¿
 *                              (config_retrieve()¤Ç¼èÆÀ¤µ¤ì¤¿¤â¤Î)
 * ÊÖ¤êÃÍ
 *      ¤Ê¤·
 *
 */
void
config_release(struct config *cfg)
{
    pthread_mutex_lock(&cfg->cf_ref_count_lock);

    cfg->cf_ref_count--;

    if (cfg->cf_reloaded == TRUE && cfg->cf_ref_count < 1) {
        /* Â¾¤ËÃ¯¤â»²¾È¤·¤Æ¤¤¤Ê¤±¤ì¤Ð²òÊü */
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
 * ÀßÄê¥Õ¥¡¥¤¥ë¤ÎºÆÆÉ¤ß¹þ¤ß¤ò¹Ô¤Ê¤¦
 *
 * °ú¿ô
 *      ¤Ê¤·
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS       Àµ¾ï
 *      R_POSITIVE      ´û¤Ë¥ê¥í¡¼¥ÉÃæ
 *      R_ERROR         ¥¨¥é¡¼
 */
int
reload_config()
{
    struct config *cfg = NULL;
    struct config *old = NULL;
    int ret;

    if (config_reloading == TRUE) {

        /* ¥ê¥í¡¼¥ÉÃæ */
        SYSLOGERROR(ERR_CONFIG_RELOADING);
        return R_POSITIVE;
    }

    /* ÀßÄê¥Õ¥¡¥¤¥ë¤òÆÉ¤ß¹þ¤ß */
    ret = set_config(config_path, &cfg);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    if (cur_cfg != NULL) {
        config_reloading = TRUE;
    }

    /* ¥Ý¥¤¥ó¥¿¤ò·Ò¤®ÊÑ¤¨ */
    pthread_mutex_lock(&config_lock);
    old = cur_cfg;
    cur_cfg = cfg;
    pthread_mutex_unlock(&config_lock);

    if (old != NULL) {
        pthread_mutex_lock(&old->cf_ref_count_lock);
        if (old->cf_ref_count < 1) {
            /* Â¾¤ËÃ¯¤â»²¾È¤·¤Æ¤¤¤Ê¤±¤ì¤Ð²òÊü */
            pthread_mutex_unlock(&old->cf_ref_count_lock);
            free_config(old);
            pthread_mutex_lock(&config_lock);
            config_reloading = FALSE;
            pthread_mutex_unlock(&config_lock);
        } else {
            /* ²òÊü²ÄÇ½¤Ë¥Þ¡¼¥¯ */
            old->cf_reloaded = TRUE;
            pthread_mutex_unlock(&old->cf_ref_count_lock);
        }
    }

    return R_SUCCESS;
}

/*
 * is_timeout
 *
 * µ¡Ç½
 *     ¥¿¥¤¥à¥¢¥¦¥ÈÉÃ¿ô¤Î¥Á¥§¥Ã¥¯
 *
 * °ú¿ô
 *     int value            ¥Á¥§¥Ã¥¯¤¹¤ëÃÍ 
 *
 * ÊÖ¤êÃÍ
 *      NULL                Àµ¾ï
 *      ERR_CONF_TIMEOUT    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
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
 * µ¡Ç½
 *     Æ±»þÀÜÂ³²ÄÇ½¿ô¤Î¥Á¥§¥Ã¥¯
 *
 * °ú¿ô
 *     int value            ¥Á¥§¥Ã¥¯¤¹¤ëÃÍ 
 *
 * ÊÖ¤êÃÍ
 *      NULL                Àµ¾ï
 *      ERR_CONF_COMMANDMAXCLIENTS    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
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
 * µ¡Ç½
 *     ¥¨¥é¡¼¥¢¥¯¥·¥ç¥ó¤Î¥Á¥§¥Ã¥¯
 *
 * °ú¿ô
 *      char *str      ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó 
 *
 * ÊÖ¤êÃÍ
 *      NULL                    Àµ¾ï 
 *      ERR_CONF_ERRORACTION    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
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
 * µ¡Ç½
 *     ¥»¡¼¥Ö¥Ý¥ê¥·¡¼¤Î¥Á¥§¥Ã¥¯
 *
 * °ú¿ô
 *      char *str   ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó    
 *
 * ÊÖ¤êÃÍ
 *      NULL                   Àµ¾ï
 *      ERR_CONF_SAVEPOLICY    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
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
 * µ¡Ç½
 *    ¥á¡¼¥ë¥Õ¥©¥ë¥À¡¼¤Î¥Á¥§¥Ã¥¯ 
 *
 * °ú¿ô
 *      char *str   ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó    
 *
 * ÊÖ¤êÃÍ
 *      NULL                   Àµ¾ï
 *      ERR_CONF_MAILFOLDER    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
 */
char *
is_mailfolder(char *str)
{
    char string[] = CHAR_MAILFOLDER;
    int  i, j;

    /* Ê¸»úÎó¤ÎÀèÆ¬¤¬¡Ö.¡×¤Ç¤Ê¤¤¤³¤È¤Î³ÎÇ§ */
    if (str[0] != '.') {
        return ERR_CONF_MAILFOLDER);
    }

    for (i = 0; str[i] != '\0'; i++) {
        /*¡Ö.¡×¤¬Ï¢Â³¤·¤Æ¤¤¤Ê¤¤¤³¤È¤Î³ÎÇ§ */
        if ((str[i] == '.') && (str[i+1] == '.')) {
            return ERR_CONF_MAILFOLDER);
        }
        /* ¥á¡¼¥ë¥Õ¥©¥ë¥À¤ÎÌ¾Á°¤È¤·¤ÆÅ¬ÀÚ¤ÊÊ¸»ú¤¬»È¤ï¤ì¤Æ¤¤¤ë¤³¤È¤Î³ÎÇ§ */
        for (j = 0; string[j] != '\0'; j++) {
            if (str[i] == string[j]) {
                break;
            }
        }
        /* Ê¸»ú¤¬¹çÃ×¤¹¤ë¤³¤È¤Ê¤¯È´¤±¤¿¾ì¹ç¡¢¥¨¥é¡¼ */
        if (string[j] == '\0') {
            return ERR_CONF_MAILFOLDER);
        }
    }
    /* Ê¸»úÎó¤ÎºÇ¸å¤¬¡.¤Ç¤Ê¤¤¤³¤È¤Î³ÎÇ§ */
    if (str[i-1] == '.') {
        return ERR_CONF_MAILFOLDER);
    }
    return NULL;
} 

/*
 * is_dotdelimiter
 *
 * µ¡Ç½
 *    .¤ÎÃÖ¤­´¹¤¨Ê¸»ú¤Î¥Á¥§¥Ã¥¯ 
 *
 * °ú¿ô
 *      char *str   ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó    
 *
 * ÊÖ¤êÃÍ
 *      NULL                     Àµ¾ï 
 *      ERR_CONF_DOTDELIMITER    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
 */
char *
is_dotdelimiter(char *str)
{
    char string[] = CHAR_DOT_DELIMITER;
    int i;

    /* °ìÊ¸»ú¤Ç¤¢¤ë¤« */
    if (str[1] != '\0') {
        return ERR_CONF_DOTDELIMITER);
    }

    /* Ê¸»ú¥Á¥§¥Ã¥¯ */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ¹çÃ×¤¹¤ë¤³¤È¤Ê¤¯È´¤±¤Æ¤·¤Þ¤Ã¤¿¾ì¹ç¤Ï¡¢°ãÈ¿¤·¤¿Ê¸»ú */
    if (string[i] == '\0' ) {
        return ERR_CONF_DOTDELIMITER);
    }
    return NULL;
} 

/*
 * is_slashdelimiter
 *
 * µ¡Ç½
 *    /¤ÎÃÖ¤­´¹¤¨Ê¸»ú¤Î¥Á¥§¥Ã¥¯ 
 *
 * °ú¿ô
 *      char *str   ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó    
 *
 * ÊÖ¤êÃÍ
 *      NULL                       Àµ¾ï
 *      ERR_CONF_SLASHDELIMITER    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
 */
char *
is_slashdelimiter(char *str)
{
    char string[] = CHAR_SLASH_DELIMITER;
    int i;

    /* °ìÊ¸»ú¤Ç¤¢¤ë¤« */
    if (str[1] != '\0') {
        return ERR_CONF_SLASHDELIMITER);
    }

    /* Ê¸»ú¥Á¥§¥Ã¥¯ */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ¹çÃ×¤¹¤ë¤³¤È¤Ê¤¯È´¤±¤Æ¤·¤Þ¤Ã¤¿¾ì¹ç¤Ï¡¢°ãÈ¿¤·¤¿Ê¸»ú */
    if (string[i] == '\0' ) {
        return ERR_CONF_SLASHDELIMITEn;
    }
    return NULL;
}

#endif    /* OLD_CODE */

/*
 * is_not_null
 *
 * µ¡Ç½
 *    Ê¸»ú¤¬Æþ¤Ã¤Æ¤¤¤ë¤«¤Î¥Á¥§¥Ã¥¯ 
 *
 * °ú¿ô
 *      char *str   ¥Á¥§¥Ã¥¯¤¹¤ëÊ¸»úÎó    
 *
 * ÊÖ¤êÃÍ
 *      NULL                 Àµ¾ï 
 *      ERR_CONF_DEFAULTDOMAIN    ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸
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
 * µ¡Ç½
 *    erroraction¤òÊÑ´¹¤·¤Æ³ÊÇ¼¤¹¤ë½èÍý
 *
 * °ú¿ô
 *    struct config *cfg   ¥Ç¡¼¥¿¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ    
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS    À®¸ù
 *      R_ERROR      ¼ºÇÔ
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
 * µ¡Ç½
 *    savepolicy¤òÊÑ´¹¤·¤Æ³ÊÇ¼¤¹¤ë½èÍý
 *
 * °ú¿ô
 *    struct config *cfg   ¥Ç¡¼¥¿¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ    
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS   À®¸ù
 *      R_ERROR     ¼ºÇÔ
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
 * µ¡Ç½
 *    ldapscope¤òÊÑ´¹¤·¤Æ³ÊÇ¼¤¹¤ë½èÍý
 *
 * °ú¿ô
 *    struct config *cfg   ¥Ç¡¼¥¿¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ    
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS    À®¸ù
 *      R_ERROR      ¼ºÇÔ 
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
 * µ¡Ç½
 *    saveignoreheader¤òÊÑ´¹¤·¤Æ³ÊÇ¼¤¹¤ë½èÍý
 *
 * °ú¿ô
 *    struct config *cfg   ¥Ç¡¼¥¿¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ    
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS    À®¸ù
 *      R_ERROR      ¼ºÇÔ
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
 * µ¡Ç½
 *    config¤òÊÑ´¹¤·¤Æ³ÊÇ¼¤¹¤ë½èÍý
 *
 * °ú¿ô
 *    struct config *cfg   ¥Ç¡¼¥¿¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ    
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS    Àµ¾ï
 *      R_ERROR    °Û¾ï
 */
int
conv_config(struct config *cfg)
{
    int ret;

    /* erroraction¤ÎÊÑ´¹ */
    ret = conv_erroraction(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_ERRORACTION);
        return R_ERROR;
    }
    /* savepolicy¤ÎÊÑ´¹ */
    ret = conv_savepolicy(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEPOLICY);
        return R_ERROR;
    }
    /* ldapscopen¤ÎÊÑ´¹ */
    ret = conv_ldapscope(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_LDAPSCOPE);
        return R_ERROR;
    }
    /* saveignoreheader¤ÎÊÑ´¹ */
    ret = conv_saveignoreheader(cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONV_SAVEIGNOREHEADER);
        return R_ERROR;
    }
    /* mydomain¤ò¥ê¥¹¥È¤Ë³ÊÇ¼ */
    cfg->cf_mydomain_list = split_comma(cfg->cf_mydomain);
    /* savemailaddress¤ò¥ê¥¹¥È¤Ë³ÊÇ¼ */
    cfg->cf_savemailaddress_list = split_comma(cfg->cf_savemailaddress);
    
    return R_SUCCESS;
}

/*
 * msy_module_init
 *
 * µ¡Ç½
 *      ³Æ¥â¥¸¥å¡¼¥ë¤Îinit´Ø¿ô¤ò¼Â¹Ô¤¹¤ë
 *
 * °ú¿ô
 *      struct cfentry **cfe    config entry¹½Â¤ÂÎ(»²¾ÈÅÏ¤·)
 *      size_t cfesize          config entry¹½Â¤ÂÎ¤Î¥µ¥¤¥º(»²¾ÈÅÏ¤·)
 *      struct config  **cfg    config ¹½Â¤ÂÎ(»²¾ÈÅÏ¤·)
 *      size_t cfgsize          config ¹½Â¤ÂÎ¤Î¥µ¥¤¥º(»²¾ÈÅÏ¤·)
 *
 * ÊÖ¤êÃÍ
 *             0        Àµ¾ï
 *             1        °Û¾ï
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
        /* ´Ø¿ôÌ¾¤ÎÀ¸À® */
        sprintf(funcname, "%s_init", p->mh_modulename);

        /* ´Ø¿ô¥Ý¥¤¥ó¥¿¤ÎÂåÆþ */
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

    // ¥í¡¼¥É¤µ¤ì¤Æ¤¤¤ë¥â¥¸¥å¡¼¥ëËè¤Ëlist¤ò¸¡º÷¤·dlsym¤ò¹Ô¤¦
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

    /* offset¤«¤é³Æ¥â¥¸¥å¡¼¥ë¤Îconfig¥Ý¥¤¥ó¥¿¤ò³ÊÇ¼¤¹¤ë */
    for (excf = (*cfg)->cf_extraconfig; 
                 excf != NULL; excf = excf->excf_next) {
        excf->excf_config = (void *)((char *)*cfg + (size_t)excf->excf_config);
    }

    return 0;
}

/*
 * set_dlsymbol
 *
 * µ¡Ç½
 *¡¡¡¡¡¡modulelist¤Ë´Ø¿ô¤Ø¤Î¥Ý¥¤¥ó¥¿¤ò¥»¥Ã¥È
 *
 * °ú¿ô
 *      char *mh_modulename     dlopen¤µ¤ì¤Æ¤¤¤ë¥â¥¸¥å¡¼¥ë¤ÎÌ¾Á°
 *      void *dlptr             ¥â¥¸¥å¡¼¥ë¥Ï¥ó¥É¥ë
 *      struct modulelist *list ³Æ´Ø¿ô¤Î¥â¥¸¥å¡¼¥ë¥ê¥¹¥È
 *
 * ÊÖ¤êÃÍ
 *             0        Àµ¾ï
 *             1        °Û¾ï
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

            /* ´Ø¿ô¥Ý¥¤¥ó¥¿¤ÎÂåÆþ */
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
 * µ¡Ç½
 *    config¤òÆÉ¤ß¹þ¤à½èÍý(Âç¸µ)
 *
 * °ú¿ô
 *     *file  ÀßÄê¥Õ¥¡¥¤¥ë¤Î¥Ñ¥¹
 *    **cfg   ÆÉ¤ß¹þ¤ó¤ÀÀßÄê¥Õ¥¡¥¤¥ë¾ðÊó¤Î³ÊÇ¼Àè
 *
 * ÊÖ¤êÃÍ
 *      R_SUCCESS       Àµ¾ï
 *      R_ERROR         ¥¨¥é¡¼
 */
int
set_config(char *file, struct config **cfg)
{
    int ret;
    char *msg;
    size_t cfesize = sizeof(struct cfentry) * COUNT;
    size_t cfgsize = sizeof(struct config);
    struct cfentry *new_cfe = NULL;

    /* ¥í¥°¤Î½é´ü²½¡ÊÉ¸½à¥¨¥é¡¼½ÐÎÏ¤Ø¡Ë*/
    dgloginit();

    /* ÀßÄê¥Õ¥¡¥¤¥ë¤ò³ÊÇ¼¤¹¤ë¹½Â¤ÂÎ¤Î½é´ü²½ */
    *cfg = init_config();

    /* cfe¹½Â¤ÂÎ¤ò¥Ò¡¼¥×ÎÎ°è¤Ø¥³¥Ô¡¼ */
    new_cfe = (struct cfentry *)malloc(cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "mlfi_connect", E_STR);
        return R_ERROR;
    }
    memcpy(new_cfe, &cfe, cfesize);

    /* ³Æ¥â¥¸¥å¡¼¥ë¤Îinit´Ø¿ô¤ò¼Â¹Ô */
    ret = msy_module_init(&new_cfe, &cfesize, cfg, &cfgsize);
    if (ret != 0) {
        free(new_cfe);
        free(*cfg);
        *cfg = NULL;
        return R_ERROR;
    }
    
    /* ¥³¥ó¥Õ¥£¥°¥Õ¥¡¥¤¥ë¤ÎÆÉ¤ß¹þ¤ß */ 
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

    /* ¥³¥ó¥Õ¥£¥°¤ÎÃÍÊÑ¹¹ */
    msy_module_modconfig(cfg);
        
    /* ¥³¥ó¥Õ¥£¥°¥Õ¥¡¥¤¥ë¤ÎÉ¬¿ÜÃÍ¥Á¥§¥Ã¥¯ */
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

    /* cf_msyhostname¤Î³ÊÇ¼ */
    (*cfg)->cf_msyhostname = msy_hostname;

    /* ¥³¥ó¥Õ¥£¥°¥Õ¥¡¥¤¥ë¤ÎÊÑ´¹¤È³ÊÇ¼ */
    ret = conv_config(*cfg);
    if (ret != R_SUCCESS) {
        SYSLOGWARNING(ERR_CONF_CONVERT);
        free_config(*cfg);
        return R_ERROR;
    }

    /* ¥·¥¹¥í¥°¥Õ¥¡¥·¥ê¥Æ¥£¤Î¥Á¥§¥Ã¥¯ */
    msg = is_syslog_facility((*cfg)->cf_syslogfacility);
    if (msg != NULL) {
        SYSLOGWARNING("%s", msg);
        free_config(*cfg);
        return R_ERROR;
    }
    
    /* ¥í¥°½ÐÎÏÀè¤ò¥³¥ó¥Õ¥£¥°¥Õ¥¡¥¤¥ë¤Ë¹ç¤ï¤»¤ÆÊÑ¹¹ */
    dglogchange(IDENT, (*cfg)->cf_syslogfacility);

    return R_SUCCESS;
}

/*
 * msy_module_modconfig
 *
 * µ¡Ç½
 *¡¡¡¡¡¡³Æ¥â¥¸¥å¡¼¥ë¤Îmodconfig´Ø¿ô¤ò¼Â¹Ô¤¹¤ë
 *
 * °ú¿ô
 *      struct cfentry **cfe    config entry¹½Â¤ÂÎ(»²¾ÈÅÏ¤·)
 *      size_t cfesize          config entry¹½Â¤ÂÎ¤Î¥µ¥¤¥º(»²¾ÈÅÏ¤·)
 *      struct config  **cfg    config ¹½Â¤ÂÎ(»²¾ÈÅÏ¤·)
 *      size_t cfgsize          config ¹½Â¤ÂÎ¤Î¥µ¥¤¥º(»²¾ÈÅÏ¤·)
 *
 * ÊÖ¤êÃÍ
 *             0        Àµ¾ï
 *             1        °Û¾ï
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

        /* ´Ø¿ôÌ¾¤ÎÀ¸À® */
        sprintf(funcname, "%s_mod_extra_config", p->mh_modulename);

        /* ´Ø¿ô¥Ý¥¤¥ó¥¿¤ÎÂåÆþ */
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
 * µ¡Ç½
 *    ZipCommand ¥Á¥§¥Ã¥¯
 *
 * °ú¿ô
 *      char *str              ¥³¥Þ¥ó¥É¥Ñ¥¹
 *
 * ÊÖ¤êÃÍ
 *      NULL                   Àµ¾ï
 *      ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸       °Û¾ï
 */
char *
is_executable_file(char *str)
{
    struct stat st;
    char *space;
    char *cmd;

    /* ½é´ü²½*/
    space = NULL;
    cmd = NULL;

    /* ¥³¥Þ¥ó¥É¤òcmd¤Ë¥³¥Ô¡¼¤¹¤ë*/
    cmd = strdup(str);
    if (cmd == NULL) {
        return ERR_CONF_ALLOC;
    }

    /* ¥³¥Þ¥ó¥É¥Õ¥¡¥¤¥ëÌ¾¤òÊ¬ÀÏ*/
    /* ¥¹¥Ú¡¼¥¹¤òÃµ¤¹¤³¤È*/
    space = strchr(cmd, (int)' ');
    /* ¥³¥Þ¥ó¥É¥Õ¥¡¥¤¥ëÌ¾¤ò¶èÀÚ¤ê*/
    if (space != NULL) {
        *space = '\0';
    }

    /* ¥Õ¥¡¥¤¥ë¥³¥Þ¥ó¥ÉÂ¸ºß¥Á¥§¥Ã¥¯*/
    if (stat(cmd, &st) == -1) {
        free(cmd);
        return ERR_FILE_EXIST;
    }

    /* ¥Õ¥¡¥¤¥ë¼Â¹Ô¸¢¸Â³ÎÇ§*/
    if (access(cmd, X_OK) != 0) {
        free(cmd);
        return ERR_FILE_EXECUTE_PERMITION;
    }

    /* Ê¸»úÎó¤Î³«Êü*/
    free(cmd);

    return (NULL);
}

/*
 * is_usable_password
 *
 * µ¡Ç½
 *    ÅÏ¤·¤¿¥Ñ¥¹¥ï¡¼¥É¤ò¥Á¥§¥Ã¥¯¤¹¤ë
 *
 * °ú¿ô
 *    *password                ¥Á¥§¥Ã¥¯¥Ñ¥¹¥ï¡¼¥É
 *
 * ÊÖ¤êÃÍ
 *      ¥¨¥é¡¼¥á¥Ã¥»¡¼¥¸       °Û¾ï
 *      NULL                   Àµ¾ï
 */
char *
is_usable_password(char *password) 
{
    char *is_null_msg;
    char string[] = CHAR_PASSWORD;
    int i,j;

    /* ¥Á¥§¥Ã¥¯¥Ñ¥¹¥ï¡¼¥ÉNULL*/
    is_null_msg = is_not_null(password);
    if (is_null_msg != NULL) {
        return ERR_PASSWORD_NULL;
    }

    /* ¥Á¥§¥Ã¥¯µö²ÄÊ¸»ú*/
    for (i = 0; password[i] != '\0'; i++) {
        /* ¥Ñ¥¹¥ï¡¼¥É¤È¤·¤ÆÅ¬ÀÚ¤ÊÊ¸»ú¤¬»È¤ï¤ì¤Æ¤¤¤ë¤³¤È¤Î³ÎÇ§ */
        for (j = 0; string[j] != '\0'; j++) {
            if (password[i] == string[j]) {
                break;
            }
        }
        /* Ê¸»ú¤¬¹çÃ×¤¹¤ë¤³¤È¤Ê¤¯È´¤±¤¿¾ì¹ç¡¢¥¨¥é¡¼ */
        if (string[j] == '\0') {
            return (ERR_INVALID_PASSWORD);
        }
    }
    /* À®¸ù½ªÎ»*/
    return NULL;
}

/*
 * cmd_strrep
 *
 * ¥³¥Þ¥ó¥ÉÌ¾¡¢¥ª¥×¥·¥ç¥ó¤ò¶èÀÚ¤ê¤·¤Æ¡¢¥ê¥¹¥È¤Ë³ÊÇ¼¤¹¤ë
 *
 * °ú¿ô
 *      char *str              ¸µ¤ÎÊ¸»úÎó
 *      char *sep              ¶èÀÚ¤êÊ¸
 *      char **real            ³ÎÊÝ¤·¤¿¥³¥Þ¥ó¥É¤ÎÎÎ°è¤Î¥Ý¥¤¥ó¥È¤òÊÝ»ý
 *      int epoo               extend part of option
 *
 * ÊÖ¤êÃÍ
 *      cmd_list       Àµ¾ï
 *      NULL           ¥¨¥é¡¼
 */
char **
cmd_strrep(char *str, char sep, char **real, int epoo)
{
    int len;
    int i;
    char *cmd;
    char **cmd_list;
    char *p;

    /* ½é´ü²½*/
    len = 1;
    i = 0;
    cmd = NULL;
    cmd_list = NULL;

    /* epoo ¤ÎÃÍ¥¨¥é¡¼*/
    if (epoo < 1) {
        SYSLOGERROR(ERR_EXTEND_PART_OPTION_NUM, "cmd_strrep");
        return NULL;
    }

    /*ÍøÍÑ¤·¤Æ¤¤¤ë¥³¥Þ¥ó¥É¤ÎÎÎ°è¤ò³ÎÊÝ¤¹¤ë*/
    cmd = strdup(str);
    if (cmd == NULL) {
        SYSLOGERROR(ERR_MALLOC, "cmd_strrep", E_STR);        
        return NULL;
    }

    /* p¥Ý¥¤¥ó¥È¤òcmd¤ÎÀèÆ¬¤ËÁ«°Ü*/
    p = cmd;

    /* sep¤Î¿ô¤ò·×»»¤¹¤ë*/
    while (*p != '\0') {
        /*sep¤òÃµ¤¹*/
        if(*p == sep) {
            len++;
        }
        /* ¥ë¡¼¥×¤Î¥Ý¥¤¥ó¥È¾å¤¬¤ë*/
        p++;
    }

    /*Ê¸»úÎóÇÛÎó¤Î³ÎÊÝ*/
    cmd_list = (char **)malloc(sizeof(char *) * (len + epoo));
    if (cmd_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, cmd_strrep, E_STR);
        free(cmd);
        return NULL;
    }

    /* p¥Ý¥¤¥ó¥È¤òcmd¤ÎÀèÆ¬¤Ë¼¨¤¹*/
    p = cmd;
    cmd_list[i] = p;
    i++;

    /*·«¤êÊÖ¤·¤ÇÊ¸»úÎó¤ÎÊ¬ÀÏ*/
    while (*p != '\0') {
        /*sep¤òÃµ¤¹*/
        if(*p == sep) {
            *p = '\0';
            /*¼¡¤ÎÊ¸»úÎó¤ËÁ«°Ü*/
            p++;
            /*ÇÛÎó¤Ë³ÊÇ¼¤¹¤ë*/
            cmd_list[i] = p;
            i++;
            continue;
        }

        /* ¥ë¡¼¥×¤Î¥Ý¥¤¥ó¥È¾å¤¬¤ë*/
        p++;
    }

    /* »Ä¤Ã¤¿²Õ½ê¤ËNULL¤òÀßÄê¤¹¤ë*/
    for (i = 0; i < epoo; i++) {
        cmd_list[len + i] = NULL;
    }

    /* real command¡¢argscommand¤òÊÖ¤¹*/
    *real = cmd;
    return cmd_list;
}

