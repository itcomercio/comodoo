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

#ifndef PYBLOCK_DMRAID_H
#define PYBLOCK_DMRAID_H 1

typedef struct {
	PyObject_HEAD

	struct lib_context *lc;
	PyObject *children;
} PydmraidContextObject;
PyAPI_DATA(PyTypeObject) PydmraidContext_Type;

#define PydmraidContext_Check(op) PyObject_TypeCheck(op, &PydmraidContext_Type)
#define PydmraidContext_CheckExact(op) ((op)->ob_type == &PydmraidContext_Type)

typedef struct {
	PyObject_HEAD

	PydmraidContextObject *ctx;
	PyObject *idx;

	char *path;
	char *serial;
	u_int64_t sectors;
} PydmraidDeviceObject;
PyAPI_DATA(PyTypeObject) PydmraidDevice_Type;

#define PydmraidDevice_Check(op) PyObject_TypeCheck(op, &PydmraidDevice_Type)
#define PydmraidDevice_CheckExact(op) ((op)->ob_type == &PydmraidDevice_Type)

typedef struct {
	PyObject_HEAD

	PydmraidContextObject *ctx;
	PyObject *idx;

	struct raid_dev *rd;
} PydmraidRaidDevObject;
PyAPI_DATA(PyTypeObject) PydmraidRaidDev_Type;

#define PydmraidRaidDev_Check(op) PyObject_TypeCheck(op, &PydmraidRaidDev_Type)
#define PydmraidRaidDev_CheckExact(op) ((op)->ob_type == &PydmraidRaidDev_Type)

typedef struct {
	PyObject_HEAD

	PydmraidContextObject *ctx;
	PyObject *idx;

	struct raid_set *rs;
} PydmraidRaidSetObject;
PyAPI_DATA(PyTypeObject) PydmraidRaidSet_Type;

#define PydmraidRaidSet_Check(op) PyObject_TypeCheck(op, &PydmraidRaidSet_Type)
#define PydmraidRaidSet_CheckExact(op) ((op)->ob_type == &PydmraidRaidSet_Type)

extern PyObject *PydmraidRaidSet_FromContextAndRaidSet(
		PydmraidContextObject *ctx, struct raid_set *rs);

typedef struct {
	PyObject_HEAD

	PydmraidContextObject *ctx;
	PyObject *idx;

	enum lc_lists type;
} PydmraidListObject;
PyAPI_DATA(PyTypeObject) PydmraidList_Type;

#define PydmraidList_Check(op) PyObject_TypeCheck(op, &PydmraidList_Type)
#define PydmraidList_CheckExact(op) ((op)->ob_type == &PydmraidList_Type)

extern PyObject *PydmraidList_FromContextAndType(PydmraidContextObject *ctx,
		enum lc_lists type);

extern PyObject *GroupingError;
#endif /* PYBLOCK_DMRAID_H */

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
