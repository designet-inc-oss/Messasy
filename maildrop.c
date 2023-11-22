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
 * $RCSfile: maildrop.c,v $
 * $Revision: 1.30 $
 * $Date: 2009/10/22 04:18:09 $
 */

#ifdef OLD_CODE

#include <stdio.h>
#include <stdlib.h>
#include <libdgstr.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <limits.h>
#include <libmilter/mfapi.h>
#include <sys/utsname.h>

#include <libdgstr.h>
#include <libdgconfig.h>
#include <libdgmail.h>

#include "messasy.h"
#include "msy_config.h"
#include "maildrop.h"
#include "utils.h"
#include "log.h"

static struct maildrop *md_struct_init(unsigned int, struct config *,
                                        time_t, struct strset *,
                                        struct strlist *, struct strlist *);
static int md_makesavefilename(unsigned int, struct maildrop *, char *, int);
static int md_makedirlist(unsigned int, struct maildrop *, struct strlist **);
static int md_makedirbylist(unsigned int, struct maildrop *, struct strlist *);
static int md_makemaildir_tree(unsigned int, char *, int);
static void md_makemaildir(unsigned int, char *);
static int md_mkdir(unsigned int, char *);
static void md_makesavefile(unsigned int, struct maildrop *,
                            char *, struct strlist *);
static void md_list2str(unsigned int, struct strset *, struct strlist *);
static void md_free(struct maildrop *);

/*
 * maildrop_open
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
 *      struct maildrop *       ����
 *      NULL                    ���顼 (����ե�����Υ����ץ�˼���)
 */
struct maildrop *
maildrop_open(unsigned int s_id, struct config *config,
                time_t time, struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct maildrop *md;
    mode_t old_umask;
    int temppathlen;

    /* maildrop��¤�Τ����� */
    md = md_struct_init(s_id, config, time, from, to_h, saveaddr_h);

    /* MailDir�۲��˥��֥ǥ��쥯�ȥ��������� */
    md_makemaildir(s_id, config->cf_maildir);

    /* ����ե�����Υѥ������ */
    temppathlen = strlen(config->cf_maildir) +
                    strlen(msy_hostname) + TEMPFILEPATH_LEN;
    md->md_tempfilepath = (char *)malloc(temppathlen);
    if (md->md_tempfilepath == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "maildrop_open", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(md->md_tempfilepath, TEMPFILEPATH,
            config->cf_maildir, md->md_recvtime, msy_hostname);

    /* ����ե�����򥪡��ץ� */
    old_umask = umask(0077);
    md->md_tempfile_fd = mkstemp(md->md_tempfilepath);
    umask(old_umask);
    if (md->md_tempfile_fd < 0) {
        SYSLOGERROR(ERR_S_MKSTEMP, s_id, md->md_tempfilepath, E_STR);
        md_free(md);
        return NULL;
    }

    return md;
}

/*
 * maildrop_write_header
 *
 * �إå������ե�����˽��Ϥ���
 * ��������إå�����˽񤭹���
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct maildrop *       maildrop��¤��
 *      char *                  �إå��ե������ (������Хå����Ϥ��줿�ޤ�)
 *      char *                  �إå��� (������Хå����Ϥ��줿�ޤ�)
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
maildrop_write_header(unsigned int s_id, struct maildrop *md,
                        char *headerf, char *headerv)
{
    char *header, *p;
    int header_len;
    ssize_t written_len;
    int ret;

    if (!md->md_writing_header) {
        /* �Ϥ���˥�������إå���񤭹��� */
        md->md_writing_header = 1;
        ret = maildrop_write_header(s_id, md, CUSTOMHDR_FROM,
                                    md->md_header_from.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
        ret = maildrop_write_header(s_id, md, CUSTOMHDR_TO,
                                    md->md_header_to.ss_str);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* �إå��ν񤭹��� */
    header_len = strlen(headerf) + strlen(headerv) + 3; // ʸ���� + ': ' + '\n'
    header = (char *)malloc(header_len + 1);    // '\0'
    if (header == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "maildrop_write_header", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(header, "%s: %s\n", headerf, headerv);

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
 * maildrop_write_body
 *
 * �᡼��ܥǥ������ե�����˽��Ϥ���
 * �إå��ȥܥǥ��ζ��ڤ����˽񤭹���
 * ����ʸ����CRLF��LF�����줹��
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct maildrop *       maildrop��¤��
 *      unsigned char *         �ܥǥ� (������Хå����Ϥ��줿�ޤ�)
 *      size_t                  Ĺ�� (������Хå����Ϥ��줿�ޤ�)
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
maildrop_write_body(unsigned int s_id, struct maildrop *md,
                    unsigned char *bodyp, size_t len)
{
    ssize_t written_len;
    int ret;
    int i;

    if (!md->md_writing_body) {
        /* �Ϥ���˥إå��ȥܥǥ��ζ��ڤ�ʸ����񤭹��� */
        md->md_writing_body = 1;
        ret = maildrop_write_body(s_id, md, (unsigned char *) "\n", 1);
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
 * maildrop_close
 *
 * ����ե�����򥯥���������¸����
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct maildrop *       maildrop��¤��
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
int
maildrop_close(unsigned int s_id, struct maildrop *md)
{
    struct strlist *list_h;
    char filename[NAME_MAX + 6];
    size_t ret_s;
    int ret;

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

    /* ��¸��Υե�����̾����� */
    ret = md_makesavefilename(s_id, md, filename, sizeof(filename));
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* ɬ�פʥǥ��쥯�ȥ��������� */
    ret = md_makedirlist(s_id, md, &list_h);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* ɬ�פʥǥ��쥯�ȥ����� */
    ret = md_makedirbylist(s_id, md, list_h);
    if (ret != R_SUCCESS) {
        free_strlist(list_h);
        return R_ERROR;
    }

    /* �ե����륳�ԡ� */
    md_makesavefile(s_id, md, filename, list_h);

    /* ����ե������� */
    unlink(md->md_tempfilepath);

    free_strlist(list_h);
    md_free(md);

    return 0;
}

/*
 * maildrop_abort
 *
 * �᡼����¸��������ߤ���
 *
 * ����
 *      unsigned int            ���å����ID (��������)
 *      struct maildrop *       maildrop��¤��
 *
 * �֤���
 *      �ʤ�
 */
void
maildrop_abort(unsigned int s_id, struct maildrop *md)
{
    if (md == NULL) {
        return;
    }

    if (md->md_tempfile_fd > 0) {
        /* �ե����뤬�����ץ󤵤�Ƥ�����ϥ��������� */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }
    unlink(md->md_tempfilepath);

    md_free(md);

    return;
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
 *      struct strset *         Envelope From���ɥ쥹
 *      struct strlist *        Envelope To���ɥ쥹��������Ƭ�ݥ���
 *      struct strlist *        ��¸�оݥ��ɥ쥹��������Ƭ�ݥ���
 *
 * �֤���
 *      struct maildrop *       maildrop��¤��
 */
static struct maildrop *
md_struct_init(unsigned int s_id, struct config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct maildrop *md;
    int ret;

    /* �ΰ����� */
    md = (struct maildrop *)malloc(sizeof(struct maildrop));
    if (md == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "md_struct_init", E_STR);
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct maildrop));

    /* ������������¸ */
    md->md_recvtime = time;

    /* MailDir, MailFolder���ͤ���¸ */
    strset_set(&md->md_maildir, config->cf_maildir);
    strset_set(&md->md_mailfolder, config->cf_mailfolder);

    /* *Delimiter���ͤ���¸ */
    md->md_dotdelimiter = *(config->cf_dotdelimiter);
    md->md_slashdelimiter = *(config->cf_slashdelimiter);

    /* ��������إå������ */
    strset_init(&md->md_header_from);
    ret = strset_catstrset(&md->md_header_from, from);
    if (ret == -1) {
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
        exit(EXIT_MILTER);
    }

    strset_init(&md->md_header_to);
    md_list2str(s_id, &md->md_header_to, to_h);

    /* ��¸���ɥ쥹��������¸ */
    md->md_saveaddr_h = saveaddr_h;

    return md;
}

/*
 * md_makedirlist
 *
 * ��������ɬ�פΤ���ǥ��쥯�ȥ�������������
 *
 * ����
 *      unsigned int            ���å����ID
 *      struct maildrop *       maildrop��¤��
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
static int
md_makedirlist(unsigned int s_id, struct maildrop *md, struct strlist **list_h)
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
 *      struct maildrop *       maildrop��¤��
 *      struct strlist *        �ǥ��쥯�ȥ��������Ƭ�ݥ���
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
static int
md_makedirbylist(unsigned int s_id, struct maildrop *md, struct strlist *list)
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
 *      unsigned int            ���å����ID
 *      struct maildrop *       maildrop��¤��
 *      char *                  �ե�����̾�γ�Ǽ��
 *      int                     ��Ǽ���Ĺ��
 *
 * �֤���
 *      R_SUCCESS               ����
 *      R_ERROR                 ���顼
 */
static int
md_makesavefilename(unsigned int s_id, struct maildrop *md,
                    char *filename, int filename_len)
{
    struct stat stbuf;
    int ret;

    /* ����ե������i�Ρ����ֹ���� */
    ret = stat(md->md_tempfilepath, &stbuf);
    if (ret < 0) {
        SYSLOGERROR(ERR_S_STAT, s_id, md->md_tempfilepath, E_STR);
        return R_ERROR;
    }

    /* �ե�����Υѥ� (/new/....) ��������� */
    snprintf(filename, filename_len, SAVEFILENAME,
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
 *      struct maildrop *       maildrop��¤��
 *      char *                  ��¸�ե�����̾
 *      struct strlist *        �ǥ��쥯�ȥ����
 *
 * �֤���
 *      �ʤ� (��󥯤˼��Ԥ�������)
 */
static void
md_makesavefile(unsigned int s_id, struct maildrop *md,
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
 * maildrop��¤�Τ��������
 *
 * ����
 *      struct maildrop *       maildrop��¤�ΤΥݥ���
 *
 * �֤���
 *      �ʤ�
 */
static void
md_free(struct maildrop *md)
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

#endif    /* OLD_CODE */
