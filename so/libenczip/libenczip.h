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
#define EXTEND_PART_OPTION_NUM_ENCZIP 3    //>=1�����ꤷ�Ƥ�������

#define TEMPFILEPATH            "%s/tmp/%010ld.%s.XXXXXX"
#define TEMPFILEPATH_LEN        24      // + maildir_len + hostname_len
#define ENCZIP_SUFFIX           ".zip"
#define ENCZIP_SUFFIX_LEN       sizeof(ENCZIP_SUFFIX) - 1

#define ENCZIP_TEMPFILEPATH     "%s" ENCZIP_SUFFIX
#define ENCZIP_TEMPFILEPATH_LEN TEMPFILEPATH_LEN + ENCZIP_SUFFIX_LEN
#define ENCZIP_ENV_NAME         "ZIPOPT"
#define ENCZIP_FIXOPTION        "-qjmP"
#define OVERWRITE               1

#define SAVEFILENAME            "/new/%010ld.%010ld.%s"
#define ENCZIPSAVEFILENAME      SAVEFILENAME ENCZIP_SUFFIX


#define UNKNOWN "UNKNOWN"
#define DOT     '.'
#define SLASH   '/'

#define MAILDROP_CFECOUNT        (sizeof(enczip_cfe) / sizeof(struct cfentry))

/**********************************
 * ��¤��
 **********************************/
struct enczip_config {
    char 	   *cf_enczipcommand;
    char 	   *cf_enczippassword;
    char 	   *cf_enczipmaildir;
    char 	   *cf_enczipmailfolder;
    char 	   *cf_enczipdotdelimiter;
    char 	   *cf_enczipslashdelimiter;
};
/**********************************
 * ��¤��
 **********************************/
/* �⥸�塼���ѤΥץ饤�١���buf */
struct enczip_priv {
    struct enczip     *mypriv; 
//    struct enczip_config     *myconfig; 
};

/**********************************
 * �ؿ��ΰ����ꥹ��
 **********************************/
struct enczip {
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
extern struct enczip *enczip_open(unsigned int, struct enczip_config *,
                                        time_t, struct strset *, struct strlist *,
                                        struct strlist *, char *);
extern int enczip_write_header(unsigned int, struct enczip *,
                                    char *, char *);
extern int enczip_write_body(unsigned int, struct enczip *,
                                unsigned char *, size_t);
extern int enczip_close(unsigned int, struct enczip *, struct config *);
extern void enczip_abort(unsigned int, struct enczip *);

extern int enczip_init(struct cfentry **, size_t *, struct config **, size_t *);
extern int enczip_mod_extra_config(struct config **cfg);

#endif // _LIBDUMMY_H_
