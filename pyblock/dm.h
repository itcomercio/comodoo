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

#ifndef PYBLOCK_DM_H
#define PYBLOCK_DM_H 1

#include <libdevmapper.h>
#ifdef USESELINUX
#include <selinux/selinux.h>
#endif

typedef struct {
	PyObject_HEAD

	dev_t dev;
#ifdef USESELINUX
	security_context_t con;
#endif
	mode_t mode;
} PydmDeviceObject;

PyAPI_DATA(PyTypeObject) PydmDevice_Type;

#define PydmDevice_Check(op) PyObject_TypeCheck(op, &PydmDevice_Type)
#define PydmDevice_CheckExact(op) ((op)->ob_type == &PydmDevice_Type)

PyAPI_FUNC(PyObject *) PydmDevice_FromMajorMinor(u_int32_t major, u_int32_t minor);

typedef struct {
	PyObject_HEAD

	loff_t start;
	u_int64_t size;
	char *type;
	char *params;
} PydmTableObject;

PyAPI_DATA(PyTypeObject) PydmTable_Type;

#define PydmTable_Check(op) PyObject_TypeCheck(op, &PydmTable_Type)
#define PydmTable_CheckExact(op) ((op)->ob_type == &PydmTable_Type)

PyAPI_FUNC(PyObject *) PydmTable_FromInfo(loff_t start, u_int64_t size,
		char *type, char *params);

typedef struct {
	PyObject_HEAD

	int initialized;

	char *name;
	char *uuid;
	PyObject *dev;
	struct dm_info info;
} PydmMapObject;

PyAPI_DATA(PyTypeObject) PydmMap_Type;

#define PydmMap_Check(op) PyObject_TypeCheck(op, &PydmMap_Type)
#define PydmMap_CheckExact(op) ((op)->ob_type == &PydmMap_Type)

typedef struct {
	PyObject_HEAD

	char *name;
	int major;
	int minor;
	int micro;
} PydmTargetObject;

PyAPI_DATA(PyTypeObject) PydmTarget_Type;

#define PydmTarget_Check(op) PyObject_TypeCheck(op, &PydmTarget_Type)
#define PydmTarget_CheckExact(op) ((op)->ob_type == &PydmTarget_Type)

#endif /* PYBLOCK_DM_H */

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
