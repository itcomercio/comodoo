
DESTDIR := 
SITELIB := $(shell python -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)[1:]")
DEBUGSRC := "/usr/src/debug"
DEBUGLIB := "/usr/lib/debug"
CC := gcc
PYVER := $(shell python -c "import sys; print sys.version[0:3]")
#CFLAGS := -g -I/usr/include/python$(PYVER) -Wall -Werror -fPIC
CFLAGS := -g -I/usr/include/python$(PYVER) -Wall -fPIC
LDFLAGS := -shared

VERSION := $(shell awk '/Version:/ { print $$2 }' python-pyblock.spec)
RELEASE := $(shell awk -F '[ %]' '/Release:/ { print $$2 }' python-pyblock.spec)
USESELINUX = 1

dm_LIBS = dmraid devmapper
ifeq (1, $(USESELINUX))
dm_LIBS += selinux
CFLAGS += -DUSESELINUX=1
endif
dmraid_LIBS = dmraid devmapper

PYFILES=__init__.py maps.py device.py
LIBS = dmmodule.so dmraidmodule.so

all : $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

dmmodule.so.$(VERSION) : dm.o pyhelpers.o exc.o
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname="$@" -o $@ $^ $(foreach x,$(value dm_LIBS),-l$(x))

dmraidmodule.so.$(VERSION) : dmraid.o pyhelpers.o
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname="$@" -o $@ $^ $(foreach x,$(value dmraid_LIBS),-l$(x))

%.so : %.so.$(VERSION)
	@echo -n
	@#ln -vsf $< $@

tag:
	@git tag -a -m "Tag as pyblock-$(VERSION)-$(RELEASE)" pyblock-$(VERSION)-$(RELEASE)
	@echo "Tagged as pyblock-$(VERSION)-$(RELEASE)"

archive-no-tag:
	@git archive --format=tar --prefix=pyblock-$(VERSION)/ pyblock-$(VERSION)-$(RELEASE) > pyblock-$(VERSION).tar
	@bzip2 -f pyblock-$(VERSION).tar

archive: clean tag archive-no-tag

flat_install: all
	@install -v -m 0755 -d ${DESTDIR}/
	@for x in $(PYFILES) ; do \
		install -v -m 0755 $$x ${DESTDIR}/$$x ; \
	done
	@for x in $(LIBS) ; do \
		install -v -m 0755 $$x.$(VERSION) ${DESTDIR}/$$x ; \
	done

install_debug: clean all
	@install -v -m 0755 -d "${DESTDIR}/RHupdates/block"
	@install -v -m 0755 -d "${DESTDIR}/RHupdates/${DEBUGLIB}/${SITELIB}/block"
	@install -v -m 0755 -d "${DESTDIR}/RHupdates/usr/src/debug/pyblock-${VERSION}"
	@for x in ${PYFILES} ; do \
		install -m 0755 $$x ${DESTDIR}/RHupdates/block/$$x ; \
	done
	@for x in "*.[ch]" ; do \
		install -m 0644 $$x \
		  "${DESTDIR}/RHupdates/usr/src/debug/pyblock-${VERSION}/" ; \
	done
	@for x in ${LIBS} ; do \
		echo /usr/lib/rpm/debugedit -b "${PWD}" \
			-d /usr/src/debug \
			"$$x.${VERSION}" ; \
		/usr/lib/rpm/debugedit -b "${PWD}" \
			-d /usr/src/debug \
			"$$x.${VERSION}" ; \
		echo eu-strip -f \
		  "${DESTDIR}/RHupdates/${DEBUGLIB}/${SITELIB}/block/$$x.debug"\
		  "$$x.${VERSION}" ; \
		echo install -m 0755 $$x.${VERSION} \
			${DESTDIR}/RHupdates/block/$$x ; \
		install -m 0755 $$x.${VERSION} \
			${DESTDIR}/RHupdates/block/$$x ; \
	done

install : all
	@install -v -m 0755 -d "${DESTDIR}/usr/share/doc/pyblock-${VERSION}"
	@install -v -m 0644 COPYING "${DESTDIR}/usr/share/doc/pyblock-${VERSION}"
	@install -v -m 0755 -d ${DESTDIR}/${SITELIB}/block/
	@for x in ${PYFILES} ; do \
		install -v -m 0755 $$x ${DESTDIR}/${SITELIB}/block/$$x ; \
	done
	@for x in ${LIBS} ; do \
		install -v -m 0755 $$x.${VERSION} \
			${DESTDIR}/${SITELIB}/block/$$x ; \
	done

clean :
	@rm -vf *.py[co] *.so *.so.${VERSION} *.o core* *.pyc *.pyo
	@rm -vf pyblock-*.tar.bz2

.PHONY : clean install
.SECONDARY : ${foreach lib,${LIBS},${lib}.${VERSION}}
