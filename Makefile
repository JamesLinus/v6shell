# Makefile for osh-060124

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

# If your system has a login(1) utility, what is its path name?
# This is the utility used by the shell's `login' special command
# to replace an interactive shell with an instance of login(1).
#
PATH_LOGIN?=	/usr/bin/login

# If your system has a newgrp(1) utility, what is its path name?
# This is the utility used by the shell's `newgrp' special command
# to replace an interactive shell with an instance of newgrp(1).
#
PATH_NEWGRP?=

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

PEXSRC=	pexec.h pexec.c
OSHSRC=	osh.h osh.c osh-parse.c osh-exec.c osh-misc.c
OSHOBJ=	osh.o osh-parse.o osh-exec.o osh-misc.o
ALLOBJ=	$(OSHOBJ) pexec.o if.o goto.o fd2.o sh6.o glob6.o

DEFS=	-D_PATH_LOGIN='"$(PATH_LOGIN)"' -D_PATH_NEWGRP='"$(PATH_NEWGRP)"'

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XSIE) $(DEFS) $<

#
# Build targets
#
all: $(PROGS)

osh: version.h $(OSHSRC) $(PEXSRC)
	@$(MAKE) $@bin

if: version.h if.c $(PEXSRC)
	@$(MAKE) $@bin

goto: version.h goto.c
	@$(MAKE) $@bin

fd2: version.h fd2.c $(PEXSRC)
	@$(MAKE) $@bin

sh6port: sh6 glob6

sh6: version.h sh6.c $(PEXSRC)
	@$(MAKE) $@bin

glob6: version.h glob6.c $(PEXSRC)
	@$(MAKE) $@bin

$(OSHOBJ): osh.h
$(ALLOBJ): version.h
osh-exec.o if.o fd2.o sh6.o glob6.o: pexec.h
pexec.o: $(PEXSRC)

oshbin: $(OSHOBJ) pexec.o
	$(CC) $(LDFLAGS) -o osh $(OSHOBJ) pexec.o $(LIBS)

ifbin: if.o pexec.o
	$(CC) $(LDFLAGS) -o if if.o pexec.o $(LIBS)

gotobin: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

fd2bin: fd2.o pexec.o
	$(CC) $(LDFLAGS) -o fd2 fd2.o pexec.o $(LIBS)

sh6bin: sh6.o pexec.o
	$(CC) $(LDFLAGS) -o sh6 sh6.o pexec.o $(LIBS)

glob6bin: glob6.o pexec.o
	$(CC) $(LDFLAGS) -o glob6 glob6.o pexec.o $(LIBS)

#
# Test targets
#
check: osh if goto
	@( cd tests && $(SHELL) run.sh osh )

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

install-if: if install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(DESTDIR)$(MANSECT)

install-goto: goto install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(DESTDIR)$(MANSECT)

install-fd2: fd2 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) fd2 $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) fd2.1 $(DESTDIR)$(MANSECT)

install-sh6port: sh6 glob6 install-sh6 install-glob6

install-sh6: sh6 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) sh6 $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) sh6.1 $(DESTDIR)$(MANSECT)

install-glob6: glob6 install-dest
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) glob6 $(DESTDIR)$(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) glob6.1 $(DESTDIR)$(MANSECT)

#
# Cleanup targets
#
clean-obj:
	rm -f $(ALLOBJ)

clean: clean-obj
	rm -f $(PROGS) sh6 glob6
