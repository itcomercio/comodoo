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

#include <stdarg.h>

#include "pyhelpers.h"

PyObject *
pyblock_PyString_FromFormatV(const char *format, va_list vargs)
{
	PyObject *string;
	char *s = NULL;
	int n;

	n = vasprintf(&s, format, vargs);
	if (!s) {
		PyErr_NoMemory();
		return NULL;
	}
	string = PyString_FromStringAndSize(s, n);
	free(s);
	return string;
}


PyObject *
pyblock_PyString_FromFormat(const char *format, ...)
{
	PyObject *ret;
	va_list vargs;

	va_start(vargs, format);
	ret = pyblock_PyString_FromFormatV(format, vargs);
	va_end(vargs);

	return ret;
}

PyObject *
pyblock_PyErr_Format(PyObject *exception, const char *format, ...)
{
	va_list vargs;
	PyObject *string;

	va_start(vargs, format);
	string = pyblock_PyString_FromFormatV(format, vargs);
	PyErr_SetObject(exception, string);
	//Py_XDECREF(string);
	va_end(vargs);
	return NULL;
}

void
pyblock_free_stringv(char **sv)
{
	int i = 0;
	if (!sv)
		return;
	while(sv[i]) {
		free(sv[i]);
		i++;
	}
	free(sv);
}

char **
pyblock_strtuple_to_stringv(PyObject *tup)
{
	char **sv;
	int i,n;

	n = PyTuple_GET_SIZE(tup);

	sv = calloc(n+1, sizeof(char *));
	for (i = 0; i < n; i++) {
		PyObject *str = PyTuple_GET_ITEM(tup, i);

		if (!PyString_Check(str)) {
			PyErr_SetString(PyExc_TypeError,
					"list elements must be strings");
			goto err_freev;
		}

		sv[i] = strdup(PyString_AsString(str));
		if (!sv[i]) {
			PyErr_NoMemory();
			goto err_freev;
		}
	}
	return sv;
err_freev:
	for (i = 0; i < n; i++)
		if (i)
			free(sv[i]);
	free(sv);
	return NULL;
}

/* pyblock's python object to long long converter */
int
pyblock_potoll(PyObject *obj, void *addr)
{
	long long *llval = (long long *)addr;

	if (obj->ob_type->tp_as_number &&
			obj->ob_type->tp_as_number->nb_long) {
		PyObject *o = (obj->ob_type->tp_as_number->nb_long)(obj);
		if (PyErr_Occurred())
			return 0;
		*llval = PyLong_AsLongLong(o);
		return 1;
	}

	/* this is just because I like python's errors better than figuring
	 * good text out on my own ;) */
	if (!PyArg_Parse(obj, "l", llval)) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_AssertionError, "PyArg_Parse failed");
		return 0;
	}

	return 1;
}

int
pyblock_TorLtoT(PyObject *obj, void *addr)
{
	PyObject *result = (PyObject *)addr;

	if (!obj) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_AssertionError, "obj was NULL");
		return 0;
	}

	if (PyTuple_Check(obj)) {
		result = obj;
		return 1;
	}

	if (PyList_Check(obj)) {
		result = PyList_AsTuple(obj);
		return 1;
	}

	result = NULL;
	PyErr_BadArgument();
	return 0;
}

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
