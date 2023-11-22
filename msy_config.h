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
 * $RCSfile: msy_config.h,v $
 * $Revision: 1.23 $
 * $Date: 2009/10/29 09:19:45 $
 */

#ifndef _MSY_CONFIG_H_
#define _MSY_CONFIG_H_

#include <regex.h>

/* 数値の定義 */
#define COUNT (sizeof(cfe) / sizeof(struct cfentry))
#define MAX_TIME INT_MAX
#define MAX_CONNECTION 65535

#ifdef OLD_CODE
/* 比較文字の定義 */
#define CHAR_MAILFOLDER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.%,_&-+ "
#define CHAR_DOT_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "
#define CHAR_SLASH_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "
#endif    /* OLD_CODE */

/* ポリシー定義 */
#define NONE     0
#define ONLYFROM 1
#define ONLYTO   2
#define BOTH     3

/* プロトタイプ宣言 */
extern void set_environment(char *);
extern int reload_config();
extern struct config *config_retrieve();
extern void config_release(struct config *);
extern void free_config(struct config *cfg);
extern int set_config(char *file, struct config **);

/*****************************
 *  * モジュール名格納構造体
 *   *****************************/
struct modulelist {
    struct modulelist *mlist_next;
    char *mlist_modulename;
    char *mlist_funcname;
    void (*mlist_funcptr)();
};

/*****************************
 *  * モジュール構造体
 *   *****************************/
struct extra_config {
    struct extra_config *excf_next;
    char *excf_modulename;
    void *excf_config;
};

/*****************************
 *  * モジュールハンドル構造体
 *   *****************************/
struct modulehandle {
    struct modulehandle *mh_next;
    char *mh_modulename;
    void *mh_ptr;
};

struct config {
    pthread_mutex_t      cf_ref_count_lock;
    int                  cf_ref_count;
    int                  cf_reloaded;
    char                *cf_listenip;
    int                  cf_listenport;
    unsigned int         cf_timeout;
    char                *cf_syslogfacility;
    char                *cf_erroraction;
    int                  cf_commandport;
    char                *cf_adminpassword;
    int                  cf_commandmaxclients;
    unsigned int         cf_commandtimeout;
    char                *cf_savepolicy;
    char                *cf_mydomain;
    char                *cf_savemailaddress;
    char                *cf_saveignoreheader;
    char                *cf_defaultdomain;

#ifdef OLD_CODE
    char                *cf_maildir;
    char                *cf_mailfolder;
    char                *cf_dotdelimiter;
    char                *cf_slashdelimiter;  
#endif    /* OLD_CODE */

    int                  cf_ldapcheck;
    char                *cf_ldapserver;
    int                  cf_ldapport;
    char                *cf_ldapbinddn;
    char                *cf_ldapbindpassword;
    char                *cf_ldapbasedn;
    char                *cf_ldapmailfilter;
    char                *cf_ldapscope;
    unsigned int         cf_ldaptimeout;
    int                  cf_erroraction_conv;
    int                  cf_savepolicy_conv;
    int                  cf_ldapscope_conv;
    regex_t             *cf_saveignoreheader_regex;
    struct strlist      *cf_mydomain_list;
    struct strlist      *cf_savemailaddress_list;
    struct modulelist   *cf_exec_header;
    struct modulelist   *cf_exec_body;
    struct modulelist   *cf_exec_eom;
    struct modulelist   *cf_exec_eoh;
    struct modulelist   *cf_exec_abort;
    struct extra_config *cf_extraconfig;
    char                *cf_msyhostname;

};

/* プロトタイプ宣言 */
extern void set_environment(char *);
extern int reload_config();
extern struct config *config_retrieve();
extern void config_release(struct config *);
extern void free_config(struct config *cfg);
extern int set_config(char *file, struct config **);
extern char **cmd_strrep(char *, char, char **, int);
char * is_usable_password(char *str);
char * is_executable_file(char *str);
int msy_module_modconfig(struct config **cfg);

#endif // _MSY_CONFIG_H_
