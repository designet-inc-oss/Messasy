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

#ifndef _UTILS_H_
#define _UTILS_H_

#define OK 0
#define NG 1

#define R_FOUND    1
#define R_NOTFOUND 0

/* 構造体 */
struct strlist {
    struct strset ss_data;
    struct strlist *next;
};
struct strformat {
    char        sf_formatchar;
    char        *sf_replacestr;
};

/* プロトタイプ宣言 */
extern int push_strlist(struct strlist **, struct strlist **, char *);
extern int uniq_push_strlist(struct strlist **, struct strlist **, char *);
extern int search_strlist(struct strlist *, char *);
extern void free_strlist(struct strlist *);
extern struct strlist *make_strlist(void);
extern struct strlist *split_comma (char *);
extern unsigned int get_serial();
extern unsigned int get_sessid();
extern void replace_delimiter(char *, char, char);
extern char *str_replace_format(char *, struct strformat *, int);
extern int attach_strlist(struct strlist **, struct strlist **, struct strlist *);

#endif // _UTILS_H_
