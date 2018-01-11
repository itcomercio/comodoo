/*
 * Copyright 2005-2007 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _PYBLOCK_PYHELPERS_H
#define _PYBLOCK_PYHELPERS_H 1

#define PYBLOCK_ASSERT(cond, message, action)\
	if(!(cond)){\
		PyErr_SetString(PyExc_AssertionError, message);\
		action;\
	}

/* ASSERT functions for dmraid.c */
#define PB_DMRAID_ASSERT_CTX(a_ctx, action)\
	PYBLOCK_ASSERT(a_ctx != NULL, "The pyblock context is NULL.", action);\
	PYBLOCK_ASSERT(a_ctx->lc != NULL, "The dmraid context is NULL.", action)

#define PB_DMRAID_ASSERT_RS(a_rs, action)\
	PYBLOCK_ASSERT(a_rs != NULL, "The pyblock raidset is NULL.", action);\
	PYBLOCK_ASSERT(a_rs->rs != NULL, "The dmraid raidset is NULL.", action)

#define PB_DMRAID_ASSERT_DEV(a_dev, action)\
	PYBLOCK_ASSERT(a_dev != NULL, "The pyblock device is NULL.", action)

#define PB_DMRAID_ASSERT_RSCTX(pb_rs, action)\
	PB_DMRAID_ASSERT_RS(pb_rs, action);\
	PB_DMRAID_ASSERT_CTX(pb_rs->ctx, action)

#define PB_DMRAID_ASSERT_DEVCTX(pb_dev, action)\
	PB_DMRAID_ASSERT_DEV(pb_dev, action);\
	PB_DMRAID_ASSERT_CTX(pb_dev->ctx, action)

/* ASSERT functions for dm.c */
#define PB_DM_ASSERT_DEV(a_dev, action)\
	PYBLOCK_ASSERT(a_dev != NULL, "The pyblock device is NULL.", action)

#define PB_DM_ASSERT_TABLE(a_table, action)\
	PYBLOCK_ASSERT(a_table != NULL, "The pyblock table is NULL.", action)

#define PB_DM_ASSERT_MAP(a_map, action)\
	PYBLOCK_ASSERT(a_map != NULL, "The pyblock map is NULL.", action)

#define PB_DM_ASSERT_TARGET(a_target, action)\
	PYBLOCK_ASSERT(a_target != NULL, "The pyblock target is NULL.", action)

extern PyObject *
pyblock_PyString_FromFormatV(const char *format, va_list)
	__attribute__ ((format(printf, 1, 0)));

extern PyObject *
pyblock_PyString_FromFormat(const char *format, ...)
	__attribute__ ((format(printf, 1, 2)));
	
extern PyObject *
pyblock_PyErr_Format(PyObject *exception, const char *format, ...)
	__attribute__ ((format(printf, 2, 3)));

extern char **
pyblock_strtuple_to_stringv(PyObject *tup);

extern void
pyblock_free_stringv(char **sv);

extern int
pyblock_potoll(PyObject *obj, void *addr);

extern int
pyblock_TorLtoT(PyObject *obj, void *addr);

#endif /* _PYBLOCK_PYHELPERS_H */

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
