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

#ifndef _LIBGZIP_H_
#define _LIBGZIP_H_
#include <libdgstr.h>
//#include "../messasy.h"
//#include "../utils.h"
//#include "../log.h"
#include "messasy.h"
#include "utils.h"
#include "log.h"


#define CUSTOMHDR_FROM  "X-Messasy-From"
#define CUSTOMHDR_TO    "X-Messasy-To"

/* ���̤Υޥ���*/
#define EXTEND_PART_OPTION_NUM_GZIP 2    //>=1�����ꤷ�Ƥ�������

#define GZIP_SUFFIX             ".gz"
#define GZIP_SUFFIX_LEN         sizeof(GZIP_SUFFIX) - 1
#define TEMPFILEPATH            "%s/tmp/%010ld.%s.XXXXXX"
#define TEMPFILEPATH_LEN        24      // + maildir_len + hostname_len

#define ZIPTEMPFILEPATH "%s"    GZIP_SUFFIX
#define ZIPTEMPFILEPATH_LEN     TEMPFILEPATH_LEN + GZIP_SUFFIX_LEN

#define SAVEFILENAME            "/new/%010ld.%010ld.%s"
#define SAVEZIPFILENAME         SAVEFILENAME GZIP_SUFFIX


#define UNKNOWN "UNKNOWN"
#define DOT     '.'
#define SLASH   '/'

#define MAILDROP_CFECOUNT        (sizeof(gzip_cfe) / sizeof(struct cfentry))

/**********************************
 * ��¤��
 **********************************/
struct gzip_config {
    char 	   *cf_gzipcommand;
    char 	   *cf_gzipmaildir;
    char 	   *cf_gzipmailfolder;
    char 	   *cf_gzipdotdelimiter;
    char 	   *cf_gzipslashdelimiter;
};
/**********************************
 * ��¤��
 **********************************/
/* �⥸�塼���ѤΥץ饤�١���buf */
struct gzip_priv {
    struct gzip     *mypriv; 
//    struct gzip_config     *myconfig; 
};

/**********************************
 * �ؿ��ΰ����ꥹ��
 **********************************/
struct gzip {
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
extern struct gzip *gzip_open(unsigned int, struct gzip_config *,
                                        time_t, struct strset *, struct strlist *,
                                        struct strlist *, char *);
extern int gzip_write_header(unsigned int, struct gzip *,
                                    char *, char *);
extern int gzip_write_body(unsigned int, struct gzip *,
                                unsigned char *, size_t);
extern int gzip_close(unsigned int, struct gzip *, struct config *);
extern void gzip_abort(unsigned int, struct gzip *);

extern int gzip_init(struct cfentry **, size_t *, struct config **, size_t *);
extern int gzip_mod_extra_config(struct config **cfg);

#endif // _LIBDUMMY_H_
