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

#ifndef _LIBDUMMY_H_
#define _LIBDUMMY_H_
#include <libdgstr.h>
#include "../messasy.h"
#include "../utils.h"
#include "../log.h"

#define MAILDROP_CFECOUNT        (sizeof(dummy_cfe) / sizeof(struct cfentry))

/**********************************
 * ��¤��
 **********************************/
struct dummy_config {
    char 	   *cf_dummy;
};
/**********************************
 * ��¤��
 **********************************/
/* �⥸�塼���ѤΥץ饤�١���buf */
struct dummy_priv {
    struct dummy     *mypriv; 
};

/**********************************
 * �ؿ��ΰ����ꥹ��
 **********************************/
struct dummy {
    char             *dummy_str;            /* ���ߡ���ʸ���� */
};

/**********************************
 * �ؿ��ꥹ��
 **********************************/

extern int dummy_init(struct cfentry **, size_t *, struct config **, size_t *);
extern int dummy_mod_extra_config(struct config **cfg);

#endif // _LIBDUMMY_H_
