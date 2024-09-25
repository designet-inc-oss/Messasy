/*
 * messasy
 *
 * Copyright (C) 2006-2024 DesigNET, INC.
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
 * ファイル書き込みの準備を行なう
 * - 必要なディレクトリの作成
 * - 一時ファイルのオープン
 * - カスタムヘッダの値作成
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct config *         config構造体
 *      time_t                  受信日時
 *      struct strset *         From
 *      struct strlist *        To一覧の先頭ポインタ
 *      struct strlist *        保存アドレス一覧の先頭ポインタ
 *
 * 返り値
 *      struct maildrop *       正常
 *      NULL                    エラー (一時ファイルのオープンに失敗)
 */
struct maildrop *
maildrop_open(unsigned int s_id, struct config *config,
                time_t time, struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct maildrop *md;
    mode_t old_umask;
    int temppathlen;

    /* maildrop構造体を初期化 */
    md = md_struct_init(s_id, config, time, from, to_h, saveaddr_h);

    /* MailDir配下にサブディレクトリを作成する */
    md_makemaildir(s_id, config->cf_maildir);

    /* 一時ファイルのパスを作成 */
    temppathlen = strlen(config->cf_maildir) +
                    strlen(msy_hostname) + TEMPFILEPATH_LEN;
    md->md_tempfilepath = (char *)malloc(temppathlen);
    if (md->md_tempfilepath == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "maildrop_open", E_STR);
        exit(EXIT_MILTER);
    }
    sprintf(md->md_tempfilepath, TEMPFILEPATH,
            config->cf_maildir, md->md_recvtime, msy_hostname);

    /* 一時ファイルをオープン */
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
 * ヘッダを一時ファイルに出力する
 * カスタムヘッダを初めに書き込む
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct maildrop *       maildrop構造体
 *      char *                  ヘッダフィールド (コールバックに渡されたまま)
 *      char *                  ヘッダ値 (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
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
        /* はじめにカスタムヘッダを書き込む */
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

    /* ヘッダの書き込み */
    header_len = strlen(headerf) + strlen(headerv) + 3; // 文字列 + ': ' + '\n'
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
 * メールボディを一時ファイルに出力する
 * ヘッダとボディの区切りを初めに書き込む
 * 改行文字はCRLFをLFに統一する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct maildrop *       maildrop構造体
 *      unsigned char *         ボディ (コールバックに渡されたまま)
 *      size_t                  長さ (コールバックに渡されたまま)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
maildrop_write_body(unsigned int s_id, struct maildrop *md,
                    unsigned char *bodyp, size_t len)
{
    ssize_t written_len;
    int ret;
    int i;

    if (!md->md_writing_body) {
        /* はじめにヘッダとボディの区切り文字を書き込む */
        md->md_writing_body = 1;
        ret = maildrop_write_body(s_id, md, (unsigned char *) "\n", 1);
        if (ret != R_SUCCESS) {
            return ret;
        }
    }

    /* 改行文字をLFに統一しながら、本文を書き込む */
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
 * 一時ファイルをクローズし、保存する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct maildrop *       maildrop構造体
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
int
maildrop_close(unsigned int s_id, struct maildrop *md)
{
    struct strlist *list_h;
    char filename[NAME_MAX + 6];
    size_t ret_s;
    int ret;

    /* 一時ファイルをクローズ */
    if (md->md_tempfile_fd > 0) {
        /* 改行文字扱いでないCRが残っている場合は書き込む */
        if (md->md_cr) {
            ret_s = write(md->md_tempfile_fd, "\r", 1);
            if (ret_s < 0) {
                SYSLOGERROR(ERR_S_FWRITE, s_id, md->md_tempfilepath, E_STR);
            }
            md->md_cr = 0;
        }
        /* クローズ */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }

    /* 保存先のファイル名を作成 */
    ret = md_makesavefilename(s_id, md, filename, sizeof(filename));
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* 必要なディレクトリ一覧を作成 */
    ret = md_makedirlist(s_id, md, &list_h);
    if (ret != R_SUCCESS) {
        return R_ERROR;
    }

    /* 必要なディレクトリを作成 */
    ret = md_makedirbylist(s_id, md, list_h);
    if (ret != R_SUCCESS) {
        free_strlist(list_h);
        return R_ERROR;
    }

    /* ファイルコピー */
    md_makesavefile(s_id, md, filename, list_h);

    /* 一時ファイル削除 */
    unlink(md->md_tempfilepath);

    free_strlist(list_h);
    md_free(md);

    return 0;
}

/*
 * maildrop_abort
 *
 * メール保存処理を中止する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct maildrop *       maildrop構造体
 *
 * 返り値
 *      なし
 */
void
maildrop_abort(unsigned int s_id, struct maildrop *md)
{
    if (md == NULL) {
        return;
    }

    if (md->md_tempfile_fd > 0) {
        /* ファイルがオープンされている場合はクローズする */
        close(md->md_tempfile_fd);
        md->md_tempfile_fd = 0;
    }
    unlink(md->md_tempfilepath);

    md_free(md);

    return;
}


/***** ***** ***** ***** *****
 * 内部関数
 ***** ***** ***** ***** *****/

/*
 * md_struct_init
 *
 * maildrop構造体の確保と初期化を行なう
 *
 * 引数
 *      unsigned int            セッションID
 *      struct config *         config構造体のポインタ
 *      time_t                  メール受信時刻
 *      struct strset *         Envelope Fromアドレス
 *      struct strlist *        Envelope Toアドレス一覧の先頭ポインタ
 *      struct strlist *        保存対象アドレス一覧の先頭ポインタ
 *
 * 返り値
 *      struct maildrop *       maildrop構造体
 */
static struct maildrop *
md_struct_init(unsigned int s_id, struct config *config, time_t time,
                struct strset *from, struct strlist *to_h,
                struct strlist *saveaddr_h)
{
    struct maildrop *md;
    int ret;

    /* 領域を確保 */
    md = (struct maildrop *)malloc(sizeof(struct maildrop));
    if (md == NULL) {
        SYSLOGERROR(ERR_S_MALLOC, s_id, "md_struct_init", E_STR);
        exit(EXIT_MILTER);
    }
    memset(md, 0, sizeof(struct maildrop));

    /* 受信日時を保存 */
    md->md_recvtime = time;

    /* MailDir, MailFolderの値を保存 */
    strset_set(&md->md_maildir, config->cf_maildir);
    strset_set(&md->md_mailfolder, config->cf_mailfolder);

    /* *Delimiterの値を保存 */
    md->md_dotdelimiter = *(config->cf_dotdelimiter);
    md->md_slashdelimiter = *(config->cf_slashdelimiter);

    /* カスタムヘッダを作成 */
    strset_init(&md->md_header_from);
    ret = strset_catstrset(&md->md_header_from, from);
    if (ret == -1) {
        SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstrset", E_STR);
        exit(EXIT_MILTER);
    }

    strset_init(&md->md_header_to);
    md_list2str(s_id, &md->md_header_to, to_h);

    /* 保存アドレス一覧を保存 */
    md->md_saveaddr_h = saveaddr_h;

    return md;
}

/*
 * md_makedirlist
 *
 * 作成する必要のあるディレクトリ一覧を作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct maildrop *       maildrop構造体
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
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

    /* 受信時刻から置換文字列を作成 */
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

    /* 保存対象アドレス毎にディレクトリ名を作成する */
    *list_h = list_t = NULL;
    p = md->md_saveaddr_h;
    while (p != NULL) {
        /* アドレスとドメインから置換文字列を作成する */
        strncpy(mailaddr, p->ss_data.ss_str, MAX_ADDRESS_LEN + 1);
        ret = check_7bit(mailaddr);
        if (ret != 0) {
            /* 8bit文字が含まれるためUNKNOWNに */
            addr_p = UNKNOWN;
            domain_p = UNKNOWN;
        } else {
            replace_delimiter(mailaddr, DOT, md->md_dotdelimiter);
            replace_delimiter(mailaddr, SLASH, md->md_slashdelimiter);

            domain_p = strchr(mailaddr, '@');
            if (domain_p == NULL) {
                /* アドレス一覧の作成時にドメインが補完されるので、
                 * ここには入らないはず */
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

        /* MailFolderのフォーマット文字を置換する */
        tmp = str_replace_format(md->md_mailfolder.ss_str, sf, 6);

        /* MailDir, MailFolder (置換後) を連結する */
        strset_init(&path);
        if (strset_catstrset(&path, &md->md_maildir) == -1 ||
            strset_catstr(&path, "/") == -1 ||
            strset_catstr(&path, tmp) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
        free(tmp);

        /* ディレクトリ一覧に追加する
         * まったく同一のパスが既に一覧にある場合は無視する */
        uniq_push_strlist(list_h, &list_t, path.ss_str);

        strset_free(&path);
        p = p->next;
    }

    return R_SUCCESS;
}

/*
 * md_makedirbylist
 *
 * ディレクトリ一覧を元に、Maildir形式のディレクトリを作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct maildrop *       maildrop構造体
 *      struct strlist *        ディレクトリ一覧の先頭ポインタ
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
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
 * 指定されたディレクトリに至るディレクトリツリーをMaildir形式で作成する
 *
 * /home/archive/Maildir/.2009.10.01
 * → /home/archive/Maildir/.2009/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10/{new,cur,tmp}
 *    /home/archive/Maildir/.2009.10.01/{new,cur,tmp}
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *                              (最も深いディレクトリを指定する)
 *      int                     ベースディレクトリの長さ
 *                              (Maildir形式のツリーの起点を指定する)
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー (引数エラー)
 */
static int
md_makemaildir_tree(unsigned int s_id, char *targetdir, int basedir_len)
{
    char *subtop, *dot;

    /* ポインタをフォルダ先頭のドットに移動させる
     * /path/to/basedir/.folder
     *                  ^-subtop */
    subtop = targetdir + basedir_len + 1;
    if (strchr(subtop, SLASH) != NULL) {
        /* 万一フォルダ配下にスラッシュが含まれていた場合は
         * 対応していないのでエラーを返す */
        return R_ERROR;
    }

    /* サブディレクトリを作成する */
    while ((dot = strchr(subtop, DOT)) != NULL) {
        /* ドットを\0に一時的に置き換えてディレクトリを作成 */
        *dot = '\0';
        md_makemaildir(s_id, targetdir);
        *dot = DOT;
        subtop = dot + 1;
    }

    /* 最終的なディレクトリを作成 */
    md_makemaildir(s_id, targetdir);

    return R_SUCCESS;
}

/*
 * md_makemaildir
 *
 * 指定されたディレクトリを作成し、その配下に
 *   /new, /cur, /tmp
 * の3つのディレクトリを作成する
 * ※ディレクトリの作成に失敗した場合もエラーとしない
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *
 * 返り値
 *      なし
 */
static void
md_makemaildir(unsigned int s_id, char *dirname)
{
    /* 作成するサブディレクトリ一覧 */
    char *subdirs[] = {
                       "/new",
                       "/cur",
                       "/tmp",
                       NULL
                      };

    struct strset createpath;
    char *tmp;
    int ret, i;

    /* ベースディレクトリを作成 */
    md_mkdir(s_id, dirname);

    strset_init(&createpath);
    for (i = 0; subdirs[i] != NULL; i++) {
        /* MailDirをコピー */
        tmp = strdup(dirname);
        if (tmp == NULL) {
            SYSLOGERROR(ERR_S_MALLOC, s_id, "md_makemaildir", E_STR);
            exit(EXIT_MILTER);
        }
        strset_set(&createpath, tmp);

        /* サブディレクトリ名 (.../new, .../cur, .../tmp) を付加 */
        ret = strset_catstr(&createpath, subdirs[i]);
        if (ret == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }
         
        /* サブディレクトリを作成 */
        md_mkdir(s_id, createpath.ss_str);

        strset_free(&createpath);
    }

    return;
}

/*
 * md_mkdir
 *
 * 指定されたディレクトリを作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      char *                  ディレクトリ名
 *
 * 返り値
 *      R_SUCCESS               正常 (既にディレクトリが存在した場合も)
 *      R_ERROR                 エラー
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

        /* 作成した */
        return R_SUCCESS;

    } else {
        if (!S_ISDIR(stbuf.st_mode)) {
            SYSLOGERROR(ERR_S_NDIR, s_id, dirname, E_STR);
            return R_ERROR;
        }

        /* 既に存在した */
        return R_SUCCESS;
    }

    /* 念のため */
    return R_SUCCESS;
}

/*
 * md_makesavefilename
 *
 * 保存ファイル名 ("/new/.....") を作成する
 *
 * 引数
 *      unsigned int            セッションID
 *      struct maildrop *       maildrop構造体
 *      char *                  ファイル名の格納先
 *      int                     格納先の長さ
 *
 * 返り値
 *      R_SUCCESS               正常
 *      R_ERROR                 エラー
 */
static int
md_makesavefilename(unsigned int s_id, struct maildrop *md,
                    char *filename, int filename_len)
{
    struct stat stbuf;
    int ret;

    /* 一時ファイルのiノード番号を取る */
    ret = stat(md->md_tempfilepath, &stbuf);
    if (ret < 0) {
        SYSLOGERROR(ERR_S_STAT, s_id, md->md_tempfilepath, E_STR);
        return R_ERROR;
    }

    /* ファイルのパス (/new/....) を作成する */
    snprintf(filename, filename_len, SAVEFILENAME,
                md->md_recvtime, stbuf.st_ino, msy_hostname);

    return R_SUCCESS;
}

/*
 * md_makesavefile
 *
 * 一覧に含まれるディレクトリ配下に、保存ファイルをリンクする
 * ※リンクに失敗した場合はエラーとしない
 *
 * 引数
 *      unsigned int            セッションID
 *      struct maildrop *       maildrop構造体
 *      char *                  保存ファイル名
 *      struct strlist *        ディレクトリ一覧
 *
 * 返り値
 *      なし (リンクに失敗した場合も)
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
        /* リンク先のファイルのフルパスを作成する */
        strset_init(&path);
        if (strset_catstr(&path, p->ss_data.ss_str) == -1 ||
            strset_catstr(&path, filename) == -1) {
            SYSLOGERROR(ERR_S_LIBFUNC, s_id, "strset_catstr", E_STR);
            exit(EXIT_MILTER);
        }

        /* ハードリンクを作成する */
        ret = link(md->md_tempfilepath, path.ss_str);
        if (ret < 0) {
            /* 失敗した場合はログ出力のみ */
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
 * strlist形式の一覧からカンマ区切りの文字列を作成する
 *
 * 引数
 *      unsigned int            セッションID (ログ出力用)
 *      struct strset *         格納先のstrset構造体のポインタ
 *      struct strlist *        一覧の先頭ポインタ
 *
 * 返り値
 *      なし
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
            /* 2つ目以降は ", " で繋げる */
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
 * maildrop構造体を解放する
 *
 * 引数
 *      struct maildrop *       maildrop構造体のポインタ
 *
 * 返り値
 *      なし
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
