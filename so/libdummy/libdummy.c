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
 * ��ǽ:
 *    dummy�⥸�塼��ν�����ؿ�
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
dummy_init(struct cfentry **cfe, size_t *cfesize,
           struct config **cfg, size_t *cfgsize)
{
    struct config *new_cfg;
    struct cfentry *new_cfe;
    size_t new_cfesize, new_cfgsize;
    int ret, i;

    // �⥸�塼��ꥹ�Ȥؤ��ɲ�
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

    // cfg�γ�ĥ
    new_cfgsize = *cfgsize + sizeof(struct dummy_config);
    new_cfg = (struct config *)realloc(*cfg, new_cfgsize);
    if(new_cfg == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }
    *cfg = new_cfg;

    // cfe�γ�ĥ
    new_cfesize = *cfesize + sizeof(dummy_cfe);
    new_cfe = (struct cfentry *)realloc(*cfe, new_cfesize);
    if(new_cfe == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_set_module_list", strerror(errno));
        return (-1);
    }

    // dummy_cfe�Υ��ԡ�
    memcpy(new_cfe + *cfesize / sizeof(struct cfentry),
           &dummy_cfe, sizeof(dummy_cfe));

    // dataoffset�ι���
    for (i = 0; i < MAILDROP_CFECOUNT; i++) {
        new_cfe[(*cfesize / sizeof(struct cfentry)) + i].cf_dataoffset += *cfgsize;
    }
    *cfe = new_cfe;

    // �⥸�塼�����config��¤��offset���Ǽ
    ret = dummy_set_extra_config(MYMODULE, &(*cfg)->cf_extraconfig, *cfgsize);
    if (ret != 0) {
        return -1;
    }

    // cfesize, cfgsize�ι���
    *cfesize = new_cfesize;
    *cfgsize = new_cfgsize;

    return 0;
}

/*
 * dummy_set_module_list
 *
 * ��ǽ:
 *    dummy�⥸�塼���ѤΥ⥸�塼��ꥹ�Ⱥ���
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
dummy_set_module_list (char *modname, char *funcname, struct modulelist **list)
{
    struct modulelist *new_list;

    /* module̾�Υݥ��󥿤��Ǽ�����ΰ�γ��� */
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
 * ��ǽ:
 *    dummy�⥸�塼���Ѥ�extra config�κ���
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
dummy_set_extra_config (char *modname, struct extra_config **ext_cfg,
                        size_t cfgsize)
{
    struct extra_config *new_cfg;

    /* �����⥸�塼���config��¤�Υݥ��󥿤��Ǽ�����ΰ�γ��� */
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
 * ��ǽ:
 *    dummy��config�ΰ��free����ؿ�
 * ����:
 *    mP     : priv��¤�Τ�Ĥʤ���¤��
 * ����:
 *     0: ����
 *    -1: �۾�
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
 * �����ؿ�
 ***** ***** ***** ***** *****/

/*
 * md_struct_init
 *
 * maildrop��¤�Τγ��ݤȽ������Ԥʤ�
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct config *         config��¤�ΤΥݥ���
 *      time_t                  �᡼���������
 *
 * �֤���
 *      struct maildrop *       maildrop��¤��
 */
static struct dummy *
dummy_struct_init(unsigned int s_id, struct dummy_config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct dummy *md;
    //int ret;

    /* �ΰ����� */
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
 * maildrop��¤�Τ��������
 *
 * ����
 *      struct maildrop *       maildrop��¤�ΤΥݥ���
 *
 * �֤���
 *      �ʤ�
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
 * ��ǽ:
 *    extrapriv�ΰ褬�ʤ���к�������
 *    ����м�ʬ�Ѥ��ΰ��extrapriv�ΰ�ݥ��󥿤��֤��ؿ�
 * ����:
 *    priv: mlfiPriv��¤�ΤΥݥ���(�����Ϥ�)
 * ����:
 *   ��ʬ�Ѥ�extrapriv��¤�ΤΥݥ���
 */
struct extrapriv *
dummy_get_priv(struct mlfiPriv **priv)
{
    struct extrapriv *p = NULL;      /* ������ */
    struct extrapriv *mp = NULL;     /* ���������� */
    struct extrapriv *p_old = NULL;  /* �����桢�ҤȤ����Υݥ�����¸�� */

    if (*priv != NULL) {
        /* ��ʬ��priv��¤�Τ����뤫���� */
        for (p = (*priv)->mlfi_extrapriv; p != NULL; p = p->expv_next) {
            if (strcmp(MYMODULE, p->expv_modulename) == 0) {
                /* ���ä���꥿���� */
                return (p);
            }
            /* �ҤȤ����Υݥ��󥿳�Ǽ */
            p_old = p;
        }
    }
    /* ��ʬ�Ѥ�extrapriv�ΰ迷������ */
    mp = malloc(sizeof(struct extrapriv));
    if (mp == NULL) {
        SYSLOGERROR(ERR_MALLOC, "dummy_get_priv", E_STR);
        return (NULL);
    }
    /* �ͤγ�Ǽ */
    mp->expv_modulename = MYMODULE;
    mp->expv_next = NULL;
    mp->expv_modulepriv = NULL;

    /* ����¸�ߤ��Ƥ��ʤ��ä�����Ƭ�˥ݥ��󥿤��դ��� */
    if (p_old == NULL) {
        (*priv)->mlfi_extrapriv = mp;

    /* ¸�ߤ��Ƥ��뤬����ʬ�Ѥ��ʤ��ä�����ˤĤ��� */
    } else if (p == NULL) {
        p_old->expv_next = mp;
    }
    return (mp);
}

/*
 * dummy_priv_free
 *
 * ��ǽ:
 *    ���٤Ƥ�priv��¤�Τ�free����ؿ�
 * ����:
 *     extrapriv:   �����ι�¤�ΤΥݥ���(�����Ϥ�)
 * ����:
 *    ̵��
 */
void
dummy_priv_free(struct extrapriv *expv)
{

    /* NULL�����å� */
    if (expv != NULL) {
        /* maildrop_priv�ΰ褬������ */
        if (expv->expv_modulepriv != NULL) {
            /* maildrop_priv��¤�Τ�free */
            free(expv->expv_modulepriv);
            expv->expv_modulepriv = NULL;
        }
        /* extrapriv�ΰ��free */
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
dummy_exec_header(struct mlfiPriv *priv, char *headerf, char *headerv)
{

    struct extrapriv     *expv = NULL;
    struct extra_config  *p = NULL;
    struct dummy_priv    *mypv = NULL;
    struct dummy         *mydat = NULL;
    struct dummy         *mydatp = NULL;
    //int                  ret = 0;
    //unsigned int         s_id = priv->mlfi_sid;

    /* extrapriv�ΰ��̵ͭ */
    expv = dummy_get_priv(&priv);
    /* dummy_get_priv�����顼�λ� */
    if (expv == NULL) {
        SYSLOGERROR(ERR_EXEC_FUNC, "dummy_exec_header", "dummy_get_priv");
    /* dummy_priv�ΰ褬�ʤ��ä������ */
    } else if (expv->expv_modulepriv == NULL) {
        /* dummy�ΰ� */
        mypv = malloc(sizeof(struct dummy_priv));
        if (mypv == NULL) {
            SYSLOGERROR(ERR_MALLOC, "dummy_exec_header", E_STR);
            return(-1);
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
        /* ��¤�Τ�Ĥʤ��� */
        mypv->mypriv = mydat;
    }
    /* maildrop��¤�ΤΥݥ��󥿤��ѿ��˳�Ǽ */
    mydatp = ((struct dummy_priv *)expv->expv_modulepriv)->mypriv;

    return SMFIS_CONTINUE;
}

/*
 * dummy_exec_body
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
dummy_exec_body(struct mlfiPriv *priv, u_char *bodyp, size_t bodylen)
{
    struct extrapriv    *expv = NULL;
    struct dummy     *mydat = NULL;
//    int                 ret = 0;
//    unsigned int        s_id = priv->mlfi_sid;

    /* extrapriv�ΰ��̵ͭ */
    expv = dummy_get_priv(&priv);
    /* maildrop_get_priv�����顼�λ� */
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
dummy_exec_abort(struct mlfiPriv *priv)
{
    struct extrapriv    *p = NULL;
    struct extrapriv    *p_old = NULL;
    struct dummy        *md = NULL;
    unsigned int        s_id = priv->mlfi_sid;

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
                /* �ҤȤ����ι�¤�Τ�next��free���빽¤�Τ�next��Ĥʤ��� */
                p_old->expv_next = p->expv_next;
                md = ((struct dummy_priv *)p->expv_modulepriv)->mypriv;
                /* ���ܡ��� */
                dummy_abort(s_id, md);
                dummy_priv_free(p);
            } else {
               p_old->expv_next = NULL;
            }
        /* ��������ΰ褬mlfiPriv��¤�� */
        } else {
            if (p != NULL) {
                /* �ҤȤ����ι�¤�Τ�next��free���빽¤�Τ�next��Ĥʤ��� */
                priv->mlfi_extrapriv = p->expv_next;
                md = ((struct dummy_priv *)p->expv_modulepriv)->mypriv;
                /* ���ܡ��� */
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
