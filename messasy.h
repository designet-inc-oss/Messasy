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

#ifndef _MESSASY_H_
#define _MESSASY_H_

#include <libmilter/mfapi.h>
#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>

#include "msy_config.h"

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

/* グローバル変数 */
#define MAX_HOSTNAME_LEN 255
extern char msy_hostname[MAX_HOSTNAME_LEN + 1];

/* プライベート構造体 */
struct mlfiPriv {
    unsigned int        mlfi_sid;               /* セッションID */
    time_t              mlfi_recvtime;          /* 受信日時 */
    _SOCK_ADDR          mlfi_clientaddr;        /* クライアントアドレス */
    struct strset       mlfi_envfrom;           /* From */

    struct strlist      *mlfi_rcptto_h;         /* To一覧の先頭 */
    struct strlist      *mlfi_rcptto_t;         /* To一覧の末尾 */

    struct strlist      *mlfi_addrmatched_h;    /* 保存対象アドレス一覧の先頭 */
    struct strlist      *mlfi_addrmatched_t;    /* 保存対象アドレス一覧の末尾 */

    struct config       *config;                /* 設定ファイル */
    struct extrapriv    *mlfi_extrapriv;        /* モジュール領域へのポインタ */

    int                 header_existence;      /* header1行目かのフラグ */
};

/* モジュール名とそのプライベートbuf */
struct extrapriv {
    struct extrapriv  *expv_next;
    char              *expv_modulename;
    void              *expv_modulepriv;
};

/* コールバック関数 */
sfsistat mlfi_connect(SMFICTX *, char *, _SOCK_ADDR *);
sfsistat mlfi_envfrom(SMFICTX *, char **);
sfsistat mlfi_envrcpt(SMFICTX *, char **);
sfsistat mlfi_header(SMFICTX *, char *, char *);
sfsistat mlfi_eoh(SMFICTX *);
sfsistat mlfi_body(SMFICTX *, u_char *, size_t);
sfsistat mlfi_eom(SMFICTX *);
sfsistat mlfi_abort(SMFICTX *);
sfsistat mlfi_close(SMFICTX *);

/* その他関数 */
sfsistat mlfi_cleanup(SMFICTX *);
sfsistat eom_cleanup(SMFICTX *);
sfsistat mlfi_freepriv(SMFICTX *);

#endif // _MESSASY_H_
