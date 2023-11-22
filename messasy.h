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
 * $RCSfile: messasy.h,v $
 * $Revision: 1.20 $
 * $Date: 2009/10/29 09:19:45 $
 */

#ifndef _MESSASY_H_
#define _MESSASY_H_

#include <libmilter/mfapi.h>
#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>

#include "msy_config.h"

/* ���եȥ������С������ */
#define VERSION              "3.00"

#define IDENT   "messasy"

#define CONNSOCK_LEN    30
#define CONNSOCK        "inet:%d@%s"

#define MAX_ADDRESS_LEN 256

#define EXIT_SUCCESS      0
#define EXIT_MAIN       100
#define EXIT_MILTER     200
#define EXIT_MANAGER    300
#define EXIT_UTILS      400

#define R_SUCCESS          0
#define R_POSITIVE      100
#define R_ERROR         -100

#define TRUE            1
#define FALSE           0

/* �����Х��ѿ� */
#define MAX_HOSTNAME_LEN 255
extern char msy_hostname[MAX_HOSTNAME_LEN + 1];

/* �ץ饤�١��ȹ�¤�� */
struct mlfiPriv {
    unsigned int        mlfi_sid;               /* ���å����ID */
    time_t              mlfi_recvtime;          /* �������� */
    _SOCK_ADDR          mlfi_clientaddr;        /* ���饤����ȥ��ɥ쥹 */
    struct strset       mlfi_envfrom;           /* From */

    struct strlist      *mlfi_rcptto_h;         /* To��������Ƭ */
    struct strlist      *mlfi_rcptto_t;         /* To���������� */

    struct strlist      *mlfi_addrmatched_h;    /* ��¸�оݥ��ɥ쥹��������Ƭ */
    struct strlist      *mlfi_addrmatched_t;    /* ��¸�оݥ��ɥ쥹���������� */

    struct config       *config;                /* ����ե����� */
    struct extrapriv    *mlfi_extrapriv;        /* �⥸�塼���ΰ�ؤΥݥ��� */

#ifdef OLD_CODE
    struct maildrop     *md;                    /* maildrop */
#else    /* OLD_CODE */
    int                 header_existence;      /* header1���ܤ��Υե饰 */
#endif   /* OLD_CODE */
};

/* �⥸�塼��̾�Ȥ��Υץ饤�١���buf */
struct extrapriv {
    struct extrapriv  *expv_next;
    char              *expv_modulename;
    void              *expv_modulepriv;
};

/* ������Хå��ؿ� */
sfsistat mlfi_connect(SMFICTX *, char *, _SOCK_ADDR *);
sfsistat mlfi_envfrom(SMFICTX *, char **);
sfsistat mlfi_envrcpt(SMFICTX *, char **);
sfsistat mlfi_header(SMFICTX *, char *, char *);
sfsistat mlfi_eoh(SMFICTX *);
sfsistat mlfi_body(SMFICTX *, u_char *, size_t);
sfsistat mlfi_eom(SMFICTX *);
sfsistat mlfi_abort(SMFICTX *);
sfsistat mlfi_close(SMFICTX *);

/* ����¾�ؿ� */
sfsistat mlfi_cleanup(SMFICTX *);
sfsistat mlfi_freepriv(SMFICTX *);

#endif // _MESSASY_H_
