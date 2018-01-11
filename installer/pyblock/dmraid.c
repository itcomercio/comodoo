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
#include <stdio.h>

#if 0
#define Py_REF_DEBUG 1
#endif

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


#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#ifdef Py_REF_DEBUG
long _Py_RefTotal = 0;
void
_Py_NegativeRefcount(const char *f, int l, PyObject *o)
{
	fprintf(stderr,"%s: %c: object %p has negative refcount\n", f, l, o);
	assert(0);
}
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/blkpg.h>

#define FORMAT_HANDLER
#include <dmraid/dmraid.h>
#include <dmraid/metadata.h>
#include <dmraid/format.h>
#include <dmraid/list.h>

#define ALLOW_NOSYNC

#include "dm.h"
#include "dmraid.h"

#include "pyhelpers.h"

#define for_each_format(_c, _n) list_for_each_entry(_n, LC_FMT(_c), list)
#define for_each_device(_c, _n) list_for_each_entry(_n, LC_DI(_c), list)
#define for_each_raiddev(_c, _n) list_for_each_entry(_n, LC_RD(_c), list)
#define for_each_raidset(_c, _n) list_for_each_entry(_n, LC_RS(_c), list)
#define for_each_subset(_rs, _n) list_for_each_entry(_n, &(_rs)->sets, list)

#define plist_for_each(_d, _n) list_for_each_entry(_n, \
		lc_list((_d)->ctx->lc, (_d)->type), list)

static void
pydmraid_dev_clear(PydmraidDeviceObject *dev)
{
	if (dev->ctx) {
		PyDict_DelItem(dev->ctx->children, dev->idx);
		Py_DECREF(dev->ctx);
		dev->ctx = NULL;
	}

	if (dev->idx) {
		Py_DECREF(dev->idx);
		dev->idx = NULL;
	}

	if (dev->path) {
		free(dev->path);
		dev->path = NULL;
	}

	if (dev->serial) {
		free(dev->serial);
		dev->serial = NULL;
	}
	dev->sectors = 0;
}

static void
pydmraid_dev_dealloc(PydmraidDeviceObject *dev)
{
	pydmraid_dev_clear(dev);
	PyObject_Del(dev);
}

static int
pydmraid_dev_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"context", "path", NULL};
	PydmraidDeviceObject *dev = (PydmraidDeviceObject *)self;
	PydmraidContextObject *ctx = NULL;
	struct dev_info *di;
	char *path;

	pydmraid_dev_clear(dev);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!s:device.__init__",
			kwlist, &PydmraidContext_Type, &ctx, &path))
		return -1;

	dev->idx = pyblock_PyString_FromFormat("%p", dev);
	if (dev->idx == NULL) {
		PyErr_NoMemory();
		return -1;
	}

	dev->path = strdup(path);
	if (path == NULL) {
		pydmraid_dev_clear(dev);
		PyErr_NoMemory();
		return -1;
	}

	for_each_device(ctx->lc, di) {
		if (strcmp(di->path, path))
			continue;
		
		dev->serial = strdup(di->serial);
		if (!dev->serial) {
			pydmraid_dev_clear(dev);
			PyErr_NoMemory();
			return -1;
		}

		dev->sectors = di->sectors;

		PyDict_SetItem(ctx->children, dev->idx, dev->idx);
		if (PyErr_Occurred()) {
			pydmraid_dev_clear(dev);
			return -1;
		}
		dev->ctx = ctx;
		Py_INCREF(ctx);

		return 0;
	};

	pydmraid_dev_clear(dev);
	PyErr_SetString(PyExc_ValueError, "No such device");
	return -1;
}

static PyObject *
pydmraid_dev_str_method(PyObject *self)
{
	PydmraidDeviceObject *dev = (PydmraidDeviceObject *)self;

	return pyblock_PyString_FromFormat("%s", dev->path);
}

static PyObject *
pydmraid_dev_get(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_DEVCTX(((PydmraidDeviceObject *)self), return NULL);

	PydmraidDeviceObject *dev = (PydmraidDeviceObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "path")) {
		return PyString_FromString(dev->path);
	} else if (!strcmp(attr, "serial")) {
		return PyString_FromString(dev->serial);
	} else if (!strcmp(attr, "sectors")) {
		return PyLong_FromUnsignedLongLong(dev->sectors);
	}
	return NULL;
}

static struct PyGetSetDef pydmraid_dev_getseters[] = {
	{"path", (getter)pydmraid_dev_get, NULL, "path", "path"},
	{"serial", (getter)pydmraid_dev_get, NULL, "serial", "serial"},
	{"sectors", (getter)pydmraid_dev_get, NULL, "sectors", "sectors"},
	{NULL},
};

static PyObject *
pydmraid_dev_rmpart(PyObject *self, PyObject *args, PyObject *kwds)
{
	PydmraidDeviceObject *dev = (PydmraidDeviceObject *)self;
	char *kwlist[] = {"partno", NULL};
	unsigned long long partno;
	struct blkpg_partition part;
	struct blkpg_ioctl_arg io = {
		.op = BLKPG_DEL_PARTITION,
		.datalen = sizeof(part),
		.data = &part,
	};
	int fd;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&:device.rmpart",
			kwlist, pyblock_potoll, &partno))
		return NULL;

	if (!dev->path) {
		pyblock_PyErr_Format(PyExc_RuntimeError, "path not set");
		return NULL;
	}

	fd = open(dev->path, O_RDWR);
	if (fd < 0) {
		PyErr_SetFromErrno(PyExc_SystemError);
		return NULL;
	}

	part.pno = partno;
	ioctl(fd, BLKPG, &io);
	close(fd);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
pydmraid_dev_scanparts(PyObject *self, PyObject *args, PyObject *kwds)
{
	PydmraidDeviceObject *dev = (PydmraidDeviceObject *)self;
	int fd;

	if (!dev->path) {
		pyblock_PyErr_Format(PyExc_RuntimeError, "path not set");
		return NULL;
	}

	fd = open(dev->path, O_RDWR);
	if (fd < 0) {
		PyErr_SetFromErrno(PyExc_SystemError);
		return NULL;
	}

	ioctl(fd, BLKRRPART, 0);
	close(fd);
	Py_INCREF(Py_None);
	return Py_None;

}

static struct PyMethodDef pydmraid_dev_methods[] = {
	{"rmpart",
		(PyCFunction)pydmraid_dev_rmpart,
		METH_VARARGS | METH_KEYWORDS,
		"Removes the partition defined by the partno argument"},
	{"scanparts",
		(PyCFunction)pydmraid_dev_scanparts,
		METH_NOARGS,
		"(Re)scans all partitions for the current device"},
	{NULL},
};

PyTypeObject PydmraidDevice_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dmraid.device",
	.tp_basicsize = sizeof(PydmraidDeviceObject),
	.tp_dealloc = (destructor)pydmraid_dev_dealloc,
	.tp_getset = pydmraid_dev_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydmraid_dev_init_method,
	.tp_str = pydmraid_dev_str_method,
	.tp_methods = pydmraid_dev_methods,
	.tp_new = PyType_GenericNew,
	.tp_doc = "The system device",
};

PyObject *
PydmraidDevice_FromContextAndDevInfo(PydmraidContextObject *ctx,
		struct dev_info *di)
{
	PydmraidDeviceObject *dev;

	dev = PyObject_New(PydmraidDeviceObject, &PydmraidDevice_Type);
	if (!dev)
		return NULL;

	dev->ctx = NULL;
	dev->idx = NULL;
	dev->path = NULL;
	dev->serial = NULL;

	dev->idx = pyblock_PyString_FromFormat("%p", dev);
	if (dev->idx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	dev->path = strdup(di->path);
	if (!dev->path) {
		pydmraid_dev_clear(dev);
		PyErr_NoMemory();
		return NULL;
	}


	dev->serial = strdup(di->serial);
	if (!dev->serial) {
		pydmraid_dev_clear(dev);
		PyErr_NoMemory();
		return NULL;
	}

	dev->sectors = di->sectors;

	/* this must be last */
	PyDict_SetItem(ctx->children, dev->idx, dev->idx);
	if (PyErr_Occurred()) {
		pydmraid_dev_clear(dev);
		return NULL;
	}
	dev->ctx = ctx;
	Py_INCREF(ctx);

	return (PyObject *)dev;
}

/* end device object */

/* begin raiddev object */

static void
pydmraid_raiddev_clear(PydmraidRaidDevObject *dev)
{
	if (dev->ctx) {
		PyDict_DelItem(dev->ctx->children, dev->idx);
		Py_DECREF(dev->ctx);
		dev->ctx = NULL;
	}

	if (dev->idx) {
		Py_DECREF(dev->idx);
		dev->idx = NULL;
	}
	
	/* la dee dah, ignoring dev->di completely for now */
}

static void
pydmraid_raiddev_dealloc(PydmraidRaidDevObject *dev)
{
	pydmraid_raiddev_clear(dev);
	PyObject_Del(dev);
}

static int
pydmraid_raiddev_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"context", NULL};
	PydmraidRaidDevObject *dev = (PydmraidRaidDevObject *)self;
	PydmraidContextObject *ctx = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!:raiddev.__init__",
				kwlist,	&PydmraidContext_Type, &ctx))
		return -1;

	dev->idx = pyblock_PyString_FromFormat("%p", dev);
	if (dev->idx == NULL) {
		PyErr_NoMemory();
		return -1;
	}

	/* la dee dah, ignoring dev->di completely for now */

	/* this must be last */
	PyDict_SetItem(ctx->children, dev->idx, dev->idx);
	if (PyErr_Occurred()) {
		pydmraid_raiddev_clear(dev);
		return -1;
	}
	dev->ctx = ctx;
	Py_INCREF(ctx);

	return 0;
}

static PyObject *
pydmraid_raiddev_str_method(PyObject *self)
{
	PydmraidRaidDevObject *dev = (PydmraidRaidDevObject *)self;

	/* XXX should be set name + member position? */
	return PyString_FromString(dev->rd->di->path);
}

static PyObject *
pydmraid_raiddev_repr_method(PyObject *self)
{
	PydmraidRaidDevObject *dev = (PydmraidRaidDevObject *)self;

	return pyblock_PyString_FromFormat("<raiddev object %s at %p>",
			dev->rd->di->path, dev);
}

static PyObject *
pydmraid_raiddev_get(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_DEVCTX(((PydmraidRaidDevObject *)self), return NULL);

	PydmraidRaidDevObject *dev = (PydmraidRaidDevObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "device")) {
		return PydmraidDevice_FromContextAndDevInfo(dev->ctx,
				dev->rd->di);
	} else if (!strcmp(attr, "set")) {
		return PyString_FromString(dev->rd->name);
	} else if (!strcmp(attr, "status")) {
		return PyString_FromString(
				get_status(dev->ctx->lc, dev->rd->status));
	} else if (!strcmp(attr, "sectors")) {
		if (dev->rd->di)
			return PyLong_FromUnsignedLong(dev->rd->di->sectors);
		else
			return PyLong_FromUnsignedLong(0);
	}
				
	return NULL;
}

static struct PyGetSetDef pydmraid_raiddev_getseters[] = {
	{"device", (getter)pydmraid_raiddev_get, NULL, "device", "device"},
	{"set", (getter)pydmraid_raiddev_get, NULL, "set", "set"},
	{"status", (getter)pydmraid_raiddev_get, NULL, "status", "status"},
	{"sectors", (getter)pydmraid_raiddev_get, NULL, "sectors", "sectors"},
	{NULL},
};

PyTypeObject PydmraidRaidDev_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dmraid.raiddev",
	.tp_basicsize = sizeof(PydmraidRaidDevObject),
	.tp_dealloc = (destructor)pydmraid_raiddev_dealloc,
	.tp_getset = pydmraid_raiddev_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydmraid_raiddev_init_method,
	.tp_str = pydmraid_raiddev_str_method,
	.tp_repr = pydmraid_raiddev_repr_method,
	.tp_new = PyType_GenericNew,
	.tp_doc = "The raid device.  These are devices that contain raid sets.",
};

PyObject *
PydmraidRaidDev_FromContextAndRaidDev(PydmraidContextObject *ctx,
		struct raid_dev *rd)
{
	PydmraidRaidDevObject *dev;


	dev = PyObject_New(PydmraidRaidDevObject, &PydmraidRaidDev_Type);
	if (!dev) {
		return NULL;
	}

	dev->idx = pyblock_PyString_FromFormat("%p", dev);
	if (dev->idx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	dev->rd = rd;

	/* this must be last */
	PyDict_SetItem(ctx->children, dev->idx, dev->idx);
	if (PyErr_Occurred()) {
		pydmraid_raiddev_clear(dev);
		return NULL;
	}
	dev->ctx = ctx;
	Py_INCREF(ctx);

	return (PyObject *)dev;
}

/* end raiddev object */

/* begin raidset object */

static void
pydmraid_raidset_clear(PydmraidRaidSetObject *set)
{
	if (set->ctx) {
		PyDict_DelItem(set->ctx->children, set->idx);
		Py_DECREF(set->ctx);
		set->ctx = NULL;
	}

	if (set->idx) {
		Py_DECREF(set->idx);
		set->idx = NULL;
	}
}

static void
pydmraid_raidset_dealloc(PydmraidRaidSetObject *set)
{
	pydmraid_raidset_clear(set);
	PyObject_Del(set);
}

static int
pydmraid_raidset_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"context", NULL};
	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	PydmraidContextObject *ctx = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!:raidset.__init__",
				kwlist,	&PydmraidContext_Type, &ctx))
		return -1;

	set->idx = pyblock_PyString_FromFormat("%p", set);
	if (set->idx == NULL) {
		PyErr_NoMemory();
		return -1;
	}

	/* la dee dah, ignoring set->di completely for now */

	/* this must be last */
	PyDict_SetItem(ctx->children, set->idx, set->idx);
	if (PyErr_Occurred()) {
		pydmraid_raidset_clear(set);
		return -1;
	}
	set->ctx = ctx;
	Py_INCREF(ctx);

	return 0;
}

static PyObject *
pydmraid_raidset_str_method(PyObject *self)
{
	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;

	return PyString_FromString(set->rs->name);
}

static PyObject *
pydmraid_raidset_repr_method(PyObject *self)
{
	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;

	return pyblock_PyString_FromFormat("<raidset object %s at %p>",
			set->rs->name, set);
}

static PyObject *
pydmraid_raidset_get_children(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_RSCTX(((PydmraidRaidSetObject *)self), return NULL);

	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	struct lib_context *lc = set->ctx->lc;
	PyObject *t;
	struct raid_set *rs = set->rs;
        Py_ssize_t n;
	unsigned int i = 0;

	if (!list_empty(&rs->sets)) {
		struct raid_set *nrs;
		PyObject *o;

		n = count_sets(lc, &rs->sets);
		t = PyTuple_New(n);

		list_for_each_entry(nrs, &rs->sets, list) {
			o = PydmraidRaidSet_FromContextAndRaidSet(set->ctx,nrs);
			if (!o) {
				Py_DECREF(t);
				return NULL;
			}

			Py_INCREF(o);
			if (PyTuple_SetItem(t, i++, o) < 0) {
				Py_DECREF(o);
				Py_DECREF(t);
				return NULL;
			}
		}
		Py_INCREF(t);
		return t;
	} else if (!list_empty(&rs->devs)) {
		struct raid_dev *rd;
		PyObject *o;

		n = count_devs(lc, rs, ct_all);
		t = PyTuple_New(n);

		list_for_each_entry(rd, &rs->devs, devs) {
			o = PydmraidRaidDev_FromContextAndRaidDev(set->ctx, rd);
			if (!o) {
				Py_DECREF(t);
				return NULL;
			}

			Py_INCREF(o);
			if (PyTuple_SetItem(t, i++, o) < 0) {
				Py_DECREF(o);
				Py_DECREF(t);
				return NULL;
			}
		}
		Py_INCREF(t);
		return t;
	}
	PyErr_SetString(PyExc_AssertionError, "should not get here");
	return NULL;
}

static PyObject *
pydmraid_raidset_get_spares(PyObject *self, void *data)
{
	/* XXX FIXME write this code */
	return PyList_New(0);
}

static PyObject *
pydmraid_raidset_get_table(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_RSCTX(((PydmraidRaidSetObject *)self), return NULL);

	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	struct lib_context *lc = set->ctx->lc;
	struct raid_set *rs = set->rs;
	char *table = NULL;;
	PyObject *o;
#ifndef ALLOW_NOSYNC
	char *a, *b;
	ssize_t spn;
#endif

	/* ugh, there's no generic way of telling if this should even
	   have a table*/
	table = libdmraid_make_table(lc, rs);
	if (!table) {
		PyErr_SetString(PyExc_RuntimeError, "no mapping possible");
		return NULL;
	}

#ifndef ALLOW_NOSYNC
	/* XXX FIXME we shouldn't need to do this */
	/* transform "core 2 64 nosync" into "core 1 64" */
	a = strstr(table, "core");
	if (a) {
		/* find the first space */
		a += strcspn(a, " ");
		/* find the first nonspace */
		a += strspn(a, " ");
		/* if it's not 2, don't change anything */
		if (!strncmp(a, "2 ", 2)) {
			/* change "2 " to a "1 " */
			a[0]='1';
			a+=2;
			/* find the end of this arg (the space after "64") */
			a += strcspn(a, " ");
			/* .. and the arg we want rid of */
			a += strspn(a, " ");
			/* now find the beginning of our replacement */
			b = a + strcspn(a, " ");
			b += strspn(b, " ");
			/* and now clobber it */
			spn = strlen(b);
			memmove(a, b, spn+1);
		}
	}
#endif

	o = PyString_FromString(table);
	free(table);
	return o;
}

static PyObject *
pydmraid_raidset_get_dm_table(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_RSCTX(((PydmraidRaidSetObject *)self), return NULL);

	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	struct lib_context *lc = set->ctx->lc;
	struct raid_set *rs = set->rs;

	char *s = NULL, *a, *b;
	char *dmtype = NULL, *rule = NULL;
	u_int64_t start, length;
	size_t spn;

	PyObject *m = NULL, *md = NULL;
	PyObject *type = NULL;
	PyObject *table = NULL;
	PyObject *table_list = NULL;
	PyObject *iret = NULL;

	/* ugh, there's no generic way of telling if this should even
	   have a table*/
	a = b = libdmraid_make_table(lc, rs);
	if (a) {
		s = strdupa(a);
		free(a);
	}
	if (!s) {
		PyErr_SetString(PyExc_RuntimeError, "no mapping possible");
		return NULL;
	}

	/* get the DSO for block.dm loaded */
	m = PyImport_ImportModule("block.dm");
	if (!m)
		goto out;

	md = PyModule_GetDict(m);
	if (!md)
		goto out;

	type = PyDict_GetItemString(md, "table");
	if (!type)
		goto out;

	table_list = PyList_New(0);
	if (!table_list)
		goto out;

	s = strtok(s, "\n");
	do {
		a = b = s;
		/* find the beginning of the start */
		spn = strspn(s, " \t");
		a += spn;
		if (*a == '\0')
			goto out;

		b = NULL;
		errno = 0;
		start = strtoull(a, &b, 10);
		if (start == ULLONG_MAX && errno != 0)
			goto out;

		/* find the length */
		spn = strspn(b, " \t");
		a = b + spn;
		if (!spn || *a == '\0')
			goto out;

		b = NULL;
		errno = 0;
		length = strtoull(a, &b, 10);
		if (length == ULLONG_MAX && errno != 0)
			goto out;

		/* now the dm type */
		spn = strspn(b, " \t");
		a = b + spn;
		if (!spn || *a == '\0')
			goto out;

		spn = strcspn(a, " \t");
		if (!spn)
			goto out;
		dmtype = strndupa(a, spn);

		a += spn;
		spn = strspn(a, " \t");
		a += spn;
		if (a)
			rule=strdupa(a);

#ifndef ALLOW_NOSYNC
		/* XXX FIXME we shouldn't need to do this */
		/* transform "core 2 64 nosync" into "core 1 64" */
		a = strstr(rule, "core");
		if (a) {
			/* find the first space */
			a += strcspn(a, " ");
			/* find the first nonspace */
			a += strspn(a, " ");
			/* if it's not 2, don't change anything */
			if (!strncmp(a, "2 ", 2)) {
				/* change "2 " to a "1 " */
				a[0]='1';
				a+=2;
				/* find the end of this arg (the space after "64") */
				a += strcspn(a, " ");
				/* .. and the arg we want rid of */
				a += strspn(a, " ");
				/* now find the beginning of our replacement */
				b = a + strcspn(a, " ");
				b += strspn(b, " ");
				/* and now clobber it */
				spn = strlen(b);
				memmove(a, b, spn+1);
			}
		}
#endif
		table = PyType_GenericNew((PyTypeObject *)type, NULL, NULL);
		if (!table)
			goto out;

		iret = PyObject_CallMethod(table, "__init__", "LLss", start, length, dmtype, rule);
		if (!iret)
			goto out;

		if (PyList_Append(table_list, table) < 0)
			goto out;

		Py_CLEAR(iret);
		Py_CLEAR(table);
	} while ((s = strtok(NULL, "\n")));

	return table_list;

out:
	Py_XDECREF(iret);
	Py_XDECREF(table);
	Py_XDECREF(table_list);
	if (!PyErr_Occurred()) {
		if (errno != 0)
			PyErr_SetFromErrno(PyExc_SystemError);
		else
			pyblock_PyErr_Format(PyExc_ValueError,
					"invalid map '%s'", s);
	}
	return NULL;
}

static PyObject *
pydmraid_raidset_get_map(PyObject *self, void *data)
{
	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	struct raid_set *rs = set->rs;

	PyObject *m = NULL, *md = NULL;
	PyObject *type = NULL;
	PyObject *map = NULL, *table = NULL;

	PyObject *iret = NULL;

	table = pydmraid_raidset_get_dm_table(self, NULL);
	if (!table)
		goto out;

	m = PyImport_ImportModule("block.dm");
	if (!m)
		goto out;

	md = PyModule_GetDict(m);
	if (!md)
		goto out;

	type = PyDict_GetItemString(md, "map");
	if (!type)
		goto out;

	map = PyType_GenericNew((PyTypeObject *)type, NULL, NULL);
	if (!map)
		goto out;

	iret = PyObject_CallMethod(map, "__init__", "sO", rs->name, table);
	if (!iret) {
		Py_CLEAR(map);
		goto out;
	}

out:
	Py_XDECREF(iret);
	Py_XDECREF(table);
	if (!map && !PyErr_Occurred()) {
		if (errno != 0)
			PyErr_SetFromErrno(PyExc_SystemError);
		else
			pyblock_PyErr_Format(PyExc_ValueError,
					"invalid map '%s'", rs->name);
	}
	return map;
}

static PyObject *
pydmraid_raidset_get(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_RSCTX(((PydmraidRaidSetObject *)self), return NULL);

	PydmraidRaidSetObject *set = (PydmraidRaidSetObject *)self;
	struct lib_context *lc = set->ctx->lc;
	struct raid_set *rs = set->rs;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "name")) {
		return PyString_FromString(rs->name);
	} else if (!strcmp(attr, "type")) {
		return PyString_FromString(get_type(lc, rs->type));
	} else if (!strcmp(attr, "dmtype")) {
		const char *dmtype;
		
		dmtype = get_dm_type(lc, rs->type);
		if (dmtype == NULL) {
			Py_INCREF(Py_None);
			return Py_None;
		}
		return PyString_FromString(dmtype);
	} else if (!strcmp(attr, "set_type")) {
		return PyString_FromString(get_set_type(lc, rs));
	} else if (!strcmp(attr, "status")) {
		return PyString_FromString(get_status(lc, rs->status));
	} else if (!strcmp(attr, "sectors")) {
		return PyLong_FromUnsignedLongLong(
				total_sectors(lc, rs));
	} else if (!strcmp(attr, "total_devs")) {
		return PyLong_FromUnsignedLong(rs->total_devs);
	} else if (!strcmp(attr, "found_devs")) {
		return PyLong_FromUnsignedLong(rs->found_devs);
	} else if (!strcmp(attr, "degraded")) {
		PyObject *ret = Py_False;
		if (S_INCONSISTENT(rs->status))
			ret = Py_True;
		Py_INCREF(ret);
		return ret;
	} else if (!strcmp(attr, "broken")) {
		PyObject *ret = Py_False;
		if (S_BROKEN(rs->status))
			ret = Py_True;
		Py_INCREF(ret);
		return ret;
	}

	PyErr_SetString(PyExc_AssertionError, "should not get here");
	return NULL;
}

static int
pydmraid_raidset_set(PyObject *self, PyObject *value, void *data)
{
	const char *attr = (const char *)data;
	int ret = 0;
	
	if (!strcmp(attr, "name")) {
		PyObject *map;
		PyObject *name;

		map = pydmraid_raidset_get_map(self, NULL);
		if (!map)
			return -1;

		name = PyString_FromString("name");
		if (!name) {
			Py_DECREF(map);
			return -1;
		}

		ret = PyObject_GenericSetAttr(map, name, value);
		Py_DECREF(name);
		Py_DECREF(map);
		/* XXX need to repoll dmraid name */
	}

	return ret;
}

static struct PyGetSetDef pydmraid_raidset_getseters[] = {
	{"children", (getter)pydmraid_raidset_get_children, NULL,
		"children","children"},
	{"spares", (getter)pydmraid_raidset_get_spares, NULL, "spares", "spares"},
	{"table", (getter)pydmraid_raidset_get_table, NULL, "table", "table"},
	{"dmTable", (getter)pydmraid_raidset_get_dm_table, NULL, "block.dm.Table", "block.dm.Table"},
	{"name", (getter)pydmraid_raidset_get, (setter)pydmraid_raidset_set,
		"name", "name"},
	{"type", (getter)pydmraid_raidset_get, NULL, "type", "type"},
	{"dmtype", (getter)pydmraid_raidset_get, NULL, "dmtype", "dmtype"},
	{"set_type", (getter)pydmraid_raidset_get, NULL, "set_type","set_type"},
	{"status", (getter)pydmraid_raidset_get, NULL, "status", "status"},
	{"sectors", (getter)pydmraid_raidset_get, NULL, "sectors", "sectors"},
	{"total_devs", (getter)pydmraid_raidset_get, NULL, "total_devs", "total_devs"},
	{"found_devs", (getter)pydmraid_raidset_get, NULL, "found_devs", "found_devs"},
	{"degraded", (getter)pydmraid_raidset_get, NULL, "degraded", "degraded"},
	{"broken", (getter)pydmraid_raidset_get, NULL, "broken", "broken"},
	{NULL},
};

PyTypeObject PydmraidRaidSet_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dmraid.raidset",
	.tp_basicsize = sizeof(PydmraidRaidSetObject),
	.tp_dealloc = (destructor)pydmraid_raidset_dealloc,
	.tp_getset = pydmraid_raidset_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydmraid_raidset_init_method,
	.tp_str = pydmraid_raidset_str_method,
	.tp_repr = pydmraid_raidset_repr_method,
	.tp_new = PyType_GenericNew,
	.tp_doc = "The raid set.  Metadata for dmraid devices.",
};

PyObject *
PydmraidRaidSet_FromContextAndRaidSet(PydmraidContextObject *ctx,
		struct raid_set *rs)
{
	PydmraidRaidSetObject *set;

	set = PyObject_New(PydmraidRaidSetObject, &PydmraidRaidSet_Type);
	if (!set) {
		return NULL;
	}

	set->idx = pyblock_PyString_FromFormat("%p", set);
	if (set->idx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	set->rs = rs;

	/* this must be last */
	PyDict_SetItem(ctx->children, set->idx, set->idx);
	if (PyErr_Occurred()) {
		pydmraid_raidset_clear(set);
		return NULL;
	}
	set->ctx = ctx;
	Py_INCREF(ctx);

	return (PyObject *)set;
}

/* end raidset object */

/* begin context object */
static void
pydmraid_ctx_clear(PydmraidContextObject *ctx)
{
	if (ctx->lc) {
		libdmraid_exit(ctx->lc);
		ctx->lc = NULL;
	}
	if (ctx->children) {
		PyDict_Clear(ctx->children);
		Py_DECREF(ctx->children);
		ctx->children = NULL;
	}
}

static void
pydmraid_ctx_dealloc(PydmraidContextObject *ctx)
{
	pydmraid_ctx_clear(ctx);
	PyObject_Del(ctx);
}

static int
pydmraid_ctx_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {NULL};
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	const char * const argv[] = {"block.dmraid", NULL};
	
	pydmraid_ctx_clear(ctx);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, ":context.__init__",
				kwlist))
		return -1;

	ctx->lc = libdmraid_init(1, (char **)argv);
	if (!ctx->lc) {
		PyErr_NoMemory();
		return -1;
	}

#if 0
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);
	lc_inc_opt(ctx->lc, LC_VERBOSE);

	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
	lc_inc_opt(ctx->lc, LC_DEBUG);
#endif

	ctx->children = PyDict_New();
	if (!ctx->children) {
		pydmraid_ctx_clear(ctx);
		PyErr_NoMemory();
		return -1;
	}
	return 0;
}

static PyObject *
pydmraid_ctx_get(PyObject *self, void *data)
{
	PB_DMRAID_ASSERT_CTX(((PydmraidContextObject *)self), return NULL);

	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "disks")) {
		return PydmraidList_FromContextAndType(ctx, LC_DISK_INFOS);
	} else if (!strcmp(attr, "raiddevs")) {
		return PydmraidList_FromContextAndType(ctx, LC_RAID_DEVS);
	} else if (!strcmp(attr, "raidsets")) {
		return PydmraidList_FromContextAndType(ctx, LC_RAID_SETS);
	}
	return NULL;
}

static struct PyGetSetDef pydmraid_ctx_getseters[] = {
#if 0
	{"formats", (getter)pydmraid_ctx_get, NULL, "formats", "formats"},
#endif
	{"disks", (getter)pydmraid_ctx_get, NULL, "disks", "disks"},
	{"raiddevs", (getter)pydmraid_ctx_get, NULL, "raiddevs", "raiddevs"},
	{"raidsets", (getter)pydmraid_ctx_get, NULL, "raidsets", "raidsets"},
	{NULL},
};

#if 0
static PyObject *
pydmraid_ctx_count_raidsets(PyObject *self)
{
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	long int x;
	PyObject *ret;
	
	x = count_devices(ctx->lc, SET);
	ret = PyLong_FromLong(x);
	if (!ret)
		return NULL;
	return ret;
}
#endif

static PyObject *
pydmraid_ctx_discover_disks(PyObject *self, PyObject *args, PyObject *kwds)
{
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	char *kwlist[] = {"devices", NULL};
	PyObject *tup = NULL;
	char **sv = NULL;
	int rc;
	unsigned long n;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
			"|O&:discover_disks", kwlist,
			pyblock_TorLtoT, &tup)) {
		if (PyTuple_Check(args) && PyTuple_Size(args) > 0) {
			PyObject *first = PyTuple_GetItem(args, 0);
			if (!PyString_Check(first))
				return NULL;
			PyErr_Clear();
			tup = args;
		} else
			return NULL;
	}

	if (tup && PyTuple_Size(tup) > 0) {
		sv = pyblock_strtuple_to_stringv(tup);
		if (sv == NULL)
			return NULL;
	}

	rc = discover_devices(ctx->lc, sv);
	pyblock_free_stringv(sv);
	if (!rc) {
		PyErr_SetString(PyExc_RuntimeError,
				"discover_devices() returned error\n");
		return NULL;
	}

	n = count_devices(ctx->lc, DEVICE);
	return PyLong_FromUnsignedLong(n);
}

static PyObject *
pydmraid_ctx_discover_raiddevs(PyObject *self, PyObject *args, PyObject *kwds)
{
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	char *kwlist[] = {"devices", NULL};
	PyObject *tup = NULL;
	char **sv = NULL;
	unsigned int n;

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
			"|O&:discover_raiddevs", kwlist,
			pyblock_TorLtoT, &tup)) {
		if (PyTuple_Check(args) && PyTuple_Size(args) > 0) {
			PyObject *first = PyTuple_GetItem(args, 0);
			if (!PyString_Check(first))
				return NULL;
			PyErr_Clear();
			tup = args;
		} else
			return NULL;
	}

	if (tup && PyTuple_Size(tup) > 0) {
		sv = pyblock_strtuple_to_stringv(tup);
		if (sv == NULL)
			return NULL;
	}

	discover_raid_devices(ctx->lc, sv);
	pyblock_free_stringv(sv);

	n = count_devices(ctx->lc, RAID);
	count_devices(ctx->lc, NATIVE);
	return PyLong_FromUnsignedLong(n);
}

static PyObject *
pydmraid_ctx_discover_raidsets(PyObject *self)
{
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	int n;
	char *argv[] = { NULL };

	if (!count_devices(ctx->lc, RAID)) {
		return PyLong_FromLong(0); 
	}

	if (!group_set(ctx->lc, argv)) {
		pyblock_PyErr_Format(GroupingError, "group_set failed");
		return NULL;
	}
	n = count_devices(ctx->lc, SETS);
	return PyLong_FromUnsignedLong(n);
}

static PyObject *
pydmraid_ctx_get_raidsets(PyObject *self, PyObject *args, PyObject *kwds)
{
	PydmraidContextObject *ctx = (PydmraidContextObject *)self;
	PyObject *num;
	int n;

	num = pydmraid_ctx_discover_disks(self, args, kwds);
	if (!num)
		return NULL;
	n = PyLong_AsLong(num);
	Py_DECREF(num);
	if (n <= 0) 
		goto out;

	num = pydmraid_ctx_discover_raiddevs(self, args, kwds);
	if (!num)
		return NULL;
	n = PyLong_AsLong(num);
	Py_DECREF(num);
	if (n <= 0)
		goto out;
	
	num = pydmraid_ctx_discover_raidsets(self);
	if (!num)
		return NULL;
	Py_DECREF(num);
out:
	return PydmraidList_FromContextAndType(ctx, LC_RAID_SETS);
}

static struct PyMethodDef pydmraid_ctx_methods[] = {
	{"discover_disks",
		(PyCFunction)pydmraid_ctx_discover_disks,
		METH_VARARGS | METH_KEYWORDS,
		"Discover all disks in the system. Expects a list of "
		"filesystem nodes where the disks should be searched.  "
		"It will search in sysfs if no list is given.  Returns "
		"number of disks found."},
	{"discover_raiddevs",
		(PyCFunction)pydmraid_ctx_discover_raiddevs,
		METH_VARARGS | METH_KEYWORDS,
		"Identifies which disks are part of a raid set. Expects a "
		"list of devices and returns the number of raid devices "
		"found.  It will search all devices if no list is provided."},
	{"discover_raidsets",
		(PyCFunction)pydmraid_ctx_discover_raidsets,
		METH_NOARGS,
		"Discovers the raid sets in the system"},
	{"get_raidsets",
		(PyCFunction)pydmraid_ctx_get_raidsets,
		METH_VARARGS | METH_KEYWORDS,
		"Returns a list of raid sets in the system.  This "
		"function identifies all the disks in the system, identifies "
		"what disks are part of raid sets and identifies these raid "
		"sets.  It expects a list of devices and/or nodes where to "
		"search for disks and raid devs.  If not list is provided all "
		"the raidsets in the system are returned."},
	{NULL},
};

PyTypeObject PydmraidContext_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dmraid.context",
	.tp_basicsize = sizeof(PydmraidContextObject),
	.tp_dealloc = (destructor)pydmraid_ctx_dealloc,
	.tp_getset = pydmraid_ctx_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydmraid_ctx_init_method,
	.tp_methods = pydmraid_ctx_methods,
	.tp_new = PyType_GenericNew,
	.tp_doc =	"The dmraid context.  Is needed for all the dmraid "
			"actions. It mainly contains the dmraid lib context.",
};

static int
pydmraid_ctx_remove_list(PydmraidListObject *plist)
{
	PyObject *idx = pyblock_PyString_FromFormat("%p", plist);

	if (!idx) {
		PyErr_NoMemory();
		return -1;
	}

	PyDict_DelItem(plist->ctx->children, idx);
	Py_DECREF(idx);
	if (PyErr_Occurred())
		return -1;

	Py_DECREF(plist->ctx);
	plist->ctx = NULL;
	return 0;
}

static int
pydmraid_ctx_add_list(PydmraidContextObject *ctx,
		PydmraidListObject *plist)
{
	PyObject *idx = pyblock_PyString_FromFormat("%p", plist);
	PyObject *o = NULL;

	if (!idx) {
		PyErr_NoMemory();
		return -1;
	}

	o = PyDict_GetItem(ctx->children, idx);
	if (o) {
		Py_DECREF(idx);
		PyErr_SetString(PyExc_AssertionError,
				"device list is already	associated");
		return -1;
	} else if (PyErr_Occurred()) {
		PyErr_Clear();
	}

	if (PyDict_SetItem(ctx->children, idx, idx) < 0) {
		Py_DECREF(idx);
		return -1;
	}

	Py_DECREF(idx);
	Py_INCREF(ctx);
	plist->ctx = ctx;
	return 0;
}



/* end context object */

/* begin device list */

static int
pydmraid_list_clear(PydmraidListObject *plist)
{
	if (plist->ctx) {
		if (pydmraid_ctx_remove_list(plist) < 0)
			return -1;

		plist->ctx = NULL;
	}
	plist->type = LC_LISTS_SIZE;
	return 0;
}

static void
pydmraid_list_dealloc(PydmraidListObject *plist)
{
	pydmraid_list_clear(plist);
	PyObject_Del(plist);
}

static int
pydmraid_list_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"context", "type", NULL};
	PydmraidListObject *plist = (PydmraidListObject *)self;
	PydmraidContextObject *ctx = NULL;
	int list_type;

	pydmraid_list_clear(plist);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!l:list.__init__",
			kwlist, &PydmraidContext_Type, &ctx, &list_type))
		return -1;

	if (list_type < 0 || list_type >= LC_LISTS_SIZE) {
		PyErr_SetString(PyExc_ValueError, "invalid device list type");
		return -1;
	}
	if (plist->type == LC_FORMATS) {
		PyErr_SetString(PyExc_NotImplementedError, "sorry");
		return -1;
	}
	if (pydmraid_ctx_add_list(ctx, plist) < 0) {
		return -1;
	}

	plist->type = list_type;

	return 0;
}

static Py_ssize_t
pydmraid_list_len(PyObject *self)
{
	PydmraidListObject *plist = (PydmraidListObject *)self;
	int n = 0;

	if (plist->type == LC_LISTS_SIZE) {
		PyErr_SetString(PyExc_RuntimeError, "list is not initialized");
		return -1;
	}
	
	switch (plist->type) {
		case LC_DISK_INFOS:
		{
			struct dev_info *di;

			for_each_device(plist->ctx->lc, di)
				n++;
			return n;
		}
		case LC_RAID_DEVS:
		{
			struct raid_dev *rd;
			
			for_each_raiddev(plist->ctx->lc, rd)
				n++;
			return n;
		}
		case LC_RAID_SETS:
		{
			struct raid_set *rs;
			for_each_raidset(plist->ctx->lc, rs) {
				struct raid_set *subset;
				if (T_GROUP(rs)) {
					for_each_subset(rs, subset)
						n++;
				} else {
					n++;
				}
			}
			return n;
		}
		case LC_FORMATS:
		default:
			PyErr_SetString(PyExc_NotImplementedError, "sorry");
			return -1;
	}
	PyErr_SetString(PyExc_AssertionError, "should never get here");
	return -1;
}

static PyObject *
pydmraid_list_item(PyObject *self, Py_ssize_t i)
{
	PydmraidListObject *plist = (PydmraidListObject *)self;
	int n = 0;

	if (plist->type == LC_LISTS_SIZE) {
		PyErr_SetString(PyExc_RuntimeError, "list is not initialized");
		return NULL;
	}

	switch (plist->type) {
		case LC_DISK_INFOS:
		{
			struct dev_info *di;

			plist_for_each(plist, di) {
				if (n++ != i)
					continue;
				return PydmraidDevice_FromContextAndDevInfo(
						plist->ctx, di);
			}
			PyErr_SetString(PyExc_IndexError,
					"list index out of range");
			return NULL;
		}
		case LC_RAID_DEVS:
		{
			struct raid_dev *rd;

			plist_for_each(plist, rd) {
				if (n++ != i)
					continue;
				return PydmraidRaidDev_FromContextAndRaidDev(
						plist->ctx, rd);
			}
			PyErr_SetString(PyExc_IndexError,
					"list index out of range");
			return NULL;
		}
		case LC_RAID_SETS:
		{
			struct raid_set *rs;

			plist_for_each(plist, rs) {
				if (T_GROUP(rs)) {
					struct raid_set *subset;
					for_each_subset(rs, subset) {
						if (n++ != i)
							continue;
						return PydmraidRaidSet_FromContextAndRaidSet(plist->ctx, subset);
					}
				} else {
					if (n++ != i)
						continue;
					return PydmraidRaidSet_FromContextAndRaidSet(plist->ctx, rs);
				}
			}
			PyErr_SetString(PyExc_IndexError,
					"list index out of range");
			return NULL;
		}
		case LC_FORMATS:
		default:
			PyErr_SetString(PyExc_NotImplementedError, "sorry");
			return NULL;
	}
	PyErr_SetString(PyExc_AssertionError, "should never get here");
	return NULL;
}

static int
pydmraid_rs_has_child(struct raid_set *rs, char *name)
{
	struct raid_dev *rd = NULL;
	struct raid_set *crs = NULL;

	if (!strcmp(rs->name, name))
		return 1;

	list_for_each_entry(rd, &rs->devs, list) {
		if (!strcmp(name, rd->name))
			return 1;
	}

	list_for_each_entry(crs, &rs->sets, list) {
		if (pydmraid_rs_has_child(crs, name))
			return 1;
	}
	return 0;
}

static int
pydmraid_list_contains(PyObject *self, PyObject *v)
{
	PydmraidListObject *plist = (PydmraidListObject *)self;
	PydmraidDeviceObject *dev = NULL;
	PydmraidRaidSetObject *set = NULL;
	PydmraidRaidDevObject *raiddev = NULL;
	char *name = NULL;

	if (PydmraidDevice_CheckExact(v)) {
		dev = (PydmraidDeviceObject *)v;
		name = dev->path;
	} else if (PydmraidRaidDev_CheckExact(v)) {
		raiddev = (PydmraidRaidDevObject *)v;
		name = raiddev->rd->name;
	} else if (PydmraidRaidSet_CheckExact(v)) {
		set = (PydmraidRaidSetObject *)v;
		name = set->rs->name;
	} else if (PyString_Check(v)) {
		name = PyString_AsString(v);
	} else {
		return 0;
	}

	if (plist->type == LC_LISTS_SIZE) {
		PyErr_SetString(PyExc_RuntimeError,
				"list is not initialized");
		return -1;
	}

	switch (plist->type) {
		case LC_DISK_INFOS:
		{
			PydmraidDeviceObject *dev = (PydmraidDeviceObject *)v;
			struct dev_info *di;

			plist_for_each(plist, di) {
				if (dev) {
					if (!strcmp(di->path, dev->path) &&
					    !strcmp(di->serial, dev->serial) &&
					    di->sectors == dev->sectors)
						return 1;
				} else if (name) {
					if (!strcmp(di->path, name))
						return 1;
				}
			}
			return 0;
		}
		case LC_RAID_SETS:
		{
			struct raid_set *rs;

			plist_for_each(plist, rs) {
				struct raid_set *subset;
				if (T_GROUP(rs)) {
					for_each_subset(rs, subset) {
						if (pydmraid_rs_has_child(rs,
								name))
							return 1;
					}
				} else {
					if (pydmraid_rs_has_child(rs, name))
						return 1;
				}
			}
			return 0;
		}
		case LC_RAID_DEVS:
		{
			struct raid_dev *rd;

			plist_for_each(plist, rd) {
				if (!strcmp(rd->name, name))
					return 1;
			}
			return 0;
		}
		case LC_FORMATS:
		default:
			PyErr_SetString(PyExc_NotImplementedError, "sorry");
			return -1;
	}
	return 0;
}

static PySequenceMethods pydmraid_list_as_sequence = {
	.sq_length = pydmraid_list_len, /* len(x) */
	.sq_item = pydmraid_list_item, /* x[i] */
	.sq_contains = pydmraid_list_contains, /* i in x */
};

PyTypeObject PydmraidList_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dmraid.list",
	.tp_basicsize = sizeof(PydmraidListObject),
	.tp_dealloc = (destructor)pydmraid_list_dealloc,
//	.tp_getset = pydmraid_list_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydmraid_list_init_method,
	.tp_new = PyType_GenericNew,
};

PyObject *PydmraidList_FromContextAndType(PydmraidContextObject *ctx,
		enum lc_lists type)
{
	PyObject *self;
	PydmraidListObject *plist;

	if (type < 0 || type >= LC_LISTS_SIZE) {
		PyErr_SetString(PyExc_ValueError, "invalid device list type");
		return NULL;;
	}

	if (!PydmraidContext_Check(ctx)) {
		PyErr_SetString(PyExc_ValueError, "invalid context");
		return NULL;;
	}

	self = PydmraidList_Type.tp_new(&PydmraidList_Type, NULL, NULL);
	if (!self)
		return NULL;
	plist = (PydmraidListObject *)self;

	pydmraid_list_clear(plist);

	if (pydmraid_ctx_add_list(ctx, plist) < 0) {
		Py_DECREF(self);
		return NULL;
	}
	plist->type = type;

	return self;
}

/* end device list */

static PyMethodDef pydmraid_functions[] = {
	{NULL, NULL},
};

PyObject *GroupingError = NULL;

static int
pydmraid_init_grouping_error(PyObject *m)
{
	PyObject *d = NULL, *r = NULL;

	d = PyDict_New();
	if (!d)
		goto out;
	r = PyRun_String(
		"def __init__(self, *args): self.args=args\n\n"
		"def __str__(self):\n"
		"  return self.args and ('%s' % self.args[0]) or '(what)'\n",
		Py_file_input, m, d);
	if (!r)
		goto out;
	Py_DECREF(r);

	GroupingError = PyErr_NewException("block.dmraid.GroupingError",
			PyExc_Exception, d);
	Py_INCREF(d);
	if (!GroupingError)
		goto out;
	Py_DECREF(d);
	Py_DECREF(d);
	PyModule_AddObject(m, "GroupingError", GroupingError);
	return 0;
out:
	Py_XDECREF(d);
	Py_XDECREF(GroupingError);
	return -1;
}

PyMODINIT_FUNC
initdmraid(void)
{
	PyObject *m;
	struct lib_context *ctx = NULL;
	const char * const argv[] = {"block.dmraid", NULL};

	m = Py_InitModule("dmraid", pydmraid_functions);

	if (PyType_Ready(&PydmraidDevice_Type) < 0)
		return;
	Py_INCREF(&PydmraidDevice_Type);
	PyModule_AddObject(m, "device", (PyObject *) &PydmraidDevice_Type);

	if (PyType_Ready(&PydmraidRaidDev_Type) < 0)
		return;
	Py_INCREF(&PydmraidRaidDev_Type);
	PyModule_AddObject(m, "raiddev", (PyObject *) &PydmraidRaidDev_Type);

	if (PyType_Ready(&PydmraidRaidSet_Type) < 0)
		return;
	Py_INCREF(&PydmraidRaidSet_Type);
	PyModule_AddObject(m, "raidset", (PyObject *) &PydmraidRaidSet_Type);

	PydmraidList_Type.tp_as_sequence = &pydmraid_list_as_sequence;
	if (PyType_Ready(&PydmraidList_Type) < 0)
		return;
	Py_INCREF(&PydmraidList_Type);
	PyModule_AddObject(m, "list", (PyObject *) &PydmraidList_Type);

	if (pydmraid_init_grouping_error(m) < 0)
		return;

	if (PyType_Ready(&PydmraidContext_Type) < 0)
		return;
	Py_INCREF(&PydmraidContext_Type);
	PyModule_AddObject(m, "context", (PyObject *) &PydmraidContext_Type);

	ctx = libdmraid_init(1, (char **)argv);
	PyModule_AddStringConstant(m, "version",(char *)libdmraid_version(ctx));
	PyModule_AddStringConstant(m, "date", (char *)libdmraid_date(ctx));
	libdmraid_exit(ctx);

	PyModule_AddIntConstant(m, "format_list", LC_FORMATS);
	PyModule_AddIntConstant(m, "device_list", LC_DISK_INFOS);
	PyModule_AddIntConstant(m, "raid_device_list", LC_RAID_DEVS);
	PyModule_AddIntConstant(m, "raid_set_list", LC_RAID_SETS);

	PyModule_AddIntConstant(m, "disk_status_undef", s_undef);
	PyModule_AddIntConstant(m, "disk_status_broken", s_broken);
	PyModule_AddIntConstant(m, "disk_status_inconsistent", s_inconsistent);
	PyModule_AddIntConstant(m, "disk_status_nosync", s_nosync);
	PyModule_AddIntConstant(m, "disk_status_ok", s_ok);
	PyModule_AddIntConstant(m, "disk_status_setup", s_setup);
}

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
