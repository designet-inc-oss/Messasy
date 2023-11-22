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
//#include <tcutil.h>
//#include <tcrdb.h>
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

/* Messasy include file */
//#include "../messasy.h"
//#include "../msy_config.h"
//#include "../msy_readmodule.h"
//#include "../utils.h"
//#include "../log.h"
#include "messasy.h"
#include "msy_config.h"
#include "msy_readmodule.h"
#include "utils.h"
#include "log.h"
//#include "lib_lm.h"
#include "../lib_lm.h"

/* Header for my library */
#include "libenczip.h"

/* ���ʸ������� */
#define CHAR_MAILDROP_MAILFOLDER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.%,_&-+ "
#define CHAR_MAILDROP_DOT_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "
#define CHAR_MAILDROP_SLASH_DELIMITER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,-_ "

#define MYMODULE "enczip"

#define HEADER_FUNC     "enczip_exec_header"
#define BODY_FUNC       "enczip_exec_body"
#define EOM_FUNC        "enczip_exec_eom"
#define ABORT_FUNC      "enczip_exec_abort"
#define MODCONF_FUNC    "enczip_exec_modconf"

/* �ץ�ȥ�������� */
static int enczip_set_extra_config (char *, struct extra_config **, size_t);
static int enczip_set_module_list (char *, char *, struct modulelist **);

static char * is_mailfolder(char *str);
static char * is_dotdelimiter(char *str);
static char * is_slashdelimiter(char *str);
static struct enczip *md_struct_init(unsigned int, struct enczip_config *,
                                        time_t, struct strset *,
                                        struct strlist *, struct strlist *);
static int md_makesavefilename(struct stat, struct enczip *, char *, int);
static int md_makedirlist(unsigned int, struct enczip *, struct strlist **);
static int md_makedirbylist(unsigned int, struct enczip *, struct strlist *);
static int md_makemaildir_tree(unsigned int, char *, int);
static void md_makemaildir(unsigned int, char *);
static int md_mkdir(unsigned int, char *);
static void md_makesavefile(unsigned int, struct enczip *,
                            char *, struct strlist *);
static void md_list2str(unsigned int, struct strset *, struct strlist *);
static void md_free(struct enczip *);



/* extern struct modulehandle *mhandle_list; */
struct modulehandle *mhandle_list;
char msy_hostname[MAX_HOSTNAME_LEN + 1];

struct cfentry enczip_cfe[] = {
    {
        "EncZipCommand", CF_STRING, "/usr/bin/zip",
        OFFSET(struct enczip_config, cf_enczipcommand), is_executable_file
    },
    {
        "EncZipPassword", CF_STRING, NULL,
        OFFSET(struct enczip_config, cf_enczippassword), is_usable_password
    },
    {
        "EncZipMailDir", CF_STRING, NULL,
        OFFSET(struct enczip_config, cf_enczipmaildir), is_writable_directory
    },
    {
        "EncZipMailFolder", CF_STRING, NULL,
        OFFSET(struct enczip_config, cf_enczipmailfolder), is_mailfolder
    },
    {
        "EncZipDotDelimiter", CF_STRING, ",",
        OFFSET(struct enczip_config, cf_enczipdotdelimiter), is_dotdelimiter
    },
    {
        "EncZipSlashDelimiter", CF_STRING, "_",
        OFFSET(struct enczip_config, cf_enczipslashdelimiter), is_slashdelimiter
    }
};

/*
 * enczip_init
 *
 * ��ǽ:
 *    enczip�⥸�塼��ν�����ؿ�
 *
 * ����:
 *    struct cfentry **cfe      config entry ��¤��
 *    size_t cfesize            config entry ��¤�ΤΥ�����
 *    struct config  **cfg      config ��¤��
 *    size_t cfgsize            config ��¤�ΤΥ�����
 *
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_init(struct cfentry **cfe, size_t *cfesize,
           struct config **cfg, size_t *cfgsize)
{
    struct config *new_cfg;
    struct cfentry *new_cfe;
    size_t new_cfesize, new_cfgsize;
    int ret, i;
    struct modulelist *tmp_list;

    /* �⥸�塼��ꥹ�Ȥؤ��ɲ� */
    ret = enczip_set_module_list(MYMODULE, HEADER_FUNC, &(*cfg)->cf_exec_header);
    if (ret != 0) {
        return -1;
    }
    ret = enczip_set_module_list(MYMODULE, BODY_FUNC, &(*cfg)->cf_exec_body);
    if (ret != 0) {
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    ret = enczip_set_module_list(MYMODULE, EOM_FUNC, &(*cfg)->cf_exec_eom);
    if (ret != 0) {
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* �ܥǥ��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    ret = enczip_set_module_list(MYMODULE, ABORT_FUNC, &(*cfg)->cf_exec_abort);
    if (ret != 0) {
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* �ܥǥ��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eom�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        return -1;
    }

    /* cfg�γ�ĥ */
    new_cfgsize = *cfgsize + sizeof(struct enczip_config);
    new_cfg = (struct config *)realloc(*cfg, new_cfgsize);
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "enczip_set_module_list", strerror(errno));
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* �ܥǥ��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eom�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abort�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        return -1;
    }
    *cfg = new_cfg;

    /* cfe�γ�ĥ */
    new_cfesize = *cfesize + sizeof(enczip_cfe);
    new_cfe = (struct cfentry *)realloc(*cfe, new_cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "enczip_set_module_list", strerror(errno));
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* �ܥǥ��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eom�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abort�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        return -1;
    }

    /* enczip_cfe�Υ��ԡ� */
    memcpy(new_cfe + *cfesize / sizeof(struct cfentry),
           &enczip_cfe, sizeof(enczip_cfe));

    /* dataoffset�ι��� */
    for (i = 0; i < MAILDROP_CFECOUNT; i++) {
        new_cfe[(*cfesize / sizeof(struct cfentry)) + i].cf_dataoffset += *cfgsize;
    }
    *cfe = new_cfe;

    /* �⥸�塼�����config��¤��offset���Ǽ */
    ret = enczip_set_extra_config(MYMODULE, &(*cfg)->cf_extraconfig, *cfgsize);
    if (ret != 0) {
        /* �إå��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_header->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_header;
            (*cfg)->cf_exec_header = (*cfg)->cf_exec_header->mlist_next;
            free(tmp_list);
        }
        /* �ܥǥ��Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_body->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_body;
            (*cfg)->cf_exec_body = (*cfg)->cf_exec_body->mlist_next;
            free(tmp_list);
        }
        /* eom�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_eom->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_eom;
            (*cfg)->cf_exec_eom = (*cfg)->cf_exec_eom->mlist_next;
            free(tmp_list);
        }
        /* abort�Υ��곫��*/
        if (strcmp((*cfg)->cf_exec_abort->mlist_modulename, MYMODULE) == 0) {
            tmp_list = (*cfg)->cf_exec_abort;
            (*cfg)->cf_exec_abort = (*cfg)->cf_exec_abort->mlist_next;
            free(tmp_list);
        }
        /* realloc��������ե����ΰ��Ĥäơ�¾�Υ���������Ѥ���Ǥ���*/

        return -1;
    }

    /* cfesize, cfgsize�ι��� */
    *cfesize = new_cfesize;
    *cfgsize = new_cfgsize;

    return 0;
}

/*
 * enczip_set_module_list
 *
 * ��ǽ:
 *    enczip�⥸�塼���ѤΥ⥸�塼��ꥹ�Ⱥ���
 *
 * ����:
 *    char *modname             �⥸�塼��̾
 *    char *funcname            �ؿ�̾
 *    struct modulelist **list  �⥸�塼��ꥹ��
 *
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_set_module_list(char *modname, char *funcname, struct modulelist **list)
{
    struct modulelist *new_list;

    /* module̾�Υݥ��󥿤��Ǽ�����ΰ�γ��� */
    new_list = (struct modulelist *)malloc(sizeof(struct modulelist));
    if(new_list == NULL) {
        SYSLOGERROR(ERR_MALLOC, "enczip_set_module_list", strerror(errno));
        return -1;
    }

    new_list->mlist_modulename = modname;
    new_list->mlist_funcname = funcname;
    new_list->mlist_next = *list;
    *list = new_list;

    return 0;
}

/*
 * enczip_set_extra_config
 *
 * ��ǽ:
 *    enczip�⥸�塼���Ѥ�extra config�κ���
 *
 * ����:
 *    char *modname                     �⥸�塼��̾
 *    struct extra_config **ext_cfg     extra config �ꥹ��
 *    size_t cfgsize                    config ��¤�ΤΥ�����(extra config �ޤǤ�offset)
 *
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_set_extra_config(char *modname, struct extra_config **ext_cfg,
                        size_t cfgsize)
{
    struct extra_config *new_cfg;

    /* �����⥸�塼���config��¤�Υݥ��󥿤��Ǽ�����ΰ�γ��� */
    new_cfg = (struct extra_config *)malloc(sizeof(struct extra_config));
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "enczip_set_module_list", strerror(errno));
        return -1;
    }

    new_cfg->excf_modulename = modname;

    new_cfg->excf_config = (void *)cfgsize;
    new_cfg->excf_next = *ext_cfg;
    *ext_cfg = new_cfg;

    return 0;
}

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
    char string[] = CHAR_MAILDROP_MAILFOLDER;
    int  i, j;

    /* ʸ�������Ƭ����.�פǤʤ����Ȥγ�ǧ */
    if (str[0] != '.') {
        return ERR_MAILDROP_MAILFOLDER;
    }

    for (i = 0; str[i] != '\0'; i++) {
        /*��.�פ�Ϣ³���Ƥ��ʤ����Ȥγ�ǧ */
        if ((str[i] == '.') && (str[i+1] == '.')) {
            return ERR_MAILDROP_MAILFOLDER;
        }
        /* �᡼��ե������̾���Ȥ���Ŭ�ڤ�ʸ�����Ȥ��Ƥ��뤳�Ȥγ�ǧ */
        for (j = 0; string[j] != '\0'; j++) {
            if (str[i] == string[j]) {
                break;
            }
        }
        /* ʸ�������פ��뤳�Ȥʤ�ȴ������硢���顼 */
        if (string[j] == '\0') {
            return ERR_MAILDROP_MAILFOLDER;
        }
    }
    /* ʸ����κǸ夬���Ǥʤ����Ȥγ�ǧ */
    if (str[i-1] == '.') {
        return ERR_MAILDROP_MAILFOLDER;
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
    char string[] = CHAR_MAILDROP_DOT_DELIMITER;
    int i;

    /* ��ʸ���Ǥ��뤫 */
    if (str[1] != '\0') {
        return ERR_MAILDROP_DOTDELIMITER;
    }

    /* ʸ�������å� */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ���פ��뤳�Ȥʤ�ȴ���Ƥ��ޤä����ϡ���ȿ����ʸ�� */
    if (string[i] == '\0' ) {
        return ERR_MAILDROP_DOTDELIMITER;
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
    char string[] = CHAR_MAILDROP_SLASH_DELIMITER;
    int i;

    /* ��ʸ���Ǥ��뤫 */
    if (str[1] != '\0') {
        return ERR_MAILDROP_SLASHDELIMITER;
    }

    /* ʸ�������å� */
    for (i = 0; string[i] != '\0'; i++ ) {
        if (str[0] == string[i]) {
            break;
        }
    }
    /* ���פ��뤳�Ȥʤ�ȴ���Ƥ��ޤä����ϡ���ȿ����ʸ�� */
    if (string[i] == '\0' ) {
        return ERR_MAILDROP_SLASHDELIMITER;
    }
    return NULL;
}

/*
 * enczip_free_config
 *
 * ��ǽ:
 *    enczip��config�ΰ��free����ؿ�
 * ����:
 *    mP     : priv��¤�Τ�Ĥʤ���¤��
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_free_config(struct config *cfg)
{
    struct enczip_config *p = NULL;
    struct extra_config *exp;

    if (cfg == NULL || cfg->cf_extraconfig == NULL) {
        return -1;
    }

    for (exp = cfg->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
	if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
	    p = (struct enczip_config *)(exp->excf_config);
	    break;
	}
    }

    /* p�����Ĥ��ä����*/
    if (p != NULL) {
        if (p->cf_enczipcommand != NULL) {
            free(p->cf_enczipcommand);
        }
        if (p->cf_enczippassword != NULL) {
            free(p->cf_enczippassword);
        }
        if (p->cf_enczipmaildir != NULL) {
            free(p->cf_enczipmaildir);
        }
        if (p->cf_enczipmailfolder != NULL) {
            free(p->cf_enczipmailfolder);
        }
        if (p->cf_enczipdotdelimiter != NULL) {
            free(p->cf_enczipdotdelimiter);
        }
        if (p->cf_enczipslashdelimiter != NULL) {
            free(p->cf_enczipslashdelimiter);
        }
    } else {
        return -1;
    }

    return 0;
}

/***** ***** ***** ***** *****
 * �����ؿ�
 ***** ***** ***** ***** *****/

/*
 * md_struct_init
 *
 * enczip��¤�Τγ��ݤȽ������Ԥʤ�
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct config *         config��¤�ΤΥݥ���
 *      time_t                  �᡼���������
 *
 * �֤���
 *      struct enczip *       enczip��¤��
 */
static struct enczip *
md_struct_init(unsigned int s_id, struct enczip_config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct enczip *md;
    int ret;

    /* �ΰ����� */
    md = (struct enczip *)malloc(sizeof(struct enczip));
    if (md == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "md_struct_init", E_STR);
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct enczip));

    /* ������������¸*/
    md->md_recvtime = time;

    /* MailDir, MailFolder���ͤ���¸*/
    strset_set(&md->md_maildir, config->cf_enczipmaildir);
    strset_set(&md->md_mailfolder, config->cf_enczipmailfolder);

    /* dotdelmiter, slashdelimiter������¸*/
    md->md_dotdelimiter = *(config->cf_enczipdotdelimiter);
    md->md_slashdelimiter = *(config->cf_enczipslashdelimiter);

    /* ��������إå������*/
    /* FROM�إå��ν����*/
    strset_init(&md->md_header_from);
    /* �ͳ�Ǽ*/
    ret = strset_catstrset(&md->md_header_from, from);
    if (ret == -1) {
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
        exit(EXIT_MILTER);
    }
    /* TO�إå��ν����*/
    strset_init(&md->md_header_to);
    /* �ͳ�Ǽ*/
    md_list2str(s_id, &md->md_header_to, to_h);

    /* ��¸���ɥ쥹�����γ�Ǽ*/
    md->md_saveaddr_h = saveaddr_h;

    return md;
}


/*
 * enczip_get_priv
 *
 * ��ǽ:
 *    extrapriv�ΰ褬�ʤ���к�������
 *    ����м�ʬ�Ѥ��ΰ��extrapriv�ΰ�ݥ��󥿤��֤��ؿ�
 * ����:
 *    priv: mlfiPriv��¤�ΤΥݥ���(�����Ϥ�)
 * ����:
 *   ��ʬ�Ѥ�extrapriv��¤�ΤΥݥ���
 */
struct extrapriv *
enczip_get_priv(struct mlfiPriv **priv)
{
    struct extrapriv *p = NULL;      /* ������ */
    struct extrapriv *mp = NULL;     /* ���������� */
    struct extrapriv *p_old = NULL;  /* �����桢�ҤȤ����Υݥ�����¸�� */

    if (*priv != NULL) {
        /* ��ʬ��priv��¤�Τ����뤫���� */
        for (p = (*priv)->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (strcmp(MYMODULE, p->expv_modulename) == 0) {
                /* ���ä���꥿���� */
                return p;
            }
            /* �ҤȤ����Υݥ��󥿳�Ǽ */
            p_old = p;
        }
    }
    /* ��ʬ�Ѥ�extrapriv�ΰ迷������ */
    mp = malloc(sizeof(struct extrapriv));
    if (mp == NULL) {
        SYSLOGERROR(ERR_MALLOC, "enczip_get_priv", E_STR);
        return NULL;
    }
    /* �ͤγ�Ǽ */
    /* MYMODULE���ͤ򤽤Τޤ޻��ͤ���Τǡ���������Ȥ��Ϥ��ʤ��Ǥ�������*/
    mp->expv_modulename = MYMODULE;
    /* NEXT�˽����*/
    mp->expv_next = NULL;
    /* �ץ饤�١��Ȥν����*/
    mp->expv_modulepriv = NULL;

    /* ����¸�ߤ��Ƥ��ʤ��ä�����Ƭ�˥ݥ��󥿤��դ��� */
    if (p_old == NULL) {
        (*priv)->mlfi_extrapriv = mp;

    /* ¸�ߤ��Ƥ��뤬����ʬ�Ѥ��ʤ��ä�����ˤĤ��� */
    } else if (p == NULL) {
        p_old->expv_next = mp;
    }
    return mp;
}

/*
 * enczip_priv_free
 *
 * ��ǽ:
 *    ���٤Ƥ�priv��¤�Τ�free����ؿ�
 * ����:
 *     extrapriv:   �����ι�¤�ΤΥݥ���(�����Ϥ�)
 * ����:
 *    ̵��
 */
void
enczip_priv_free(struct extrapriv *expv)
{
    /* NULL�����å� */
    if (expv != NULL) {
        /* enczip_priv�ΰ褬������ */
        if (expv->expv_modulepriv != NULL) {
            /* enczip_priv��¤�Τ�free */
            /* private �ѿ�����*/
            free(expv->expv_modulepriv);
            expv->expv_modulepriv = NULL;
        }
        /* extrapriv�ΰ��free */
        free(expv);
        expv = NULL;
    }

    return;
}


/*
 * enczip_abort
 *
 * �᡼����¸��������ߤ���
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct enczip *       enczip��¤��
 *
 * �֤���
 *      ̵��
 */
void
enczip_abort(unsigned int s_id, struct enczip *md)
{
    /* �ץ饤�١��Ⱦ���NULL�ξ��*/
    if (md == NULL) {
        return;
    }

    if (md->md_tempfile_fd > 0) {
        /* �ե����뤬�����ץ󤵤�Ƥ�����ϥ��������� */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }
    unlink(md->md_tempfilepath);
    return;
}

/*
 * enczip_exec_header
 *
 * ��ǽ:
 *    mlfi_header�ǸƤФ��ؿ�
 *    priv�ΰ�γ��ݡ��إå�����Υ����Ǽ����ؿ�
 * ����:
 *    priv   : priv��¤�Τ�Ĥʤ���¤��
 *    headerf: �إå��ι���̾
 *    headerv: �إå��ι��ܤ��Ф�����
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_exec_header(struct mlfiPriv *priv, char *headerf, char *headerv)
{
    /* �ѿ����*/
    struct extrapriv     *expv;
    struct extra_config  *p;
    struct enczip_priv *mypv;
    struct enczip      *mydat;
    struct enczip      *mydatp;
    int                  ret;
    unsigned int         s_id;

    /* �����*/
    expv = NULL;
    p = NULL;
    mypv = NULL;
    mydat = NULL;
    mydatp = NULL;
    ret = 0;
    s_id = priv->mlfi_sid;


    /* extrapriv�ΰ��̵ͭ */
    expv = enczip_get_priv(&priv);

    /* enczip_get_priv�����顼�λ� */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_header", "enczip_get_priv");
        return -1;
    }

    /* enczip_priv�ΰ褬�ʤ��ä������ */
    if (expv->expv_modulepriv == NULL) {
        /* enczip�ΰ� */
        mypv = malloc(sizeof(struct enczip_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "enczip_exec_header", E_STR);
            return -1;
        }

        /* 2�Ĥ�Ĥʤ��� */
        expv->expv_modulepriv = mypv;

        /* ��ʬ��config��¤�θ��� */ 
        if (priv->config->cf_extraconfig != NULL) { 
            for (p = priv->config->cf_extraconfig; p != NULL; p = p->excf_next) {
                if (!strcmp(MYMODULE, p->excf_modulename)) {
                    break;
                }
            }
        }

        
        /* �����ץ�  �᡼����¸�����򳫻� */
        mydat = enczip_open(s_id, ((struct enczip_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR(ERR_S_FOPEN, s_id, "enczip_exec_header", "enczip_open");
            free(mypv);
            return -1;
        }
        
        /* ��¤�Τ�Ĥʤ��� */
        mypv->mypriv = mydat;
    }

    /* enczip��¤�ΤΥݥ��󥿤��ѿ��˳�Ǽ */
    mydatp = ((struct enczip_priv *)expv->expv_modulepriv)->mypriv;

    /* �إå���񤭹���*/
    ret = enczip_write_header(s_id, mydatp, headerf, headerv);
    if (ret != R_SUCCESS) {
        SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_header", "enczip_write_header");
        return -1;
    }

    return 0;
}

/*
 * enczip_exec_body
 *
 * ��ǽ:
 *    mlfi_body�ǸƤФ��ؿ�
 *    priv�ΰ�γ��ݡ��إå�����Υ����Ǽ����ؿ�
 * ����:
 *    *priv  : priv��¤�Τ�Ĥʤ���¤��(�����Ϥ�)
 *    *bodyp : mlfi_body�����������ܥǥ���
 *    bodylen: bodyp�Υ�����
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_exec_body(struct mlfiPriv *priv, u_char *bodyp, size_t bodylen)
{
    /* �ѿ����*/
    struct extrapriv     *expv;
    struct enczip      *mydat;
    struct enczip_priv *mypv;
    struct enczip      *mydatp;
    int                   ret;
    unsigned int          s_id;
    struct extra_config  *p;

    /* �����*/
    expv = NULL;
    p = NULL;
    mypv = NULL;
    mydat = NULL;
    mydatp = NULL;
    ret = 0;
    s_id = priv->mlfi_sid;


    /* extrapriv�ΰ��̵ͭ */
    expv = enczip_get_priv(&priv);
    /* enczip_get_priv�����顼�λ� */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_body", "enczip_get_priv");
        return -1;
    }

    /* enczip_priv�ΰ褬�ʤ��ä������ */
    if (expv->expv_modulepriv == NULL) {
        /* enczip�ΰ� */
        mypv = malloc(sizeof(struct enczip_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "enczip_exec_body", E_STR);
             return -1;
        }

        /* 2�Ĥ�Ĥʤ��� */
        expv->expv_modulepriv = mypv;

        /* ��ʬ��config��¤�θ��� */ 
        if (priv->config->cf_extraconfig != NULL) { 
            for (p = priv->config->cf_extraconfig; p != NULL; p = p->excf_next) {
                if (!strcmp(MYMODULE, p->excf_modulename)) {
                    break;
                }
            }
        }

       
        /* �����ץ�  �᡼����¸�����򳫻� */
        mydat = enczip_open(s_id, ((struct enczip_config *)p->excf_config),
                           priv->mlfi_recvtime, &(priv->mlfi_envfrom),
                           priv->mlfi_rcptto_h, priv->mlfi_addrmatched_h,
                           priv->config->cf_msyhostname);
        if (mydat == NULL) {
            SYSLOGERROR(ERR_S_FOPEN, s_id, "enczip_exec_header", "enczip_open");
            free(mypv);
            return -1;
        }
        
        /* ��¤�Τ�Ĥʤ��� */
        mypv->mypriv = mydat;
    }

    /* �᡼��ǡ��������*/
    mydat = ((struct enczip_priv *)expv->expv_modulepriv)->mypriv;

    /* �ܥǥ��񤭹��� */
    ret = enczip_write_body(s_id, mydat, bodyp, bodylen);
    if (ret != R_SUCCESS) {
        SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_body", "enczip_write_body");
        return -1;
    }

    return 0;
}

/*
 * enczip_exec_eom
 *
 * ��ǽ:
 *    mlfi_eom�ǸƤФ��ؿ�
 *    mlfi_header�ǳ�Ǽ�����إå������¤�Τ˳�Ǽ����
 *    DB����Ͽ����ؿ�
 * ����:
 *    priv: priv��¤�Τ�Ĥʤ���¤��(�����Ϥ�)
 * ����:
 *     0: ����
 *    -1: �۾�
 */
int
enczip_exec_eom(struct mlfiPriv *priv)
{
    /* �ѿ����*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct enczip     *mydat;
    int                  ret;
    unsigned int         s_id;

    /* �����*/
    p = NULL;
    p_old = NULL;
    mydat= NULL;
    ret = 0;
    s_id = priv->mlfi_sid;

    /* ��ʬ���ΰ�̵ͭ�����å�*/
    if (priv != NULL) {
        /* ��ʬ��priv��¤�Τ����뤫����*/
        for (p = priv->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (!strcmp(MYMODULE, p->expv_modulename)) {
                break;
            }
            p_old = p;
        }
 
        /* ��������ΰ褬extrapriv��¤��*/
        if (p_old != NULL) {
            if (p != NULL) {
                mydat = ((struct enczip_priv *)p->expv_modulepriv)->mypriv;
                /* ������*/
                ret = enczip_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_eom", "enczip_close");
                    return -1;
                }
                /* ������ι�¤�Τ�next��free���빽¤�Τ�next��Ĥʤ���*/
                p_old->expv_next = p->expv_next;
                /* �ץ饤�١��ȥǡ�����������*/
                enczip_priv_free(p);

            }
        /* P����Ƭ�ξ��*/
        } else {
            if (p != NULL) {
                mydat = ((struct enczip_priv *)p->expv_modulepriv)->mypriv;
                /* ������*/
                ret = enczip_close(s_id, mydat, priv->config);
                if (ret != R_SUCCESS) {
                    SYSLOGERROR(ERR_EXEC_FUNC, "enczip_exec_eom", "enczip_close");
                    return -1;
                }
                /* �������mlfi��¤�Τ�free���빽¤�Τ�next��Ĥʤ���*/
                priv->mlfi_extrapriv = p->expv_next;
                /* �ץ饤�١��Ⱦ�����*/
                enczip_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * enczip_exec_abort
 *
 * ��ǽ:
 *    mlfi_abort��exec_eom�ǸƤФ��ؿ�
 *    priv��¤�Τ�����free����ؿ�
 *
 * ����:
 *    priv: priv��¤�Τ�Ĥʤ���¤��
 *
 * ����:
 *    0(R_SUCCESS): ����
 */
int
enczip_exec_abort(struct mlfiPriv *priv)
{
    /* �ѿ����*/
    struct extrapriv    *p;
    struct extrapriv    *p_old;
    struct enczip     *md;
    unsigned int         s_id;

    /* �����*/
    p = NULL;
    p_old = NULL;
    md = NULL;
    s_id = priv->mlfi_sid;

    /* ��ʬ���ΰ�̵ͭ�����å� */
    if (priv != NULL) {
        /* ��ʬ��priv��¤�Τ����뤫���� */
        for (p = priv->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (!strcmp(MYMODULE, p->expv_modulename)) {
                break;
            }
            p_old = p;
        }
        /* ��������ΰ褬extrapriv��¤�� */
        if (p_old != NULL) {
            if (p != NULL) {
                md = ((struct enczip_priv *)p->expv_modulepriv)->mypriv;
                /* ���ܡ��� */
                enczip_abort(s_id, md);
                /* �ҤȤ����ι�¤�Τ�next��free���빽¤�Τ�next��Ĥʤ��� */
                p_old->expv_next = p->expv_next;
                enczip_priv_free(p);
            }

        /* P����Ƭ�ξ��*/
        } else {
            if (p != NULL) {
                md = ((struct enczip_priv *)p->expv_modulepriv)->mypriv;
                /* ���ܡ��� */
                enczip_abort(s_id, md);
                /* �ҤȤ����ι�¤�Τ�next��free���빽¤�Τ�next��Ĥʤ��� */
                priv->mlfi_extrapriv = p->expv_next;
                enczip_priv_free(p);
            }
        }
    }
    return 0;
}

/*
 * enczip_open
 *
 * �ե�����񤭹��ߤν�����Ԥʤ�
 * - ɬ�פʥǥ��쥯�ȥ�κ���
 * - ����ե�����Υ����ץ�
 * - ��������إå����ͺ���
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct config *         config��¤��
 *      time_t                  ��������
 *      struct strset *         From
 *      struct strlist *        To��������Ƭ�ݥ���
 *      struct strlist *        ��¸���ɥ쥹��������Ƭ�ݥ���
 *
 * �֤���
 *      struct enczip *       ����
 *      NULL                    ���顼 (����ե�����Υ����ץ�˼���)
 */
struct enczip *
enczip_open(unsigned int s_id, struct enczip_config *config,
                time_t time, struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h, char *msy_hostname)
{
    struct enczip *md;
    mode_t old_umask;
    int temppathlen;

    /* enczip��¤�Τ����� */
    md = md_struct_init(s_id, config, time, from, to_h, saveaddr_h);

    /* MailDir�۲��˥��֥ǥ��쥯�ȥ��������� */
    md_makemaildir(s_id, config->cf_enczipmaildir);

    /* ����ե�����Υѥ������ */
    temppathlen = strlen(config->cf_enczipmaildir) +
                    strlen(msy_hostname) + TEMPFILEPATH_LEN;
    md->md_tempfilepath = (char *)malloc(temppathlen);
    if (md->md_tempfilepath == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "enczip_open", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(md->md_tempfilepath, TEMPFILEPATH,
            config->cf_enczipmaildir, md->md_recvtime, msy_hostname);

    /* ����ե�����򥪡��ץ� */
    old_umask = umask(0077);
    md->md_tempfile_fd = mkstemp(md->md_tempfilepath);
    umask(old_umask);
    if (md->md_tempfile_fd < 0) {
        SYSLOGERROR(ERR_S_MKSTEMP, s_id, md->md_tempfilepath, E_STR);
        return NULL;
    }
    return md;
}

/*
 * enczip_write_header
 *
 * �إå������ե�����˽��Ϥ���
 * ��������إå�����˽񤭹���
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct enczip *       enczip��¤��
 *      char *                  �إå��ե������ (������Хå����Ϥ��줿�ޤ�)
 *      char *                  �إå��� (������Хå����Ϥ��줿�ޤ�)
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
enczip_write_header(unsigned int s_id, struct enczip *md,
                        char *headerf, char *headerv)
{
    char *header, *p;
    int header_len;
    ssize_t written_len;
    int ret;

    if (!md->md_writing_header) {
        /* �Ϥ���˥�������إå���񤭹��� */
        md->md_writing_header = 1;
        ret = enczip_write_header(s_id, md, CUSTOMHDR_FROM,
                                    md->md_header_from.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
        ret = enczip_write_header(s_id, md, CUSTOMHDR_TO,
                                    md->md_header_to.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* �إå��ν񤭹��� */
    header_len = strlen(headerf) + ((headerv == NULL)?0:strlen(headerv)) + 3; /* ʸ���� + ': ' + '\n' */
    header = (char *)malloc(header_len + 1);    /* '\0' */
    if (header == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "enczip_write_header", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(header, "%s: %s\n", headerf, (headerv == NULL)?"":headerv);

    p = header;
    written_len = 0;
    while (written_len < header_len) {
        written_len = write(md->md_tempfile_fd, p, header_len);
        if (written_len < 0) {
            SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
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
 * enczip_write_body
 *
 * �᡼��ܥǥ������ե�����˽��Ϥ���
 * �إå��ȥܥǥ��ζ��ڤ����˽񤭹���
 * ����ʸ����CRLF��LF�����줹��
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct enczip *       enczip��¤��
 *      unsigned char *         �ܥǥ� (������Хå����Ϥ��줿�ޤ�)
 *      size_t                  Ĺ�� (������Хå����Ϥ��줿�ޤ�)
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
enczip_write_body(unsigned int s_id, struct enczip *md,
                    unsigned char *bodyp, size_t len)
{
    ssize_t written_len;
    int ret;
    int i;

    if (!md->md_writing_body) {
        /* �Ϥ���˥إå��ȥܥǥ��ζ��ڤ�ʸ����񤭹��� */
        md->md_writing_body = 1;
        ret = enczip_write_body(s_id, md, (unsigned char *) "\n", 1);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* ����ʸ����LF�����줷�ʤ��顢��ʸ��񤭹��� */
    for (i = 0; i < len; i++, bodyp++) {
        if (md->md_cr) {
            if (*bodyp != '\n') {
                written_len = write(md->md_tempfile_fd, "\r", 1);
                if (written_len < 0) {
                    SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
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
            SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            return R_ERROR;
        }
    }

    return R_SUCCESS;
}

/*
 * enczip_close
 *
 * ����ե�����򥯥���������¸����
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct enczip *         enczip��¤��
 *      struct config *         config��¤��
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
enczip_close(unsigned int s_id, struct enczip *md, struct config * cfg)
{
    struct strlist *list_h;
    /* ENCZIP_SUFFIX_LEN��.zip��Ĺ��*/
    char filename[NAME_MAX + 6 + ENCZIP_SUFFIX_LEN];
    size_t ret_s;
    int ret;

    /* �Ź沽���Ѥ����ѿ�*/
    struct stat st;
    pid_t pid, wpid;
    char *tempstr;
    char *envstr;
    int tempstr_len;
    int status;
    int len;

    struct enczip_config *enczcf;
    struct extra_config *exp;
    char  *command_real;
    char **command_args;

    /* �����*/
    tempstr_len = 0;
    tempstr = NULL;
    envstr = NULL;
    enczcf = NULL;
    exp = NULL;
    command_real = NULL;
    command_args = NULL;
    ret = 0;

    /* ����ե�����򥯥��� */
    if (md->md_tempfile_fd > 0) {
        /* ����ʸ�������Ǥʤ�CR���ĤäƤ�����Ͻ񤭹��� */
        if (md->md_cr) {
            ret_s = write(md->md_tempfile_fd, "\r", 1);
            if (ret_s < 0) {
                SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            }
            md->md_cr = 0;
        }
        /* ������ */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }

    /* extraconfig¸�ߥ����å�*/
    if (cfg == NULL || cfg->cf_extraconfig == NULL) {
        SYSLOGERROR(ERR_ENCZIP_CONF, s_id);
        return R_ERROR;
    }

    /* extraconfig�򸡺�*/
    for (exp = cfg->cf_extraconfig; exp != NULL; exp = exp->excf_next) {
        if (strcmp(MYMODULE, exp->excf_modulename) == 0) {
            enczcf = (struct enczip_config *)(exp->excf_config);
            break;
        }
    }
    /* �ߤĤ���̵���ä��顢���顼*/
    if (enczcf == NULL) {
        SYSLOGERROR(ERR_ENCZIP_CONF, s_id);
        return R_ERROR;
    }

    /* �ե�����¸�ߥ����å�*/
    if (stat(md->md_tempfilepath, &st) != 0) {
        SYSLOGERROR(ERR_TEMPFILE_EXIST, s_id, md->md_tempfilepath, E_STR);
        unlink(md->md_tempfilepath);
        return R_ERROR;
    }

    /* ziptempfilepath�ΰ����*/
    tempstr_len = (int) (strlen(enczcf->cf_enczipmaildir) +
                     strlen(msy_hostname) + ENCZIP_TEMPFILEPATH_LEN);
    tempstr = malloc(tempstr_len);
    if (tempstr == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "enczip_close", E_STR);
        unlink(md->md_tempfilepath);
        return R_ERROR;
    }
    sprintf(tempstr, ENCZIP_TEMPFILEPATH, md->md_tempfilepath);

    /* child process duplicate*/
    pid = fork();
    /* ���顼����*/
    if (pid < 0) {
        SYSLOGERROR(ERR_S_FORK, enczcf->cf_enczipcommand);
        free(tempstr);
        return R_ERROR;

    /* �ҥץ����ν���*/
    } else if (pid == 0) {
        /* convert command*/
        command_args = cmd_strrep(enczcf->cf_enczipcommand, ' ', &command_real,
                                  EXTEND_PART_OPTION_NUM_ENCZIP);
        if (command_args == NULL) {
            SYSLOGWARNING(ERR_MEMORY_ALLOC);
            exit(1);
        }

        /* args�Τ�Ĺ����׻�*/
        for (len = 0; command_args[len] != NULL; len++){
        }
             
        /* ���ޥ�ɸ��Υե�����̾�Ȱ��̤����ե�����̾������*/
        command_args[len] = tempstr;
        command_args[len + 1] = md->md_tempfilepath;

        /* ���ޥ�ɤΥ��ץ��������*/
        envstr = malloc(sizeof(ENCZIP_FIXOPTION) + strlen(enczcf->cf_enczippassword) + 2);
        if (envstr == NULL) {
            SYSLOGERROR(ERR_S_MALLOC, s_id, "enczip_close", E_STR);
            exit(1);
        }
        sprintf(envstr, "%s %s", ENCZIP_FIXOPTION, enczcf->cf_enczippassword);

        /* ���ץ����δĶ��ѿ�����*/
        if (setenv(ENCZIP_ENV_NAME, envstr, OVERWRITE) < 0) {
            SYSLOGERROR(ERR_SET_ENV, "enczip_close", E_STR);
            exit(1);
        }

        /* ���ץ������ΰ賫��*/
        free(envstr);

        /* ���ץ����Υ��ޥ�ɼ¹�*/
        if (execv(command_args[0], command_args) < 0) {
            SYSLOGERROR(ERR_EXEC_COMMAND, s_id, enczcf->cf_enczipcommand, E_STR);
            exit(1);
        }

        /* may not reach*/
        /* �Ķ��ѿ����ΰ賫��*/
        unsetenv(ENCZIP_ENV_NAME);
        exit(1);

    /* �ƥץ����ν���*/
    } else {
        /* �ҥץ����Ԥ�*/
        wpid = waitpid(pid, &status, WUNTRACED);
        if (wpid < 0) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, enczcf->cf_enczipcommand, status);
            /* ����ե������� */
            unlink(md->md_tempfilepath);
            free(tempstr);
            return R_ERROR;
        }
        /* �ҥץ����Υ��ơ�������ǧ*/
        if (!WIFEXITED(status)) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, enczcf->cf_enczipcommand, status);
            /* ����ե������� */
            unlink(md->md_tempfilepath);
            free(tempstr);
            return R_ERROR;
        }
        /* �ҥץ���������˽�λ����ʤ����*/
        if (WEXITSTATUS(status)) {
            SYSLOGERROR(ERR_EXEC_STATUS, s_id, enczcf->cf_enczipcommand, status);
            /* ����ե������� */
            unlink(md->md_tempfilepath);
            free(tempstr);
            return R_ERROR;
        }
    }

    /* ���̤����ե�����¸�ߥ����å�*/
    ret = stat(tempstr, &st);
    if (ret < 0) {
        SYSLOGERROR(ERR_TEMPFILE_EXIST, s_id, tempstr, E_STR);
        /* ����tempfile���*/
        if (stat(md->md_tempfilepath, &st) == 0) {
            /* ����ե������� */
            unlink(md->md_tempfilepath);
        }
        free(tempstr);
        /* ���顼���֤�*/
        return R_ERROR;
    }

    /* ��¸��tempfilepath���ΰ賫��*/
    free(md->md_tempfilepath);

    /* ziptempfilepath��¸*/
    md->md_tempfilepath = tempstr;

    /* ��¸��Υե�����̾����� */
    ret = md_makesavefilename(st, md, filename, sizeof(filename));
    if (ret != R_SUCCESS) {
        /* ����ե������� */
        unlink(md->md_tempfilepath);
        return R_ERROR;
    }

    /* ɬ�פʥǥ��쥯�ȥ��������� */
    ret = md_makedirlist(s_id, md, &list_h);
    if (ret != R_SUCCESS) {
        /* ����ե������� */
        unlink(md->md_tempfilepath);
        return R_ERROR;
    }

    /* ɬ�פʥǥ��쥯�ȥ����� */
    ret = md_makedirbylist(s_id, md, list_h);
    if (ret != R_SUCCESS) {
        free_strlist(list_h);
        /* ����ե������� */
        unlink(md->md_tempfilepath);
        return R_ERROR;
    }

    /* �ե����륳�ԡ� */
    md_makesavefile(s_id, md, filename, list_h);

    /* ����ե������� */
    unlink(md->md_tempfilepath);

    free_strlist(list_h);

    md_free(md);
    md = NULL;

    return R_SUCCESS;
}

/***** ***** ***** ***** *****
 * �����ؿ�
 ***** ***** ***** ***** *****/

/*
 * md_makedirlist
 *
 * ��������ɬ�פΤ���ǥ��쥯�ȥ�������������
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct enczip *       enczip��¤��
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
static int
md_makedirlist(unsigned int s_id, struct enczip *md, struct strlist **list_h)
{
    struct strlist *list_t, *p;
    char mailaddr[MAX_ADDRESS_LEN + 1];
    struct strformat sf[6];
    char year[5], month[3], day[3];
    char *addr_p, *domain_p, *tmp;
    struct strset path;
    struct tm lt, *ret_t;
    int ret;

    /* �������狼���ִ�ʸ�������� */
    ret_t = localtime_r(&md->md_recvtime, &lt);
    if (ret_t == NULL) {
        SYSLOGERROR(ERR_S_LTIME, s_id, E_STR);
        return R_ERROR;
    }
    strftime(year,  5, "%Y", &lt);
    strftime(month, 3, "%m", &lt);
    strftime(day,   3, "%d", &lt);

    sf[0].sf_formatchar = 'y';
    sf[0].sf_replacestr = year;
    sf[1].sf_formatchar = 'm';
    sf[1].sf_replacestr = month;
    sf[2].sf_formatchar = 'd';
    sf[2].sf_replacestr = day;

    /* ��¸�оݥ��ɥ쥹��˥ǥ��쥯�ȥ�̾��������� */
    *list_h = list_t = NULL;
    p = md->md_saveaddr_h;
    while (p != NULL) {
        /* ���ɥ쥹�ȥɥᥤ�󤫤��ִ�ʸ������������ */
        strncpy(mailaddr, p->ss_data.ss_str, MAX_ADDRESS_LEN + 1);
        ret = check_7bit(mailaddr);
        if (ret != 0) {
            /* 8bitʸ�����ޤޤ�뤿��UNKNOWN�� */
            addr_p = UNKNOWN;
            domain_p = UNKNOWN;
        } else {
            replace_delimiter(mailaddr, DOT, md->md_dotdelimiter);
            replace_delimiter(mailaddr, SLASH, md->md_slashdelimiter);

            domain_p = strchr(mailaddr, '@');
            if (domain_p == NULL) {
                /* ���ɥ쥹�����κ������˥ɥᥤ���䴰�����Τǡ�
                 * �����ˤ�����ʤ��Ϥ� */
                domain_p = UNKNOWN;
            } else {
                domain_p++;
            }
            addr_p = mailaddr;
        }

        sf[3].sf_formatchar = 'D';
        sf[3].sf_replacestr = domain_p;
        sf[4].sf_formatchar = 'M';
        sf[5].sf_formatchar = 'f';
        sf[4].sf_replacestr = sf[5].sf_replacestr = addr_p;

        /* MailFolder�Υե����ޥå�ʸ�����ִ����� */
        tmp = str_replace_format(md->md_mailfolder.ss_str, sf, 6);

        /* MailDir, MailFolder (�ִ���) ��Ϣ�뤹�� */
        strset_init(&path);
        if (strset_catstrset(&path, &md->md_maildir) == -1 ||
            strset_catstr(&path, "/") == -1 ||
            strset_catstr(&path, tmp) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
        free(tmp);

        /* �ǥ��쥯�ȥ�������ɲä���
         * �ޤä���Ʊ��Υѥ������˰����ˤ������̵�뤹�� */
        uniq_push_strlist(list_h, &list_t, path.ss_str);


        strset_free(&path);
        p = p->next;
    }

    return R_SUCCESS;
}

/*
 * md_makedirbylist
 *
 * �ǥ��쥯�ȥ�����򸵤ˡ�Maildir�����Υǥ��쥯�ȥ���������
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct enczip *       enczip��¤��
 *      struct strlist *        �ǥ��쥯�ȥ��������Ƭ�ݥ���
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
static int
md_makedirbylist(unsigned int s_id, struct enczip *md, struct strlist *list)
{
    struct strlist *p;
    int ret;

    p = list;
    while (p != NULL) {
        ret = md_makemaildir_tree(s_id, p->ss_data.ss_str,
                                    md->md_maildir.ss_len);

        if (ret != R_SUCCESS) {
            return R_ERROR;
        }

        p = p->next;
    }

    return R_SUCCESS;
}

/*
 * md_makemaildir_tree
 *
 * ���ꤵ�줿�ǥ��쥯�ȥ�˻��ǥ��쥯�ȥ�ĥ꡼��Maildir�����Ǻ�������
 *
 * /home/archive/Maildir/.2009.10.01
 * �� /home/archive/Maildir/.2009/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10.01/{new,cur,tmp}
 *
 * ����
 *      unsigned int            ���å����ID
 *      char *                  �ǥ��쥯�ȥ�̾
 *                              (�Ǥ⿼���ǥ��쥯�ȥ����ꤹ��)
 *      int                     �١����ǥ��쥯�ȥ��Ĺ��
 *                              (Maildir�����Υĥ꡼�ε�������ꤹ��)
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼 (�������顼)
 */
static int
md_makemaildir_tree(unsigned int s_id, char *targetdir, int basedir_len)
{
    char *subtop, *dot;

    /* �ݥ��󥿤�ե������Ƭ�ΥɥåȤ˰�ư������
     * /path/to/basedir/.folder
     *                  ^-subtop */
    subtop = targetdir + basedir_len + 1;
    if (strchr(subtop, SLASH) != NULL) {
        /* ����ե�����۲��˥���å��夬�ޤޤ�Ƥ�������
         * �б����Ƥ��ʤ��Τǥ��顼���֤� */
        return R_ERROR;
    }

    /* ���֥ǥ��쥯�ȥ��������� */
    while ((dot = strchr(subtop, DOT)) != NULL) {
        /* �ɥåȤ�\0�˰��Ū���֤������ƥǥ��쥯�ȥ����� */
        *dot = '\0';
        md_makemaildir(s_id, targetdir);
        *dot = DOT;
        subtop = dot + 1;
    }

    /* �ǽ�Ū�ʥǥ��쥯�ȥ����� */
    md_makemaildir(s_id, targetdir);

    return R_SUCCESS;
}

/*
 * md_makemaildir
 *
 * ���ꤵ�줿�ǥ��쥯�ȥ��������������۲���
 *   /new, /cur, /tmp
 * ��3�ĤΥǥ��쥯�ȥ���������
 * ���ǥ��쥯�ȥ�κ����˼��Ԥ������⥨�顼�Ȥ��ʤ�
 *
 * ����
 *      unsigned int            ���å����ID
 *      char *                  �ǥ��쥯�ȥ�̾
 *
 * �֤���
 *      �ʤ�
 */
static void
md_makemaildir(unsigned int s_id, char *dirname)
{
    /* �������륵�֥ǥ��쥯�ȥ���� */
    char *subdirs[] = {
                       "/new",
                       "/cur",
                       "/tmp",
                       NULL
                      };

    struct strset createpath;
    char *tmp;
    int ret, i;

    /* �١����ǥ��쥯�ȥ����� */
    md_mkdir(s_id, dirname);

    strset_init(&createpath);
    for (i = 0; subdirs[i] != NULL; i++) {
        /* MailDir�򥳥ԡ� */
        tmp = strdup(dirname);
        if (tmp == NULL) {
            SYSLOGERROR(ERR_S_MALLOC, s_id, "md_makemaildir", E_STR);
            exit(EXIT_MILTER);
        }
        strset_set(&createpath, tmp);

        /* ���֥ǥ��쥯�ȥ�̾ (.../new, .../cur, .../tmp) ���ղ� */
        ret = strset_catstr(&createpath, subdirs[i]);
        if (ret == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
         
        /* ���֥ǥ��쥯�ȥ����� */
        md_mkdir(s_id, createpath.ss_str);

        strset_free(&createpath);
    }

    return;
}

/*
 * md_mkdir
 *
 * ���ꤵ�줿�ǥ��쥯�ȥ���������
 *
 * ����
 *      unsigned int            ���å����ID
 *      char *                  �ǥ��쥯�ȥ�̾
 *
 * �֤���
 *      R_SUCCESS               ���� (���˥ǥ��쥯�ȥ꤬¸�ߤ�������)
 *      R_ERROR                 ���顼
 */
static int
md_mkdir(unsigned int s_id, char *dirname)
{
    struct stat stbuf;

    if (stat(dirname, &stbuf) < 0) {
        if (errno != ENOENT) {
            SYSLOGERROR(ERR_S_STAT, s_id, dirname, E_STR);
            return R_ERROR;
        }
        if (mkdir(dirname, 0700) < 0) {
            SYSLOGERROR(ERR_S_MKDIR, s_id, dirname, E_STR);
            return R_ERROR;
        }

        /* �������� */
        return R_SUCCESS;

    } else {
        if (!S_ISDIR(stbuf.st_mode)) {
            SYSLOGERROR(ERR_S_NDIR, s_id, dirname, E_STR);
            return R_ERROR;
        }

        /* ����¸�ߤ��� */
        return R_SUCCESS;
    }

    /* ǰ�Τ��� */
    return R_SUCCESS;
}

/*
 * md_makesavefilename
 *
 * ��¸�ե�����̾ ("/new/.....") ���������
 *
 * ����
 *      stat stbuf              file information
 *      struct enczip *         enczip��¤��
 *      char *                  �ե�����̾�γ�Ǽ��
 *      int                     ��Ǽ���Ĺ��
 *      config *                �����ѿ�
 *
 * �֤���
 *      R_SUCCESS               ����
 */
static int
md_makesavefilename(struct stat stbuf, struct enczip *md,
                    char *filename, int filename_len)
{
    /* �ե�����Υѥ� (/new/....) ��������� */
    snprintf(filename, filename_len, ENCZIPSAVEFILENAME,
             md->md_recvtime, stbuf.st_ino, msy_hostname);
    return R_SUCCESS;
}

/*
 * md_makesavefile
 *
 * �����˴ޤޤ��ǥ��쥯�ȥ��۲��ˡ���¸�ե�������󥯤���
 * ����󥯤˼��Ԥ������ϥ��顼�Ȥ��ʤ�
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct enczip *       enczip��¤��
 *      char *                  ��¸�ե�����̾
 *      struct strlist *        �ǥ��쥯�ȥ����
 *
 * �֤���
 *      �ʤ� (��󥯤˼��Ԥ�������)
 */
static void
md_makesavefile(unsigned int s_id, struct enczip *md,
                            char *filename, struct strlist *dirlist)
{
    struct strlist *p;
    struct strset path;
    int ret;

    p = dirlist;
    while (p != NULL) {
        /* �����Υե�����Υե�ѥ���������� */
        strset_init(&path);
        if (strset_catstr(&path, p->ss_data.ss_str) == -1 ||
            strset_catstr(&path, filename) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }

        /* �ϡ��ɥ�󥯤�������� */
        ret = link(md->md_tempfilepath, path.ss_str);
        if (ret < 0) {
            /* ���Ԥ������ϥ����ϤΤ� */
            SYSLOGERROR(ERR_S_LINK, s_id, p->ss_data.ss_str, E_STR);
        }
        strset_free(&path);
        p = p->next;
    }

    return;
}

/*
 * md_list2str
 *
 * strlist�����ΰ������饫��޶��ڤ��ʸ������������
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct strset *         ��Ǽ���strset��¤�ΤΥݥ���
 *      struct strlist *        ��������Ƭ�ݥ���
 *
 * �֤���
 *      �ʤ�
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
            /* 2���ܰʹߤ� ", " �ǷҤ��� */
            ret = strset_catstr(&str, ", ");
            if (ret < 0) {
                SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
                exit(EXIT_MILTER);
            }
        }
        ret = strset_catstrset(&str, &(p->ss_data));
        if (ret < 0) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
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
 * enczip��¤�Τ��������
 *
 * ����
 *      struct enczip *       enczip��¤�ΤΥݥ���
 *
 * �֤���
 *      �ʤ�
 */
static void
md_free(struct enczip *md)
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
enczip_mod_extra_config(struct config **cfg)
{
    return R_SUCCESS;
}
