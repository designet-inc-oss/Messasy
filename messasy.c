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
 * $RCSfile: messasy.c,v $
 * $Revision: 1.52 $
 * $Date: 2009/10/30 04:07:38 $
 */

/* add included header for make */
//#include "config.h"
//#include <config.h>
//#include "./config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <error.h>
#include <errno.h>
#include <pthread.h>
#include <libmilter/mfapi.h>
#include <regex.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/utsname.h>

#if 0
#include <lib/libdgstr/libdgstr.h>
#include <lib/libdgmail/libdgmail.h>
#include <lib/libdgconfig/libdgconfig.h>
#include "./lib/libdgstr/libdgstr.h"
#include "./lib/libdgmail/libdgmail.h"
#include "./lib/libdgconfig/libdgconfig.h"
#else
#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>
#endif

#include "messasy.h"
#include "client_side.h"
#include "msy_config.h"
#include "msy_readmodule.h";
#include "so/lib_lm.h";

#ifdef OLD_CODE
    #include "maildrop.h"
#endif    /* OLD_CODE */

#include "filter.h"
#include "utils.h"
#include "log.h"

/* �����Х��ѿ� */
char msy_hostname[MAX_HOSTNAME_LEN + 1];
extern struct modulehandle *mhandle_list;
extern struct cfentry cfe;

#define MLFIPRIV        ((struct mlfiPriv *) smfi_getpriv(ctx))

/* ������Хå��ؿ����� */
struct smfiDesc smfilter =
{
    IDENT,
    SMFI_VERSION,       /* version code -- do not change */
    SMFIF_ADDHDRS,      /* flags */
    mlfi_connect,       /* connection info filter */
    NULL,               /* SMTP HELO command filter */
    mlfi_envfrom,       /* envelope sender filter */
    mlfi_envrcpt,       /* envelope recipient filter */
    mlfi_header,        /* header filter */
    NULL,               /* end of header */
    mlfi_body,          /* body block filter */
    mlfi_eom,           /* end of message */
    mlfi_abort,         /* message aborted */
    mlfi_close          /* connection cleanup */
};

/*
 * manager_init
 *
 * �������󥿥ե������ε�ư
 *
 * ����
 *      �ʤ�
 *
 * �֤���
 *      R_SUCCESS ����
 *      R_ERROR   �����ƥ२�顼
 */
int
manager_init(void)
{
    int                    so;
    int                    on = 1;
    int                    ret;
    struct sockaddr_in     saddr;
    struct config          *cfg;
    pthread_t              manager;
    int                    backlog;

    char f_name[] = "manager_init";

    /* �����åȤ���� */
    so = socket(AF_INET, SOCK_STREAM, 0);
    if (so < 0) {
        SYSLOGERROR(ERR_SOCK, E_STR);
        return (R_ERROR);
    }

    ret = setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret != 0) {
        SYSLOGERROR(ERR_SETSOCK_REUSE, E_STR);
    }

    cfg = config_retrieve();

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(cfg->cf_commandport);
    inet_aton(cfg->cf_listenip, &saddr.sin_addr);

    backlog = cfg->cf_commandmaxclients + 1;

    config_release(cfg);

    ret = bind(so, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret != 0) {
        SYSLOGERROR(ERR_BIND, E_STR);
        close(so);
        return (R_ERROR);
    }

    ret = listen(so, backlog);
    if (ret != 0) {
        SYSLOGERROR(ERR_LISTEN, E_STR);
        close(so);
        return (R_ERROR);
    }

    /* �������󥿥ե������ε�ư */
    ret = pthread_create(&manager, NULL, manager_main, (void *)so);
    if (ret != 0) {
        SYSLOGERROR(ERR_THREAD_CREATE, f_name, E_STR);
        close(so);
        return (R_ERROR);
    }

    return R_SUCCESS;
}

/*
 * mlfi_connect
 *
 * ������Хå��ؿ� (CONNECT)
 * - ���å����ID���������
 * - ����ե���������Ƥ��������
 * - �ץ饤�١����ΰ���������
 */
sfsistat
mlfi_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr)
{
    struct mlfiPriv *priv;
    unsigned int s_id;
    struct config *config;
    int error_action;

    /* ���å����ID����� */
    s_id = get_sessid();

    /* ����ե���������Ƥ���� */
    config = config_retrieve();

    /* �ץ饤�١����ΰ����� */
    priv = malloc(sizeof(struct mlfiPriv));
    if (priv == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "mlfi_connect", E_STR);
        exit(EXIT_MILTER);
    }
    memset(priv, 0, sizeof(struct mlfiPriv));
    priv->config = config;
    priv->mlfi_sid = s_id;
    error_action = config->cf_erroraction_conv;

    /* ������������¸ */
    priv->mlfi_recvtime = time(NULL);
    if (priv->mlfi_recvtime < 0) {
        SYSLOGERROR(ERR_S_TIME, s_id, E_STR);
        config_release(config);
        free(priv);
        return error_action;
    }

    /* ���饤����ȥ��ɥ쥹�������¸ */
    memcpy(&priv->mlfi_clientaddr, hostaddr, sizeof(_SOCK_ADDR));

    /* �ץ饤�١����ΰ�򥻥å� */
    smfi_setpriv(ctx, priv);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_envfrom
 *
 * ������Хå��ؿ� (MAIL FROM)
 * - Envelop From���ɥ쥹����¸����
 */
sfsistat
mlfi_envfrom(SMFICTX *ctx, char **envfrom)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    char *fromaddr;

    fromaddr = get_addrpart((unsigned char *) *envfrom);
    if (fromaddr == NULL) {
        /* ���ꥨ�顼 */
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "get_addrpart", E_STR);
        exit(EXIT_MILTER);
    }
    strset_set(&(priv->mlfi_envfrom), fromaddr);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_envrcpt
 *
 * ������Хå��ؿ� (RCPT TO)
 * - Envelope To���ɥ쥹����¸����
 */
sfsistat
mlfi_envrcpt(SMFICTX *ctx, char **rcptto)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    char *rcptaddr;

    /* To���ɥ쥹����¸ */
    rcptaddr = get_addrpart((unsigned char *) *rcptto);

    if (rcptaddr == NULL) {
        /* ���ꥨ�顼 */
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "get_addrpart", E_STR);
        exit(EXIT_MILTER);
    }
    push_strlist(&priv->mlfi_rcptto_h, &priv->mlfi_rcptto_t, rcptaddr);

    free(rcptaddr);

    return SMFIS_CONTINUE;
}

/*
 * mlfi_header
 *
 * ������Хå��ؿ� (header)
 * - �إå������å� (SaveIgnoreHeader) ��Ԥʤ�
 * - ����ե�����򥪡��ץ󤹤�
 * - �إå���񤭹���
 */
sfsistat
mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)
{
    struct mlfiPriv *priv = MLFIPRIV;
    unsigned int s_id = priv->mlfi_sid;
    int error_action = priv->config->cf_erroraction_conv;
    int ret;
    
    /* �إå������å� */
    if (priv->config->cf_saveignoreheader_regex != NULL) {
        ret = check_header_regex(headerf, headerv,
                                    priv->config->cf_saveignoreheader_regex);
        if (ret == R_POSITIVE) {
            /* �ޥå������Τ���¸�оݳ��Ȥ������ */
            SYSLOGINFO(INFO_S_IGNOREHEADER, s_id, headerf);

#ifdef OLD_CODE
            maildrop_abort(s_id, priv->md);
#endif    /* OLD_CODE */

            mlfi_abort(ctx);
            return SMFIS_ACCEPT;
        }
    }

    /* �����ץ� */
#ifdef OLD_CODE
    if (priv->md == NULL) {
#else    /* OLD_CODE */
    if (priv->header_existence == FALSE) {
        /* �إå��������ɤ߹��ޤ줿�� */
        priv->header_existence = TRUE;
#endif    /* OLD_CODE */

        /* ��¸�оݥ��ɥ쥹�����κ��� */
        ret = make_savelist(&priv->mlfi_envfrom, priv->mlfi_rcptto_h,
                            &priv->mlfi_addrmatched_h, &priv->mlfi_addrmatched_t,
                            priv->config, s_id);
        if (ret != R_SUCCESS) {
            mlfi_abort(ctx);
            return error_action;
        }

        /* ��¸�оݥ��ɥ쥹���ʤ������� */
        if (priv->mlfi_addrmatched_h == NULL) {
            SYSLOGINFO(INFO_S_NOSAVEADDRESS, s_id);
            mlfi_abort(ctx);
            return SMFIS_ACCEPT;
        }

#ifdef OLD_CODE
        /* �᡼����¸�����򳫻� */
        priv->md = maildrop_open(s_id, priv->config, priv->mlfi_recvtime,
                                    &(priv->mlfi_envfrom), priv->mlfi_rcptto_h,
                                    priv->mlfi_addrmatched_h);
        }
#endif    /* OLD_CODE */

#ifdef OLD_CODE
        if (priv->md == NULL) {
            mlfi_abort(ctx);
            return error_action;
        }
#else    /* OLD_CODE */
        if (priv->header_existence == FALSE) {
            mlfi_abort(ctx);
            return error_action;
        }
#endif   /* OLD_CODE */
    }

#ifdef OLD_CODE
    /* �إå��񤭹��� */
    ret = maildrop_write_header(s_id, priv->md, headerf, headerv);
    if (ret != R_SUCCESS) {
        maildrop_abort(s_id, priv->md);
        mlfi_cleanup(ctx);
        return error_action;
    }

#endif     /* OLD_CODE */

    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
    ret = msy_exec_header(priv, headerf, headerv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_body
 *
 * ������Хå��ؿ� (body)
 * - ��ʸ��񤭹���
 */
sfsistat
mlfi_body(SMFICTX *ctx, u_char *bodyp, size_t bodylen)
{
    struct mlfiPriv *priv = MLFIPRIV;

#ifdef OLC_CODE
    unsigned int s_id = priv->mlfi_sid;
#endif    /* OLD_CODE */

    int error_action = priv->config->cf_erroraction_conv;
    int ret;

#ifdef OLD_CODE

    /* �ܥǥ��񤭹��� */
    ret = maildrop_write_body(s_id, priv->md, bodyp, bodylen);
    if (ret != R_SUCCESS) {
        maildrop_abort(s_id, priv->md);
        mlfi_cleanup(ctx);
        return error_action;
    }
#endif    /* OLD_CODE */

    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
    ret = msy_exec_body(priv,bodyp, bodylen);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_eoh
 *
 * ������Хå��ؿ� (header��λ)
 * - �᡼����¸�ν�λ������Ԥʤ�
 * - �꥽�������������
 */
sfsistat
mlfi_eoh(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    int error_action = priv->config->cf_erroraction_conv;
    int ret;

    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
    ret = msy_exec_eoh(priv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return SMFIS_CONTINUE;
}

/*
 * mlfi_eom
 *
 * ������Хå��ؿ� (DATA��λ)
 * - �᡼����¸�ν�λ������Ԥʤ�
 * - �꥽�������������
 */
sfsistat
mlfi_eom(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;

#ifdef OLD_CODE
    unsigned int s_id = priv->mlfi_sid;
#endif    /* OLD_CODE */

    int error_action = priv->config->cf_erroraction_conv;
    int ret;

#ifdef OLD_CODE

    /* ������ */
    ret = maildrop_close(s_id, priv->md);
    if (ret != R_SUCCESS) {
        maildrop_abort(s_id, priv->md);
        mlfi_cleanup(ctx);
        return error_action;
    }
#endif    /* OLD_CODE */

    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
    ret = msy_exec_eom(priv);
    if (ret != R_SUCCESS) {
        mlfi_abort(ctx);
        return error_action;
    }

    return eom_cleanup(ctx);
}

/*
 * mlfi_abort
 *
 * ������Хå��ؿ� (RSET��)
 * - �᡼����¸����߽�����Ԥʤ�
 * - �꥽�������������
 */
sfsistat
mlfi_abort(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;

#ifdef OLD_CODE
    unsigned int s_id = priv->mlfi_sid;
#endif    /* OLD_CODE */

    int error_action;
    int ret;

#ifdef OLD_CODE

    maildrop_abort(s_id, priv->md);
#endif    /* OLD_CODE */


    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
    if (priv != NULL) {
        error_action = priv->config->cf_erroraction_conv;
    } else {
        return mlfi_cleanup(ctx);
    }
    ret = msy_exec_abort(priv);
    if (ret != R_SUCCESS) {
        mlfi_cleanup(ctx);
        mlfi_freepriv(ctx);
        return error_action;
    }

    return mlfi_cleanup(ctx);
}

/*
 * mlfi_close
 *
 * ������Хå��ؿ� (���ͥ����������)
 * - ���⤷�ʤ�
 */
sfsistat
mlfi_close(SMFICTX *ctx)
{
#ifdef OLD_CODE
    return SMFIS_ACCEPT;
#endif    /* OLD_CODE */

    return mlfi_freepriv(ctx);
}

/*
 * eom_cleanup
 *
 * eom��λ��������ǡ�Ŭ���ʾ����������
 * ��mlfi_eom����ƤӽФ����
 *   (������Хå��ؿ��ǤϤʤ�)
 */
sfsistat
eom_cleanup(SMFICTX *ctx)
{
    int error_action;
    int ret;

    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_CONTINUE;

    if (priv == NULL) {
        return r;
    }

    strset_free(&priv->mlfi_envfrom);

    if (priv->mlfi_rcptto_h != NULL) {
        free_strlist(priv->mlfi_rcptto_h);
        priv->mlfi_rcptto_h = NULL;
    }
    if (priv->mlfi_addrmatched_h != NULL) {
        free_strlist(priv->mlfi_addrmatched_h);
        priv->mlfi_addrmatched_h = NULL;
    }

    /* �⥸�塼����δؿ���¹Ԥ���ؿ��θƤӽФ� */
//    error_action = priv->config->cf_erroraction_conv;
//    ret = msy_exec_abort(priv);
//    if (ret != R_SUCCESS) {
//        mlfi_cleanup(ctx);
//        mlfi_freepriv(ctx);
//        return error_action;
//    }

    return r;
}

/*
 * mlfi_cleanup
 *
 * �ץ饤�١����ΰ���������
 * ��mlfi_eom, mlfi_abort����ƤӽФ����
 *   (������Хå��ؿ��ǤϤʤ�)
 */
sfsistat
mlfi_cleanup(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_CONTINUE;

    if (priv == NULL) {
        return r;
    }

    if (priv->config != NULL) {
        config_release(priv->config);
        priv->config = NULL;
    }

    strset_free(&priv->mlfi_envfrom);

    if (priv->mlfi_rcptto_h != NULL) {
        free_strlist(priv->mlfi_rcptto_h);
        priv->mlfi_rcptto_h = NULL;
    }
    if (priv->mlfi_addrmatched_h != NULL) {
        free_strlist(priv->mlfi_addrmatched_h);
        priv->mlfi_addrmatched_h = NULL;
    }

    free(priv);

    smfi_setpriv(ctx, NULL);

    return r;
}

/*
 * mlfi_freepriv
 *
 * �ץ饤�١����ΰ���������
 * ��mlfi_close����ƤӽФ����
 *   (������Хå��ؿ��ǤϤʤ�)
 */
sfsistat
mlfi_freepriv(SMFICTX *ctx)
{
    struct mlfiPriv *priv = MLFIPRIV;
    sfsistat r = SMFIS_ACCEPT;

    if (priv == NULL) {
        return r;
    }

    if (priv->config != NULL) {
        config_release(priv->config);
        priv->config = NULL;
    }

    free(priv);

    smfi_setpriv(ctx, NULL);
 
    return r;
}

/*
 * usage
 *
 * usage��å�������ɽ������
 */
void
usage(char *arg)
{
    printf("usage: %s [config file] [module config file]\n", arg);
}

/*
 * main
 */
int
main(int argc, char *argv[])
{
    char connsock[CONNSOCK_LEN];
    struct config *config;
    struct utsname utsname;

    char defaultconf[PATH_MAX + 1];
    char defaultmoduleconf[PATH_MAX + 1];
    char *configfile;
    char *module_configfile = NULL;

    int ret;

    /* ���ץ��������å� */
    switch (argc) {
        /* �����ǻ��ꤵ��Ƥ��ʤ���� */
        case 1:
            snprintf(defaultconf, PATH_MAX, "%s/messasy.conf",
                     DEFAULT_CONFDIR);
            configfile = defaultconf;
        
            snprintf(defaultmoduleconf, PATH_MAX, "%s/module.conf",
                     DEFAULT_CONFDIR);
            module_configfile = defaultmoduleconf;

            break;

        /* �����ǻ��ꤵ��Ƥ����� */
        case 3:
            configfile = argv[1];
            module_configfile = argv[2];
            break;

        default:
            usage(argv[0]);
            exit(EXIT_MAIN);
    }

    /* �Ķ��ѿ������� */
    set_environment(configfile);

    /* �⥸�塼������ե�������ɤ߹��� */
    mhandle_list = NULL;
    ret = read_module_config(module_configfile);
    if (ret != R_SUCCESS) {
        free_lib_handle();
        exit(EXIT_MAIN);
    }

    /* ����ե������ɤ߹��� */
    ret = reload_config();
    if (ret != R_SUCCESS) {
        exit(EXIT_MAIN);
    }

    /* �������󥿥ե������ε�ư */
    ret = manager_init();
    if (ret != R_SUCCESS) {
        exit (EXIT_MAIN);
    }

    config = config_retrieve();

    /* �ۥ���̾�򥰥��Х��ѿ�����¸ */
    if (uname(&utsname) < 0) {
        SYSLOGERROR(ERR_UNAME, E_STR);
        exit(EXIT_MAIN);
    }
    strncpy(msy_hostname, utsname.nodename, MAX_HOSTNAME_LEN + 1);

    /* �����åȤ����� */
    sprintf(connsock, CONNSOCK, config->cf_listenport, config->cf_listenip);
    if (smfi_setconn(connsock) == MI_FAILURE) {
        SYSLOGERROR(ERR_SETCONN, E_STR);
        exit(EXIT_MAIN);
    }

    /* �����ॢ���Ȥ����� */
    if (smfi_settimeout(config->cf_timeout) == MI_FAILURE) {
        SYSLOGERROR(ERR_SETTIMEOUT, E_STR);
        exit(EXIT_MAIN);
    }

    config_release(config);

    /* ������Хå��ؿ�����Ͽ */
    if (smfi_register(smfilter) == MI_FAILURE) {
        SYSLOGERROR(ERR_REGISTER, E_STR);
        exit(EXIT_MAIN);
    }

    /* libmilter�����������Ϥ� */
    if (smfi_main() == MI_FAILURE) {
        SYSLOGERROR(ERR_MLFISTART, E_STR);
        exit(EXIT_MAIN);
    }

    exit(EXIT_SUCCESS);
}
