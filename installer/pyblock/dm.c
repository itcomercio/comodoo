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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/blkpg.h>

#include <libdevmapper.h>
#ifdef USESELINUX
#include <selinux/selinux.h>
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

#include "pyhelpers.h"
#include "dm.h"
#include "exc.h"

#define PYDM_ARGS (METH_VARARGS|METH_KEYWORDS)

void pydm_log_fn(int level, const char *file, int line, const char *f, ...);

/* dev stuff begin */

static void
pydm_dev_clear(PydmDeviceObject *self)
{
	self->dev = 0;
	self->mode = 0600;
#ifdef USESELINUX
	if (self->con) {
		free(self->con);
		self->con = NULL;
	}
#endif
}

static void
pydm_dev_dealloc(PydmDeviceObject *dev)
{
	pydm_dev_clear(dev);
	PyObject_Del(dev);
}

static int
pydm_dev_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"major", "minor", "dev", "path", NULL};
	PydmDeviceObject *dev = (PydmDeviceObject *)self;
	long long ma=-1, mi=-1, devno=-1;
	PyObject *pathobj = NULL;

	pydm_dev_clear(dev);

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
			"|O&O&O&O!:device.__init__", kwlist,
			pyblock_potoll, &ma, pyblock_potoll, &mi,
			pyblock_potoll, &dev, &PyString_Type, &pathobj))
		return -1;

	if (!pathobj && devno==-1 && (ma == -1 || mi == -1)) {
		PyErr_SetString(PyExc_ValueError,
				"dm.device() takes at least 1 argument");
		return -1;
	}

	if (pathobj) {
		struct stat sr;
		int rc ;
		char *path;

		path = PyString_AsString(pathobj);
		if (PyErr_Occurred())
			return -1;

		rc = stat(path, &sr);
		if (rc < 0) {
			PyErr_SetFromErrno(PyExc_OSError);
			return -1;
		}
		if (!(sr.st_mode & S_IFBLK)) {
			PyErr_SetString(PyExc_ValueError,
				"not a block device\n");
			return -1;
		}
		dev->dev = sr.st_rdev;
		dev->mode = sr.st_mode & ~S_IFMT;

#ifdef USESELINUX
		if (is_selinux_enabled()) {
			security_context_t con;
			if (getfilecon(path, &con) < 0) {
				return 0;
			}
			dev->con = strdup(con);
		}
#endif
		return 0;
	}
	if (devno != -1)
		dev->dev = devno;
	else
		dev->dev = makedev(ma, mi);
	return 0;
}

static PyObject *
pydm_dev_str_method(PyObject *self)
{
	PydmDeviceObject *dev = (PydmDeviceObject *)self;

	PyObject *str = pyblock_PyString_FromFormat("%u:%u",
			major(dev->dev), minor(dev->dev));
	return str;
}

static int
pydm_check_dir(char *path)
{
	char *lpath, *opath = NULL;
	struct stat sb;
	int rc;

	memset(&sb, '\0', sizeof sb);
	rc = stat(path, &sb);
	if (rc < 0)
		return 0;
	opath = strdup(path);
	while (S_ISLNK(sb.st_mode)) {
		lpath = canonicalize_file_name(opath);
		rc = stat(lpath, &sb);
		free(opath);
		if (rc < 0) {
			free(lpath);
			return 0;
		}
		opath = lpath;
	}
	if (opath)
		free(opath);
	
	if (S_ISDIR(sb.st_mode))
		return 1;
	return 0;
}

static PyObject *
pydm_dev_mknod(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef USESELINUX
	char *kwlist[] = {"path", "mode", "context", NULL};
	security_context_t con = NULL;
#else
	char *kwlist[] = {"path", "mode", NULL};
#endif
	PydmDeviceObject *dev = (PydmDeviceObject *)self;
	char *path = NULL, *subpath;
	long long llmode = 0600;
	mode_t mode;
	int rc;

	if (dev->dev == 0) {
		pyblock_PyErr_Format(PyExc_ValueError, "invalid major/minor");
		return NULL;
	}

#ifdef USESELINUX
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O&s:device.mknod",
			kwlist, &path, pyblock_potoll, &llmode, &con))
		return NULL;
#else
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O&:device.mknod",
			kwlist, &path, pyblock_potoll, &llmode))
		return NULL;
#endif

	mode = llmode;
	if (mode < 0)
		mode = dev->mode;
	mode |= S_IFBLK;

	subpath = strchr(path, '/');
	while(subpath) {
		char c;
		char *next;

		/* check for the last character being / */
		if (!*(++subpath)) {
			pyblock_PyErr_Format(PyExc_ValueError,
					"invalid path for mknod");
			return NULL;
		}
		next = strchr(subpath, '/');
		if (!next)
			break;

		c = *next;
		*next = '\0';

		rc = mkdir(path, 0755);
		if (rc < 0) {
			if (errno == EEXIST && pydm_check_dir(path))
				errno = 0;
			else {
				pyblock_PyErr_Format(PyExc_OSError,
						"mkdir(%s, 0755): %s\n",
						path, strerror(errno));
				return NULL;
			}
		}
		*next = c;
		subpath = strchr(subpath, '/');
	}
	unlink(path);
	rc = mknod(path, mode, dev->dev);
	if (rc < 0) {
		pyblock_PyErr_Format(PyExc_OSError, "path: %s mode: %d, dev: %" PRIu64"\n", path, mode, dev->dev);
		return NULL;
	}

#ifdef USESELINUX
	if (!is_selinux_enabled()) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	if (!con)
		con = dev->con;
	if (!con) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	rc = setfilecon(path, con);
	if (rc < 0) {
		pyblock_PyErr_Format(PyExc_OSError, "path: %s context: %s\n", path, con);
		return NULL;
	}
#endif

	Py_INCREF(Py_None);
	return Py_None;
}

static int
pydm_dev_compare(PydmDeviceObject *self, PydmDeviceObject *other)
{
	return (self->dev < other->dev) ? -1 : (self->dev > other->dev) ? 1 : 0;
}

static long
pydm_dev_hash(PyObject *self)
{
	PydmDeviceObject *dev = (PydmDeviceObject *)self;

	return dev->dev;
}

static PyObject *
pydm_dev_get(PyObject *self, void *data)
{
	PB_DM_ASSERT_DEV(((PydmDeviceObject *)self), return NULL);

	PydmDeviceObject *dev = (PydmDeviceObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "major")) {
		return PyLong_FromUnsignedLongLong(major(dev->dev));
	} else if (!strcmp(attr, "minor")) {
		return PyLong_FromUnsignedLongLong(minor(dev->dev));
	} else if (!strcmp(attr, "dev")) {
		return PyLong_FromUnsignedLongLong(dev->dev);
	} else if (!strcmp(attr, "mode")) {
		return PyLong_FromUnsignedLongLong(dev->mode);
#ifdef USESELINUX
	} else if (!strcmp(attr, "context")) {
		if (dev->con)
			return PyString_FromString(dev->con);
		else
			return PyString_FromString("");
#endif
	}

	return NULL;
}

static int
pydm_dev_set(PyObject *self, PyObject *value, void *data)
{
	PydmDeviceObject *dev = (PydmDeviceObject *)self;
	const char *attr = (const char *)data;
	u_int64_t v;

	if (!strcmp(attr, "major")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		dev->dev = makedev(v, minor(dev->dev));
	} else if (!strcmp(attr, "minor")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		dev->dev = makedev(major(dev->dev), v);
	} else if (!strcmp(attr, "dev")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		dev->dev = v;
	} else if (!strcmp(attr, "mode")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		dev->mode = v & ~S_IFMT;
#ifdef USESELINUX
	} else if (!strcmp(attr, "context")) {
		char *context = PyString_AsString(value);
		security_context_t con = NULL;

		if (PyErr_Occurred())
			return -1;

		con = strdup(context);
		if (!con) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
			return -1;
		}
		if (dev->con)
			free(dev->con);
		dev->con = con;
#endif
	}
	return 0;
}

static struct PyGetSetDef pydm_dev_getseters[] = {
	{"major", (getter)pydm_dev_get, (setter)pydm_dev_set,
		"major", "major"},
	{"minor", (getter)pydm_dev_get, (setter)pydm_dev_set,
		"minor", "minor"},
	{"dev", (getter)pydm_dev_get, (setter)pydm_dev_set,
		"dev", "dev"},
	{"mode", (getter)pydm_dev_get, (setter)pydm_dev_set,
		"mode", "mode"},
#ifdef USESELINUX
	{"context", (getter)pydm_dev_get, (setter)pydm_dev_set,
		"context", "context"},
#endif
	{NULL},
};

static struct PyMethodDef pydm_dev_methods[] = {
	{"mknod", (PyCFunction) pydm_dev_mknod, PYDM_ARGS,
		"Create the node for the current device"},
	{NULL}
};

PyTypeObject PydmDevice_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dm.device",
	.tp_basicsize = sizeof (PydmDeviceObject),
	.tp_dealloc = (destructor)pydm_dev_dealloc,
	.tp_getset = pydm_dev_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_methods = pydm_dev_methods,
	.tp_compare = (cmpfunc)pydm_dev_compare,
	.tp_hash = pydm_dev_hash,
	.tp_init = pydm_dev_init_method,
	.tp_str = pydm_dev_str_method,
	.tp_new = PyType_GenericNew,
	.tp_doc =	"The device mapper device.  "
			"Contains device file information.",
};

PyObject *
PydmDevice_FromMajorMinor(u_int32_t major, u_int32_t minor)
{
	PyObject *self;

	self = PydmDevice_Type.tp_new(&PydmDevice_Type, NULL, NULL);
	if (!self)
		return NULL;

	((PydmDeviceObject *)self)->dev = makedev(major, minor);

//	Py_INCREF(self);
	return self;
}

/* dev stuff end */

/* table stuff begin */

static void
pydm_table_clear(PydmTableObject *self)
{
	self->start = self->size = 0;
	if (self->type)
		free(self->type);
	if (self->params)
		free(self->params);
}

static void
pydm_table_dealloc(PydmTableObject *self)
{
	pydm_table_clear(self);
	PyObject_Del(self);
}

static int
pydm_init_table(PydmTableObject *table, u_int64_t start, u_int64_t size,
		char *type, char *params)
{
	char *ntype, *nparams, *comment;
	if (size == 0) {
		PyErr_SetString(PyExc_ValueError, "size must be positive");
		return -1;
	}

	ntype = strdup(type);
	if (!ntype) {
		printf("%s: %d\n", __FILE__, __LINE__);
		PyErr_NoMemory();
		return -1;
	}

	comment = strchr(params, '#');
	if (comment)
		*comment = '\0';
	nparams = strdup(params);

	if (!nparams) {
		free(ntype);
		printf("%s: %d\n", __FILE__, __LINE__);
		PyErr_NoMemory();
		return -1;
	}

	table->start = start;
	table->size = size;
	table->type = ntype;
	table->params = nparams;

	return 0;
}

static int
pydm_table_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"start", "size", "type", "params", NULL};
	PydmTableObject *table = (PydmTableObject *)self;
	unsigned long long size, start;
	char *type, *params;

	pydm_table_clear(table);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&O&ss:table.__init__",
			kwlist, pyblock_potoll, &start, pyblock_potoll, &size, 
			&type, &params))
		return -1;

	return pydm_init_table(table, start, size, type, params);
}

static PyObject *
pydm_table_str_method(PyObject *self)
{
	PydmTableObject *table = (PydmTableObject *)self;

	PyObject *str = pyblock_PyString_FromFormat(
			"%" PRIu64 " %" PRIu64 " %s %s",
			table->start, table->size, table->type, table->params);
	return str;
}

/* XXX this doesn't work if one has /dev/sdb and the other has 8:16 */
static int
pydm_table_compare(PydmTableObject *self, PydmTableObject *other)
{
	int ret;
	ret = other->start - self->start;
	if (!ret)
		ret = other->size - self->size;
	if (!ret)
		ret = strcmp(self->type, other->type);
	if (!ret)
		ret = strcmp(self->params, other->params);
	return ret < 0 ? -1 : ret > 0 ? 1 : 0;
}

static PyObject *
pydm_table_get(PyObject *self, void *data)
{
	PB_DM_ASSERT_TABLE(((PydmTableObject *)self), return NULL);

	PydmTableObject *table = (PydmTableObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "start"))
		return PyLong_FromUnsignedLongLong(table->start);
	else if (!strcmp(attr, "size"))
		return PyLong_FromUnsignedLongLong(table->size);
	else if (!strcmp(attr, "type"))
		return PyString_FromString(table->type);
	else if (!strcmp(attr, "params"))
		return PyString_FromString(table->params);

	return NULL;
}

static int
pydm_table_set(PyObject *self, PyObject *value, void *data)
{
	PydmTableObject *table = (PydmTableObject *)self;
	const char *attr = (const char *)data;
	u_int64_t v;

	if (!strcmp(attr, "start")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		table->start = v;
	} else if (!strcmp(attr, "size")) {
		if (!pyblock_potoll(value, &v))
			return -1;

		table->size = v;
	} else if (!strcmp(attr, "type")) {
		char *s0 = PyString_AsString(value), *s1;

		if (!s0 || PyErr_Occurred())
			return -1;

		s1 = strdup(s0);
		if (!s1) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
			return -1;
		}

		if (table->type)
			free(table->type);
		table->type = s1;
		return 0;
	} else if (!strcmp(attr, "params")) {
		char *s0 = PyString_AsString(value), *s1;

		if (!s0 || PyErr_Occurred())
			return -1;

		s1 = strdup(s0);
		if (!s1) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
			return -1;
		}
		if (table->params)
			free(table->params);
		table->params = s1;
		return 0;
	}

	return 0;
}

static struct PyGetSetDef pydm_table_getseters[] = {
	{"start", (getter)pydm_table_get, (setter)pydm_table_set,
		"start", "start"},
	{"size", (getter)pydm_table_get, (setter)pydm_table_set,
		"size", "size"},
	{"type", (getter)pydm_table_get, (setter)pydm_table_set,
		"type", "type"},
	{"params", (getter)pydm_table_get, (setter)pydm_table_set,
		"params", "params"},
	{NULL},
};


PyTypeObject PydmTable_Type = {
	PyObject_HEAD_INIT(NULL)
	.ob_size = 0,
	.tp_name = "dm.table",
	.tp_basicsize = sizeof (PydmTableObject),
	.tp_dealloc = (destructor)pydm_table_dealloc,
	.tp_getset = pydm_table_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_init = pydm_table_init_method,
	.tp_str = pydm_table_str_method,
	.tp_new = PyType_GenericNew,
	.tp_compare = (cmpfunc)pydm_table_compare,
	.tp_doc = "The device mapper device table",
};

PyObject *
PydmTable_FromInfo(loff_t start, u_int64_t size, char *type, char *params)
{
	PyObject *self;
	int rc;

	self = PydmTable_Type.tp_new(&PydmTable_Type, NULL, NULL);
	if (!self)
		return NULL;


	rc = pydm_init_table((PydmTableObject *)self, start, size,
			type, params);
	if (!rc < 0) {
		Py_DECREF(self);
		return NULL;
	}
	return self;
}

/* table stuff end */

#define python_error_destroy_task(task, err) \
	if (PyErr_Occurred()) { \
		dm_task_destroy(task); \
		dm_log_init(NULL); \
		return err; \
	}

/* map stuff begin */

struct pydm_map_key {
	enum {
		NONE,
		UUID,
		DEV,
		NAME,
	} type;
	union {
		char *uuid;
		u_int64_t dev;
		char *name;
	};
};

static int
pydm_map_get_best_key(PydmMapObject *map, struct pydm_map_key *key)
{
	PydmDeviceObject *dev = (PydmDeviceObject *)map->dev;
	if (map->name) {
		key->type = NAME;
		key->name = map->name;
	} else if (map->uuid) {
		key->type = UUID;
		key->uuid = map->uuid;
	} else if (map->dev) {
		key->type = DEV;
		key->dev = dev->dev;
	} else {
		map->initialized = 0;
		key->type = NONE;
		return -1;
	}
	return 0;
}

void
pydm_task_set_key(struct dm_task *task, struct pydm_map_key *key)
{

	switch (key->type) {
		case NONE:
			break;
		case UUID:
			dm_task_set_uuid(task, key->uuid);
			break;
		case DEV:
			dm_task_set_major(task, major(key->dev));
			dm_task_set_minor(task, minor(key->dev));
			break;
		case NAME:
			dm_task_set_name(task, key->name);
			break;
	}
}

static void
pydm_map_clear(PydmMapObject *map)
{
	if (map->uuid) {
		free(map->uuid);
		map->uuid = NULL;
	}
	if (map->name) {
		free(map->name);
		map->name = NULL;
	}
	if (map->dev) {
		Py_DECREF(map->dev);
		map->dev = NULL;
	}

	map->initialized = 0;
}

static int
pydm_map_read(PydmMapObject *map, struct pydm_map_key *key)
{
	struct dm_task *task;
	struct dm_info *info = &map->info;

	if (key->type == NONE)
		return 0;

	memset(info, 0, sizeof (struct dm_info));
	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_INFO);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return -1;
	}

	pydm_task_set_key(task, key);
	dm_task_run(task);
	dm_task_get_info(task, info);

	python_error_destroy_task(task, -1);

	if (!info->exists) {
		//pydm_map_clear(map);
		map->initialized = 0;
	
		switch (key->type) {
			case NONE:
				break;
			case UUID:
				if (!map->uuid)
					map->uuid = strdup(key->uuid);
				break;
			case DEV:
				if (!map->dev)
					map->dev = PydmDevice_FromMajorMinor(
							major(key->dev),
							minor(key->dev));
				break;
			case NAME:
				if (!map->name)
					map->name = strdup(key->name);
				break;
		}
		if (!map->uuid && !map->name && !map->dev) {
			pydm_map_clear(map);
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		} else
			map->initialized = 1;

		dm_task_destroy(task);
		dm_log_init(NULL);
		return (map->initialized - 1);
	}

	map->uuid = strdup(dm_task_get_uuid(task));
	map->name = strdup(dm_task_get_name(task));
	Py_XDECREF(map->dev);
	map->dev = PydmDevice_FromMajorMinor(info->major, info->minor);

	dm_task_destroy(task);
	dm_log_init(NULL);

	if (!map->uuid && !map->name && !map->dev) {
		pydm_map_clear(map);
		printf("%s: %d\n", __FILE__, __LINE__);
		PyErr_NoMemory();
		return -1;
	}

	map->initialized = 1;
	return 0;
}

static int
pydm_map_refresh(PydmMapObject *map)
{
	struct pydm_map_key key;
	int rc;

	rc = pydm_map_get_best_key(map, &key);
#if 0
	switch (key.type) {
		case NONE:
			fprintf(stderr, "no key available\n");
			break;
		case UUID:
			fprintf(stderr, "key is uuid: %s\n", key.uuid);
			break;
		case NAME:
			fprintf(stderr, "key is name: %s\n", key.name);
			break;
		case DEV:
			fprintf(stderr, "key is dev: %d:%d\n", major(key.dev),minor(key.dev));
			break;
	}
#endif
	if (rc < 0) {
		PyErr_SetString(PyExc_AssertionError, "map is not initialized");
		return rc;
	}

	return pydm_map_read(map, &key);
}

static PyObject *
pydm_map_simple(PydmMapObject *map, int taskno)
{
	struct pydm_map_key key;
	struct dm_task *task;
	int rc;

	dm_log_init(pydm_log_fn);
	task = dm_task_create(taskno);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return NULL;
	}

	rc = pydm_map_get_best_key(map, &key);
	if (rc >= 0 && key.type == NONE)
		rc = -1;
	if (rc < 0) {
		PyErr_SetString(PyExc_AssertionError, "map is not initialized");
		python_error_destroy_task(task, NULL);
	}

	pydm_task_set_key(task, &key);
	dm_task_run(task);
	python_error_destroy_task(task, NULL);

	dm_task_update_nodes();

	dm_task_destroy(task);
	dm_log_init(NULL);

	if (PyErr_Occurred())
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static int
pydm_map_create(PydmMapObject *map, PyObject *table)
{
	struct dm_task *task;
	int i;

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_CREATE);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return -1;
	}

	dm_task_set_name(task, map->name);
	python_error_destroy_task(task, -1);
	if (map->uuid)
		dm_task_set_uuid(task, map->uuid);
	python_error_destroy_task(task, -1);

	for (i = 0; i < PyList_Size(table); i++) {
		PydmTableObject *row =
			(PydmTableObject *)PyList_GET_ITEM(table, i);

		if (!PyObject_IsInstance((PyObject *)row,
					 (PyObject *)&PydmTable_Type)) {
			PyErr_SetString(PyExc_ValueError,
				"invalid table type in table list");
			dm_task_destroy(task);
			dm_log_init(NULL);
			return -1;
		}
		dm_task_add_target(task, row->start, row->size,
				   row->type, row->params);
		python_error_destroy_task(task, -1);
	}

	if (map->dev) {
		PydmDeviceObject *dev = (PydmDeviceObject *)map->dev;
		dm_task_set_major(task, major(dev->dev));
		dm_task_set_minor(task, minor(dev->dev));
	}
	python_error_destroy_task(task, -1);
	
	dm_task_run(task);
	python_error_destroy_task(task, -1);

	dm_task_update_nodes();
	dm_task_destroy(task);
	dm_log_init(NULL);
	if (PyErr_Occurred())
		return -1;
	map->initialized = 0;
	return pydm_map_refresh(map);
}

static PyObject *
pydm_map_remove(PyObject *self)
{
	return pydm_map_simple((PydmMapObject *)self, DM_DEVICE_REMOVE);
}

static int
pydm_map_init_method(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"name", "table", "uuid", "dev", NULL};
	PydmMapObject *map = (PydmMapObject *)self;
	PydmDeviceObject *dev = NULL;
	PyObject *table = NULL;
	char *uuid = NULL, *name = NULL;

	pydm_map_clear(map);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO!|zO!:map.__init__",
			kwlist, &name, &PyList_Type, &table, &uuid,
			&PydmDevice_Type, &dev))
		return -1;

	map->name = strdup(name);

	if (uuid)
		map->uuid = strdup(uuid);
	if (dev) {
		Py_INCREF(dev);
		map->dev = (PyObject *)dev;
	}

	return pydm_map_create(map, table);
}

static void
pydm_map_dealloc(PydmMapObject *self)
{
	pydm_map_clear(self);
	PyObject_Del(self);
}

#if 1
static long
pydm_map_hash(PyObject *self)
{
	PydmMapObject *map = (PydmMapObject *)self;
	return pydm_dev_hash(map->dev);
}
#endif

static int
pydm_map_compare(PydmMapObject *self, PydmMapObject *other)
{
	int rc;
	PydmDeviceObject *sdev, *odev;
	if (self->uuid && other->uuid) {
		rc = strcmp(self->uuid, other->uuid);
		if (rc)
			return rc;
	}
	sdev = (PydmDeviceObject *)self->dev;
	odev = (PydmDeviceObject *)other->dev;
	if (sdev && odev) {
		rc = pydm_dev_compare(sdev, odev);
		if (rc)
			return rc;
	}
	if (self->name && other->name) {
		rc = strcmp(self->name, other->name);
		if (rc)
			return rc;
	}
	return 0;
}

static int
pydm_map_set_name(PydmMapObject *map, const char *name)
{
	struct dm_task *task;
	int rc;
	char *newname;
	
	rc = pydm_map_refresh(map);
	if (rc < 0)
		return rc;

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_RENAME);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return -1;
	}

	newname = strdup(name);
	if (!newname) {
		dm_task_destroy(task);
		dm_log_init(NULL);
		printf("%s: %d\n", __FILE__, __LINE__);
		PyErr_NoMemory();
		return -1;
	}

	/* WTF?  you can't rename if you set the UUID?  Seems pretty
	   damn buggy... but at least the names in this case must be
	   unique. */
#if 0
	dm_task_set_uuid(task, map->uuid);
#endif
	dm_task_set_name(task, map->name);
	dm_task_set_newname(task, newname);
	dm_task_run(task);
	python_error_destroy_task(task, -1);

	dm_task_update_nodes();
	dm_task_destroy(task);
	dm_log_init(NULL);

	free(map->name);
	map->name = newname;

	return pydm_map_refresh(map);
}

static int
pydm_map_set_suspend(PydmMapObject *map, int suspend)
{
	struct dm_task *task;
	struct pydm_map_key key;
	int rc;

	rc = pydm_map_get_best_key(map, &key);
	if (rc < 0) {
		PyErr_SetString(PyExc_AssertionError, "map is not initialized");
		return rc;
	}

	dm_log_init(pydm_log_fn);
	task = dm_task_create(suspend ? DM_DEVICE_SUSPEND : DM_DEVICE_RESUME);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return -1;
	}
	pydm_task_set_key(task, &key);

	dm_task_run(task);
	python_error_destroy_task(task, -1);

	dm_task_update_nodes();
	dm_task_destroy(task);
	dm_log_init(NULL);

	return pydm_map_refresh(map);
}

static PyObject *
pydm_map_get_table(PydmMapObject *map)
{
	struct dm_task *task;
	struct pydm_map_key key;
	int rc;
	void *next = NULL;
	PyObject *table = NULL, *table_list = NULL;

	rc = pydm_map_get_best_key(map, &key);
	if (rc < 0) {
		PyErr_SetString(PyExc_AssertionError, "map is not initialized");
		return NULL;
	}

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_TABLE);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return NULL;
	}

	pydm_task_set_key(task, &key);
	dm_task_run(task);
	python_error_destroy_task(task, NULL);

	do {
		u_int64_t start, length;
		char *target_type, *params;

		next = dm_get_next_target(task, next, &start, &length,
				&target_type, &params);
		if (!target_type) {
			PyErr_SetString(PyExc_RuntimeError, "no dm table found");
			Py_CLEAR(table_list);
			break;
		}

		if (!table_list) {
			table_list = PyList_New(0);
			if (!table_list)
				break;
		}

		table = PydmTable_FromInfo(start, length, target_type, params);
		if (!table) {
			Py_CLEAR(table_list);
			break;
		}

		rc = PyList_Append(table_list, table);
		Py_DECREF(table);
		if (rc < 0) {
			Py_CLEAR(table_list);
			break;
		}
	} while (next);

	dm_task_update_nodes();
	dm_task_destroy(task);
	dm_log_init(NULL);

 	return table_list;
}

static PyObject *
pydm_map_get_deps(PydmMapObject *map)
{
	struct pydm_map_key key;
	struct dm_task *task;
	struct dm_info *info = &map->info;
	struct dm_deps *deps;
	int rc, i;
	PyObject *o = NULL;

	rc = pydm_map_get_best_key(map, &key);
	if (rc < 0) {
		PyErr_SetString(PyExc_AssertionError, "map is not initialized");
		return o;
	}

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_DEPS);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		dm_log_init(NULL);
		return o;
	}

	pydm_task_set_key(task, &key);
	dm_task_run(task);
	dm_task_get_info(task, info);
	deps = dm_task_get_deps(task);
	if (!deps) {
		pydm_map_clear(map);
		python_error_destroy_task(task, o);
		return o;
	}

	python_error_destroy_task(task, o);
	if (!info->exists) {
		PyErr_SetString(PyExc_AssertionError, "map does not exist");
		dm_task_destroy(task);
		dm_log_init(NULL);
		return o;
	}

	o = PyTuple_New(deps->count);
	if (!o) {
		dm_task_destroy(task);
		dm_log_init(NULL);
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		return NULL;
	}

	for (i = 0; i < deps->count; i++) {
		PyObject *dev = PydmDevice_FromMajorMinor(
					major(deps->device[i]),
					minor(deps->device[i]));

		if (!dev) {
			dm_task_destroy(task);
			dm_log_init(NULL);
			Py_DECREF(o);
			if (!PyErr_Occurred()) {
				printf("%s: %d\n", __FILE__, __LINE__);
				PyErr_NoMemory();
			}
			return NULL;
		}
		rc = PyTuple_SetItem(o, i, dev);
		if (rc < 0) {
			dm_task_destroy(task);
			dm_log_init(NULL);
			Py_DECREF(o);
			if (!PyErr_Occurred()) {
				printf("%s: %d\n", __FILE__, __LINE__);
				PyErr_NoMemory();
			}
			return NULL;
		}
	}
	dm_task_update_nodes();
	dm_task_destroy(task);
	dm_log_init(NULL);

	if (PyErr_Occurred()) {
		Py_DECREF(o);
		return NULL;
	}
	return o;
}

PyObject *
pydm_map_get(PyObject *self, void *data)
{
	PB_DM_ASSERT_MAP(((PydmMapObject *)self), return NULL);

	PydmMapObject *map = (PydmMapObject *)self;
	const char *attr = (const char *)data;

	if (!map->initialized) {
		int rc;
		rc = pydm_map_refresh(map);
		if (rc < 0)
			return NULL;
	}

	if (!strcmp(attr, "exists"))
		return PyBool_FromLong(map->info.exists);
	else if (!strcmp(attr, "suspended"))
		return PyBool_FromLong(map->info.suspended);
	else if (!strcmp(attr, "live_table"))
		return PyBool_FromLong(map->info.live_table);
	else if (!strcmp(attr, "inactive_table"))
		return PyBool_FromLong(map->info.inactive_table);
	else if (!strcmp(attr, "open_count"))
		return PyLong_FromLong(map->info.open_count);
	else if (!strcmp(attr, "dev")) {
		PyObject *dev = map->dev;
		if (!dev) {
			dev = PydmDevice_FromMajorMinor(0,0);
			map->dev = dev;
		}
		Py_INCREF(dev);
		return dev;
	} else if (!strcmp(attr, "uuid")) {
		if (!map->uuid) {
			Py_INCREF(Py_None);
			return Py_None;
		}
		return PyString_FromString(map->uuid);
	} else if (!strcmp(attr, "name")) {
		if (!map->name) {
			Py_INCREF(Py_None);
			return Py_None;
		}
		return PyString_FromString(map->name);
	}
	else if (!strcmp(attr, "table"))
		return pydm_map_get_table(map);
	else if (!strcmp(attr, "deps"))
		return pydm_map_get_deps(map);

	return NULL;
}

static int
pydm_map_set(PyObject *self, PyObject *value, void *data)
{
	PydmMapObject *map = (PydmMapObject *)self;
	const char *attr = (const char *)data;

	if (!map->initialized) {
		struct pydm_map_key key;

		key.type = NONE;
		if (!strcmp(attr, "uuid")) {
			char *v = PyString_AsString(value);
			if (!v || PyErr_Occurred())
				return -1;
			
			key.type = UUID;
			key.uuid = strdup(v);
		} else if (!strcmp(attr, "name")) {
			char *v = PyString_AsString(value);
			if (!v || PyErr_Occurred())
				return -1;
			
			key.type = NAME;
			key.name = strdup(v);
		} else if (!strcmp(attr, "dev")) {
			key.type = DEV;
			key.dev = PyLong_AsUnsignedLongLong(value);
		}
		return pydm_map_read(map, &key);
	} else if (!strcmp(attr, "name")) {
		char *newname = PyString_AsString(value);

		if (!newname || PyErr_Occurred())
			return -1;

		return pydm_map_set_name(map, newname);
	} else if (!strcmp(attr, "suspended")) {
		long suspended = value == Py_True ? 1 : 0;

		if (PyErr_Occurred())
			return -1;
		if (map->info.suspended == suspended)
			return 0;

		return pydm_map_set_suspend(map, suspended);
	}

	return 0;
}

static struct PyGetSetDef pydm_map_getseters[] = {
	{"deps", (getter)pydm_map_get, NULL, "deps", "deps"},
	{"table", (getter)pydm_map_get, NULL, "table", "table"},
	{"exists", (getter)pydm_map_get, NULL, "exists", "exists"},
	{"suspended", (getter)pydm_map_get, (setter)pydm_map_set,
		"suspended", "suspended"},
	{"live_table", (getter)pydm_map_get, NULL, "live_table", "live_table"},
	{"inactive_table", (getter)pydm_map_get, NULL, "inactive_table",
		"inactive_table"},
	{"open_count", (getter)pydm_map_get, NULL, "open_count", "open_count"},
	{"name", (getter)pydm_map_get, (setter)pydm_map_set,
		"name", "name"},
	{"uuid", (getter)pydm_map_get, NULL, "uuid", "uuid"},
	{"dev", (getter)pydm_map_get, pydm_map_set, "dev", "dev"},
	{NULL},
};

static struct PyMethodDef pydm_map_methods[] = {
	{"remove", (PyCFunction) pydm_map_remove, METH_NOARGS},
	{NULL, NULL}
};

PyTypeObject PydmMap_Type = {
	PyObject_HEAD_INIT(NULL)
	.ob_size = 0,
	.tp_name = "dm.map",
	.tp_basicsize = sizeof (PydmMapObject),
	.tp_dealloc = (destructor)pydm_map_dealloc,
	.tp_getset = pydm_map_getseters,
	.tp_methods = pydm_map_methods,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_compare = (cmpfunc)pydm_map_compare,
#if 1
	.tp_hash = pydm_map_hash,
#endif
	.tp_init = pydm_map_init_method,
	.tp_new = PyType_GenericNew,
	.tp_doc =	"The device mapper device map.  "
			"Has most of the related metadata.",
};

/* map stuff end */

/* target stuff begin */
static void
pydm_target_clear(PydmTargetObject *self)
{
	if (!self)
		return;

	if (self->name) {
		free(self->name);
		self->name = NULL;
	}
	self->major = -1;
	self->minor = -1;
	self->micro = -1;
}

static void
pydm_target_dealloc(PydmTargetObject *target)
{
	pydm_target_clear(target);
	PyObject_Del(target);
}

static PyObject *
pydm_target_str_method(PyObject *self)
{
	PydmTargetObject *target = (PydmTargetObject *)self;
	PyObject *str;

	if (target->name)
		str = pyblock_PyString_FromFormat("%-16s v%d.%d.%d",
			target->name, target->major,
			target->minor, target->micro);
	else
		str = target->ob_type->tp_repr(self);

	return str;
}

static PyObject *
pydm_target_get(PyObject *self, void *data)
{
	PB_DM_ASSERT_TARGET(((PydmTargetObject *)self), return NULL);

	PydmTargetObject *target = (PydmTargetObject *)self;
	const char *attr = (const char *)data;

	if (!strcmp(attr, "name")) {
		if (target->name)
			return PyString_FromString(target->name);
		else
			return PyString_FromString("");
	} else if (!strcmp(attr, "major"))
		return PyLong_FromUnsignedLongLong(target->major);
	else if (!strcmp(attr, "minor"))
		return PyLong_FromUnsignedLongLong(target->minor);
	else if (!strcmp(attr, "micro"))
		return PyLong_FromUnsignedLongLong(target->micro);

	return NULL;
}

static struct PyGetSetDef pydm_target_getseters[] = {
	{"name", (getter)pydm_target_get, NULL, "name", "name"},
	{"major", (getter)pydm_target_get, NULL, "major", "major"},
	{"minor", (getter)pydm_target_get, NULL, "minor", "minor"},
	{"micro", (getter)pydm_target_get, NULL, "micro", "micro"},
	{NULL},
};

PyTypeObject PydmTarget_Type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "dm.target",
	.tp_basicsize = sizeof (PydmTargetObject),
	.tp_dealloc = (destructor)pydm_target_dealloc,
	.tp_getset = pydm_target_getseters,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
		    Py_TPFLAGS_BASETYPE,
	.tp_str = pydm_target_str_method,
	.tp_new = PyType_GenericNew,
	.tp_doc = "The device mapper type",
};
/* target stuff end */

/* logging stuff begin */

static PyObject *pydm_py_log_fn;

void pydm_log_fn(int level, const char *file, int line, const char *f, ...)
{
	char *buf;
	int ret;
	va_list ap;
	PyObject *args;

	if (!pydm_py_log_fn)
		return;

	va_start(ap, f);
	ret = vasprintf(&buf, f, ap);
	va_end(ap);
	if (ret < 0) {
		PyErr_SetFromErrno(PyExc_SystemError);
		return;
	}

	args = Py_BuildValue("isis", level, file, line, buf);
	PyObject_CallObject(pydm_py_log_fn, args);
	Py_DECREF(args);

	free(buf);
}

static PyObject *
pydm_log_init(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"log_function", NULL};

	Py_CLEAR(pydm_py_log_fn);

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:log_init", kwlist,
			&pydm_py_log_fn))
		return NULL;

	if (pydm_py_log_fn == Py_None) {
		pydm_py_log_fn = NULL;
		Py_INCREF(Py_None);
		return Py_None;
	}

	if (!PyCallable_Check(pydm_py_log_fn)) {
		pydm_py_log_fn = NULL;
		PyErr_SetString(PyExc_TypeError, "a callable object is required!");
		return NULL;
	}

	Py_INCREF(pydm_py_log_fn);
	Py_INCREF(Py_None);
	return Py_None;
}

/* logging stuff end */

static PyObject *
pydm_scan_parts(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *path = NULL;
	char *kwlist[] = {"dev_path", NULL};
	int fd;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:scanparts", kwlist, 
			&path))
		return NULL;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		PyErr_SetFromErrno(PyExc_SystemError);
		return NULL;
	}

	ioctl(fd, BLKRRPART, 0);
	close(fd);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
pydm_rmpart(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *kwlist[] = {"dev_path", "partno", NULL};
	char *path = NULL;
	unsigned long long partno;
	struct blkpg_partition part;
	struct blkpg_ioctl_arg io = {
		.op = BLKPG_DEL_PARTITION,
		.datalen = sizeof(part),
		.data = &part,
	};
	int fd;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO&:rmpart",
			kwlist, &path, pyblock_potoll, &partno))
		return NULL;

	fd = open(path, O_RDWR);
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
pydm_maps(PyObject *self)
{
	struct dm_task *task = NULL;
	struct dm_names *names;
	int n;
	unsigned int next = 0;
	PyObject *list = NULL, *ret = NULL;
	PyObject *map;

	list = PyList_New(0);
	if (!list)
		goto out;

        /* dm_task_create() only works as root for non root use just return an
           empty list */
        if (geteuid()) {
		printf("%s: %d: not running as root returning empty list\n",
			__FILE__, __LINE__);
		goto save_list;
        }

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_LIST);
	if (!task) {
		if (!PyErr_Occurred()) {
			printf("%s: %d\n", __FILE__, __LINE__);
			PyErr_NoMemory();
		}
		goto out;
	}
	dm_task_run(task);
	if (PyErr_Occurred())
		goto out;

	names = dm_task_get_names(task);
	if (PyErr_Occurred()) {
		printf("%s: %d\n", __FILE__, __LINE__);
		PyErr_NoMemory();
	}
	if (!names || !names->dev)
		goto save_list;

	n = 0;
	do {
		struct pydm_map_key key;
		names = (void *)names + next;

		map = PydmMap_Type.tp_new(&PydmMap_Type, NULL, NULL);
		if (!map)
			goto out;

		key.type = NAME;
		key.name = names->name;

		if (pydm_map_read((PydmMapObject *)map, &key) < 0)
			goto out;
		PyList_Insert(list, n++, map);
		Py_DECREF(map);

		next = names->next;
	} while(next);

save_list:
	Py_INCREF(list);
	ret = list;
out:
	Py_XDECREF(list);
	if (task)
		dm_task_destroy(task);
	dm_log_init(NULL);
	if (PyErr_Occurred()) {
		Py_XDECREF(ret);
		return NULL;
	}
	return ret;
}


static PyObject *
pydm_targets(PyObject *self)
{
	struct dm_task *task;
	struct dm_versions *version, *last_version;
	int n;
	PyObject *list = NULL, *ret = NULL;
	PydmTargetObject *target;

	list = PyList_New(0);
	if (!list)
		goto out;

	dm_log_init(pydm_log_fn);
	task = dm_task_create(DM_DEVICE_LIST_VERSIONS);
	if (!task) {
		if (!PyErr_Occurred())
			pyblock_PyErr_Format(DmError, "%s:%d: %m", __FILE__,
					__LINE__);
		goto out;
	}
	dm_task_run(task);
	if (PyErr_Occurred())
		goto out;

	version = dm_task_get_versions(task);
	if (!version || !version->name) {
		if (!PyErr_Occurred())
			pyblock_PyErr_Format(DmError, "%s:%d: %m", __FILE__,
					__LINE__);
		goto out;
	}

	n = 0;
	do {
		last_version = version;

		target = (PydmTargetObject *)PydmTarget_Type.tp_new(
				&PydmTarget_Type, NULL, NULL);
		if (!target)
			goto out;

		target->name = strdup(version->name);
		target->major = version->version[0];
		target->minor = version->version[1];
		target->micro = version->version[2];

		if (!target->name) {
			Py_DECREF(target);
			goto out;
		}

		PyList_Insert(list, n++, (PyObject *)target);
		Py_DECREF(target);
		target = NULL;

		version = (void *) version + version->next;
	} while(last_version != version);

	Py_INCREF(list);
	ret = list;
out:
	Py_XDECREF(list);
	if (task)
		dm_task_destroy(task);
	dm_log_init(NULL);
	if (PyErr_Occurred()) {
		Py_XDECREF(ret);
		return NULL;
	}
	return ret;
}

static PyMethodDef pydm_functions[] = {
	{"scanparts", (PyCFunction)pydm_scan_parts, PYDM_ARGS,
		"Rescans the partition talbe for the specified device.  "
		"Expects a string representing the device with the keyword "
		"dev_path. returns None on success and NULL on failure."},
	{"rmpart", (PyCFunction)pydm_rmpart, PYDM_ARGS,
		"Deletes a partition from the specified device.  "
		"Expects a string representing the device with the keyword "
		"dev_path and an long representing the partition number "
		"with the keyword partno.  Returns None on success and NULL "
		"on failure."},
	{"set_logger", (PyCFunction)pydm_log_init, PYDM_ARGS,
		"Defines the log function to be used.  Expects a callable "
		"object.  Will return None on success and NULL on failure. "},
	{"maps", (PyCFunction)pydm_maps, METH_NOARGS,
		"Scans the system for mapped devices.  It does not expect any "
		"arguments.  It returns a list of map objects."},
	{"targets", (PyCFunction)pydm_targets, METH_NOARGS,
		"Scans for suppoerted targets.  It does not expect any args.  "
		"It returns a list of target objects."},
	{NULL, NULL}
};

PyMODINIT_FUNC
initdm(void)
{
	PyObject *m;

	m = Py_InitModule("dm", pydm_functions);

	if (PyType_Ready(&PydmDevice_Type) < 0)
		return;
	Py_INCREF(&PydmDevice_Type);
	PyModule_AddObject(m, "device", (PyObject *) &PydmDevice_Type);

	if (PyType_Ready(&PydmTable_Type) < 0)
		return;
	Py_INCREF(&PydmTable_Type);
	PyModule_AddObject(m, "table", (PyObject *) &PydmTable_Type);

	if (PyType_Ready(&PydmMap_Type) < 0)
		return;
	Py_INCREF(&PydmMap_Type);
	PyModule_AddObject(m, "map", (PyObject *) &PydmMap_Type);

	if (PyType_Ready(&PydmTarget_Type) < 0)
		return;
	Py_INCREF(&PydmTarget_Type);
	PyModule_AddObject(m, "target", (PyObject *) &PydmTarget_Type);

	PyModule_AddIntConstant(m, "log_debug", 7);
	PyModule_AddIntConstant(m, "log_info", 6);
	PyModule_AddIntConstant(m, "log_notice", 5);
	PyModule_AddIntConstant(m, "log_warn", 4);
	PyModule_AddIntConstant(m, "log_err", 3);
	PyModule_AddIntConstant(m, "log_fatal", 2);

	if (pydm_exc_init(m) < 0)
		return;

	dm_log_init(NULL);
	pydm_py_log_fn = NULL;

}

/*
 * vim:ts=8:sw=8:sts=8:noet
 */
