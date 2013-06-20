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

#define _GNU_SOURCE

#define ARGHA _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#define ARGHB _GNU_SOURCE
#undef _GNU_SOURCE
#define ARGHC _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#include <Python.h>
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE ARGHA
#undef ARGHA
#undef _GNU_SOURCE
#define _GNU_SOURCE ARGHB
#undef ARGHB
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE ARGHC
#undef ARGHC

#include "exc.h"

PyObject *DmError = NULL;

int
pydm_exc_init(PyObject *m)
{
	DmError = PyErr_NewException("dm.DmError", PyExc_Exception, NULL);
	if (!DmError)
		return -1;
	Py_INCREF(DmError);
	if (PyModule_AddObject(m, "DmError", DmError) < 0)
		return -1;
	return 0;
}

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
