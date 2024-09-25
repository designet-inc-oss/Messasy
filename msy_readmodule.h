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

#define IS_BLANK(c) ((c) == ' ' || (c) == '\t')
#define ST_HEAD         0
#define ST_MODNAME      1
#define ST_PATH         2
#define ST_END          3
#define MODSTR          "LoadModule"
#define MODSTRSIZE      sizeof(MODSTR)
#define MAXFUNCNAME     2048
#define ERR_LIB_FILE_OPEN "%s: Cannot open lib file.(%s): (%s)"
#define ERR_CREATE_FUNC_HANDLE "%s: Cannot create func handle.(%s): (%s)"
#define ERR_EXEC_FUNC "%s: Execution of %s went wrong."

/* 許可パスワード文字列*/
#define CHAR_PASSWORD "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

#include <libdgstr.h>
#include <libdgmail.h>
#include <libdgconfig.h>

#include "msy_config.h"

/*****************************************
 *
 * 関数リスト
 *
 *****************************************/
int msy_exec_header(struct mlfiPriv *, char *, char *);
int msy_exec_body(struct mlfiPriv *, u_char *, size_t );
int msy_exec_eoh(struct mlfiPriv *mP);
int msy_exec_eom(struct mlfiPriv *mP);
int msy_exec_abort(struct mlfiPriv *mP);
int read_module_config(char *);
void free_lib_handle();
int set_lib_handle (char *, void *, struct modulehandle **);
