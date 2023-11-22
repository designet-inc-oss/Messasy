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
 * $RCSfile: maildrop.h,v $
 * $Revision: 1.13 $
 * $Date: 2009/10/22 02:10:39 $
 */

#ifdef OLD_CODE

#ifndef _MAILDROP_H_
#define _MAILDROP_H_

#define CUSTOMHDR_FROM  "X-Messasy-From"
#define CUSTOMHDR_TO    "X-Messasy-To"

#define TEMPFILEPATH            "%s/tmp/%010ld.%s.XXXXXX"
#define TEMPFILEPATH_LEN        24      // + maildir_len + hostname_len

#define SAVEFILENAME    "/new/%010ld.%010ld.%s"

#define UNKNOWN "UNKNOWN"
#define DOT     '.'
#define SLASH   '/'

struct maildrop {
    time_t              md_recvtime;            /* 受信日時 */
    struct strset       md_maildir;             /* MailDir */
    struct strset       md_mailfolder;          /* MailFolder */
    char                md_dotdelimiter;        /* DotDelimiter */
    char                md_slashdelimiter;      /* SlashDelimiter */

    char                *md_tempfilepath;       /* 一時ファイルのパス */
    int                 md_tempfile_fd;         /* 一時ファイルのfd */
    int                 md_writing_header;      /* ヘッダ書いてるフラグ */
    int                 md_writing_body;        /* ボディ書いてるフラグ */
    int                 md_cr;                  /* CRを見つけたフラグ
                                                 * (本文の改行文字統一に使用) */

    struct strset       md_header_from;         /* X-Messasy-Fromの値 */
    struct strset       md_header_to;           /* X-Messasy-Toの値 */

    struct strlist      *md_saveaddr_h;         /* 保存アドレス一覧の先頭 */
};

extern struct maildrop *maildrop_open(unsigned int, struct config *,
                                        time_t, struct strset *,
                                        struct strlist *, struct strlist *);
extern int maildrop_write_header(unsigned int, struct maildrop *,
                                    char *, char *);
extern int maildrop_write_body(unsigned int, struct maildrop *,
                                unsigned char *, size_t);
extern int maildrop_close(unsigned int, struct maildrop *);
extern void maildrop_abort(unsigned int, struct maildrop *);

#endif // _MAILDROP_H_

#endif   /* OLD_CODE */
