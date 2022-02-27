# $Id: Makefile 3 2011-09-14 03:17:55Z efalk $

# (C) Copyright 2011 Hewlett-Packard Development Company, L.P. 
#
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either version 2 
# of the License, or (at your option) any later version. 
#
# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# GNU General Public License for more details. 
#
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

# Targets:
#
#  all		Build everything
#  install	Install everything
#  clean	remove .o files, etc.
#  dist		resizefat.tar.gz
#  rpm		resizefat-1.0-1.i386.rpm
#  distclean	remove everything left over from a build

VERSION = $(shell grep '%define.ver' resizefat.spec | cut -f 3)
RELEASE = $(shell grep '%define.rel' resizefat.spec | cut -f 3)

# Where it will get installed:
prefix = /usr/local
bindir = ${prefix}/bin
man8dir = ${prefix}/share/man/man8

# Build options
OPT = -O
LDOPTS = -s

CFLAGS += ${OPT} -Wall -Werror

BIN=resizefat
SOURCE=resizefat.c repairfat.c
HEADERS=repairfat.h

OBJECTS=$(SOURCE:.c=.o)

LIBS = -lparted

# CC=arm-none-linux-gnueabi-gcc

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) ${LDOPTS} -o $(BIN) $(OBJECTS) $(LIBS) $(LDFLAGS)

DESTDIRS = ${bindir} ${man8dir}

install: all ${DESTDIRS}
	install ${BIN} ${bindir}
	install -m 644 resizefat.8 ${man8dir}

${DESTDIRS}:
	mkdir -p $@

clean:
	rm -f $(BIN) $(OBJECTS) tags


# From here down, it's all about building packages.

DIST_FILES = README INSTALL LICENSE ${SOURCE} ${HEADERS} Makefile\
	resizefat.8 resizefat.lsm resizefat.spec

DIST_NAME = resizefat-${VERSION}
DIST_DIR = /tmp/${DIST_NAME}
TARBALL = ${DIST_NAME}.tar.gz
RPM_SOURCE_DIR = /tmp/rpmsource-${DIST_NAME}
RPM_BUILD_DIR = /tmp/rpmbuild-${DIST_NAME}

distclean: clean
	rm -rf ${TARBALL} RPMS *.deb

dist:	${TARBALL}

${TARBALL}: ${DIST_FILES}
	rm -rf ${DIST_DIR}
	mkdir -p ${DIST_DIR}
	cp -p ${DIST_FILES} ${DIST_DIR}
	chmod -R a+r ${DIST_DIR}
	chmod -R u+w ${DIST_DIR}
	chmod -R go-w ${DIST_DIR}
	cd ${DIST_DIR}/.. ; tar cvf ${DIST_NAME}.tar ${DIST_NAME}
	mv ${DIST_DIR}.tar .
	gzip ${DIST_NAME}.tar
	rm -rf ${DIST_DIR}


rpm:	${TARBALL}
	rm -rf ${RPM_SOURCE_DIR} ${RPM_BUILD_DIR}
	mkdir -p ${RPM_SOURCE_DIR}
	mkdir -p ${RPM_BUILD_DIR}
	cp ${TARBALL} ${RPM_SOURCE_DIR}

	rpmbuild -bb resizefat.spec \
	  --define "_sourcedir ${RPM_SOURCE_DIR}" \
	  --define "_builddir ${RPM_BUILD_DIR}" \
	  --define "_rpmdir ${RPM_SOURCE_DIR}"

	rm ${RPM_SOURCE_DIR}/${TARBALL}
	test -d RPMS || mkdir RPMS
	cp -af  ${RPM_SOURCE_DIR}/* RPMS
	rm -rf ${RPM_SOURCE_DIR} ${RPM_BUILD_DIR}
