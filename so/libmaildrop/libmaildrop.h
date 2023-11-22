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

#ifndef _LIBMAILDROP_H_
#define _LIBMAILDROP_H_
#include <libdgstr.h>
#include "messasy.h"
#include "utils.h"
#include "log.h"


#define CUSTOMHDR_FROM  "X-Messasy-From"
#define CUSTOMHDR_TO    "X-Messasy-To"

#define TEMPFILEPATH            "%s/tmp/%010ld.%s.XXXXXX"
#define TEMPFILEPATH_LEN        24      // + maildir_len + hostname_len

#define SAVEFILENAME    "/new/%010ld.%010ld.%s"

#define UNKNOWN "UNKNOWN"
#define DOT     '.'
#define SLASH   '/'

#define MAILDROP_CFECOUNT        (sizeof(maildrop_cfe) / sizeof(struct cfentry))

/**********************************
 * ��¤��
 **********************************/
struct maildrop_config {
    char 	   *cf_maildir;
    char 	   *cf_mailfolder;
    char 	   *cf_dotdelimiter;
    char 	   *cf_slashdelimiter;
};
/**********************************
 * ��¤��
 **********************************/
/* �⥸�塼���ѤΥץ饤�١���buf */
struct maildrop_priv {
    struct maildrop     *mypriv; 
//    struct maildrop_config     *myconfig; 
};

/**********************************
 * �ؿ��ΰ����ꥹ��
 **********************************/
struct maildrop {
    time_t              md_recvtime;            /* �������� */
    struct strset       md_maildir;             /* MailDir */
    struct strset       md_mailfolder;          /* MailFolder */
    char                md_dotdelimiter;        /* DotDelimiter */
    char                md_slashdelimiter;      /* SlashDelimiter */

    char                *md_tempfilepath;       /* ����ե�����Υѥ� */
    int                 md_tempfile_fd;         /* ����ե������fd */
    int                 md_writing_header;      /* �إå��񤤤Ƥ�ե饰 */
    int                 md_writing_body;        /* �ܥǥ��񤤤Ƥ�ե饰 */
    int                 md_cr;                  /* CR�򸫤Ĥ����ե饰
                                                 * (��ʸ�β���ʸ������˻���) */

    struct strset       md_header_from;         /* X-Messasy-From���� */
    struct strset       md_header_to;           /* X-Messasy-To���� */

    struct strlist      *md_saveaddr_h;         /* ��¸���ɥ쥹��������Ƭ */
};

/**********************************
 * �ؿ��ꥹ��
 **********************************/
extern struct maildrop *maildrop_open(unsigned int, struct maildrop_config *,
                                        time_t, struct strset *, struct strlist *,
                                        struct strlist *, char *);
extern int maildrop_write_header(unsigned int, struct maildrop *,
                                    char *, char *);
extern int maildrop_write_body(unsigned int, struct maildrop *,
                                unsigned char *, size_t);
extern int maildrop_close(unsigned int, struct maildrop *, char *);
extern void maildrop_abort(unsigned int, struct maildrop *);

extern int maildrop_init(struct cfentry **, size_t *, struct config **, size_t *);
extern int maildrop_mod_extra_config(struct config **cfg);

#endif // _LIBDUMMY_H_
