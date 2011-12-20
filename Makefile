# Makefile for osh-051120

# Begin CONFIGURATION
#
# Adjust the following variables as needed to match your desires
# and/or the requirements of your system.
#
# POSIX and X/Open System Interface notes:
#	On systems which support the XSI extension, defining
#	_XOPEN_SOURCE with a value of 600 enables it.  However,
#	this should only be done when it is absolutely required.
#
#	Notice that defining _XOPEN_SOURCE is not currently required
#	when building this software on FreeBSD, NetBSD, OpenBSD, or
#	Mac OS X.  This may be true for other systems as well.
#
#	However, GNU/Linux systems may require it to be defined.
#	The developer suggests the following on GNU/Linux systems:
#
#		1) Try to build the software without defining it.
#		   If there are no compilation errors or warnings,
#		   there is nothing more to do.  Otherwise, continue
#		   with step number 2.
#
#		2) Uncomment the following `#XSIE=' line by removing
#		   the `#' character.  Then, do a `make clean; make'.
#
#XSIE=		-D_XOPEN_SOURCE=600

# Compiler and linker flags
#
CFLAGS+=	-O2
#CFLAGS+=	-g
CFLAGS+=	-Wall -W
#LDFLAGS+=	-static

# Build utilities (SHELL must be POSIX-compliant)
#
#CC=		/usr/bin/cc
INSTALL=	/usr/bin/install
SHELL=		/bin/sh

# Where does the login(1) utility live on your system?  This is the
# utility used by the shell's `login' special command to replace an
# interactive shell with a new instance of login(1).
#
PATH_LOGIN?=	/usr/bin/login

# Choose where and how to install the binaries and manual pages.
#
PREFIX?=	/usr/local
BINDIR=		$(PREFIX)/bin
MANDIR=		$(PREFIX)/man
MANSECT=	$(MANDIR)/man1
#BINGRP=		-g bin
BINMODE=	-m 0555
#MANGRP=		-g bin
MANMODE=	-m 0444
#
# End CONFIGURATION - Generally, there should be no reason to change
#		      anything below this line.

PROGS=		osh if goto fd2
INSTALLPROGS=	install-osh install-if install-goto install-fd2

PEXSRC=		pexec.h pexec.c
SHSRC=		osh.h osh.c osh-parse.c osh-exec.c osh-misc.c
SHOBJ=		osh.o osh-parse.o osh-exec.o osh-misc.o
ALLOBJ=		$(SHOBJ) pexec.o if.o goto.o fd2.o

DEFINES=	$(XSIE) $(SH) -D_PATH_LOGIN='"$(PATH_LOGIN)"'

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFINES) $<

#
# Build targets
#
all: $(PROGS)

sh6: version.h $(SHSRC) $(PEXSRC)
	@if test -e osh.o && strings osh.o | grep HOME >/dev/null; then \
		rm -f $(SHOBJ); \
	fi
	@$(MAKE) SH=-DSH6 $@bin

osh: version.h $(SHSRC) $(PEXSRC)
	@if test -e osh.o && ! ( strings osh.o | grep HOME >/dev/null ); then \
		rm -f $(SHOBJ); \
	fi
	@$(MAKE) $@bin

if: version.h if.c $(PEXSRC)
	@$(MAKE) $@bin

goto: version.h goto.c
	@$(MAKE) $@bin

fd2: version.h fd2.c $(PEXSRC)
	@$(MAKE) $@bin

$(SHOBJ): osh.h
$(ALLOBJ): version.h
osh-exec.o if.o fd2.o: pexec.h
pexec.o: $(PEXSRC)

sh6bin: $(SHOBJ) pexec.o
	$(CC) $(LDFLAGS) -o sh6 $(SHOBJ) pexec.o $(LIBS)

oshbin: $(SHOBJ) pexec.o
	$(CC) $(LDFLAGS) -o osh $(SHOBJ) pexec.o $(LIBS)

ifbin: if.o pexec.o
	$(CC) $(LDFLAGS) -o if if.o pexec.o $(LIBS)

gotobin: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

fd2bin: fd2.o pexec.o
	$(CC) $(LDFLAGS) -o fd2 fd2.o pexec.o $(LIBS)

#
# Test targets
#
check: osh if goto
	@( cd tests && $(SHELL) run.sh osh )

check-sh6: sh6 if goto
	@( cd tests && $(SHELL) run.sh sh6 )

check-newlog: osh if goto
	@( cd tests && $(SHELL) run.sh -newlog osh )

#
# Install targets
#
install: all $(INSTALLPROGS)

install-dest:
	test -d $(DESTDIR)$(BINDIR) || mkdir -p $(DESTDIR)$(BINDIR)
	test -d $(DESTDIR)$(MANSECT) || mkdir -p $(DESTDIR)$(MANSECT)

install-osh: osh install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(DESTDIR)$(MANSECT)

install-sh6: sh6 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) sh6 $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) sh6.1 $(DESTDIR)$(MANSECT)

install-if: if install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(DESTDIR)$(MANSECT)

install-goto: goto install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(DESTDIR)$(MANSECT)

install-fd2: fd2 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) fd2 $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) fd2.1 $(DESTDIR)$(MANSECT)

#
# Cleanup targets
#
clean-obj:
	rm -f $(ALLOBJ)

clean: clean-obj
	rm -f $(PROGS) sh6
