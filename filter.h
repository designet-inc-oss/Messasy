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
 * $RCSfile: filter.h,v $
 * $Revision: 1.12 $
 * $Date: 2009/10/22 04:03:31 $
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
