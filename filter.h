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

#define FROM 1
#define TO   2

/* プロトタイプ宣言 */
extern int make_savelist(struct strset *from,
                         struct strlist *rcptlist,
                         struct strlist **savelist_h,
                         struct strlist **savelist_t,
                         struct config *cfg,
                         unsigned int s_id);
extern int check_header_regex(char *, char *, regex_t *);

#ifndef _FILTER_H_
#define _FILTER_H_


#endif // _FILTER_H_
