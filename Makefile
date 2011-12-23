# Makefile for osh-040628

# Begin CONFIGURATION
#
# Adjust the following variables as needed to match your desires
# and/or the requirements of your system.
#
# POSIX and X/Open CAE notes:
#	_XOPEN_SOURCE=500 corresponds to SUSv2.
#	_XOPEN_SOURCE=600 corresponds to SUSv3.
#
#	_XOPEN_SOURCE does not need to be defined when building this
#	software on FreeBSD, NetBSD, OpenBSD, or OS X.  This may also
#	be true for other operating systems.
#
#	However, GNU/Linux systems do apparently require it.
#	So, uncomment either of the following `#XOPF=' lines
#	by removing the initial `#' character if required.
#
#XOPF=		-D_XOPEN_SOURCE=500
#XOPF=		-D_XOPEN_SOURCE=600

# Compiler and linker flags
#
CFLAGS+=	-Wall -W
#LDFLAGS+=	-static

# Utilities (SHELL must be POSIX-compliant)
#
#CC=		/usr/bin/cc
INSTALL=	/usr/bin/install
SHELL=		/bin/sh

# Choose where and how to install the binaries and manual pages.
#
PREFIX?=	/usr/local
BINDIR=		$(DESTDIR)$(PREFIX)/bin
MANDIR=		$(DESTDIR)$(PREFIX)/man
MANSECT=	$(MANDIR)/man1
#BINGRP=		-g bin
BINMODE=	-m 0555
#MANGRP=		-g bin
MANMODE=	-m 0444
#
# End CONFIGURATION

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) $<

all: osh if goto

osh: osh.o
	$(CC) $(LDFLAGS) -o osh osh.o $(LIBS)

if: if.o
	$(CC) $(LDFLAGS) -o if if.o $(LIBS)

goto: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

fd2: fd2.o
	$(CC) $(LDFLAGS) -o fd2 fd2.o $(LIBS)

# Run a minimal test suite.
check: all
	@if test ! -e tests/.check_done; then $(SHELL) tests/run.sh; fi

# Generate new reference logs for the tests.
check-newref: all
	@$(SHELL) tests/run.sh -newref

clean:
	rm -f osh osh.o if if.o goto goto.o fd2 fd2.o tests/.check_done

install-dest:
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	test -d $(MANSECT) || mkdir -p $(MANSECT)

install-fd2: fd2 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) fd2 $(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) fd2.1 $(MANSECT)

install: all install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(MANSECT)
