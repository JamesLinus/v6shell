# Makefile for osh-051030

# Begin CONFIGURATION
#
# Adjust the following variables as needed to match your desires
# and/or the requirements of your system.
#
# POSIX and X/Open CAE notes:
#	_XOPEN_SOURCE=500 corresponds to SUSv2.
#	_XOPEN_SOURCE=600 corresponds to SUSv3.
#
#	_XOPEN_SOURCE is not currently required when building this
#	software on FreeBSD, NetBSD, OpenBSD, or Mac OS X.  This may
#	also be true for other operating systems.
#
#	However, GNU/Linux systems do seem to require it.
#	If this is true for your particular system, uncomment
#	either of the following `#XOPF=' lines by removing the
#	initial `#' character.  Note that more recent systems
#	do better with `-D_XOPEN_SOURCE=600', and older systems
#	do better with `-D_XOPEN_SOURCE=500'.  Since I cannot
#	say what is recent and what is not, I leave this to
#	the judgment of the user.
#
#	If you get compilation errors or warnings with one, choose
#	the other and do a `make clean; make'.
#
#XOPF=		-D_XOPEN_SOURCE=500
#XOPF=		-D_XOPEN_SOURCE=600

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

DEFINES=	$(SH) -D_PATH_LOGIN='"$(PATH_LOGIN)"' $(XOPF)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFINES) $<

#
# Build targets
#
all: $(PROGS)

osh: $(SHSRC) $(PEXSRC) version.h
	@if test -e osh.o && ! ( strings osh.o | grep HOME >/dev/null ); then \
		rm -f $(SHOBJ); \
	fi
	@$(MAKE) $@x

sh6: $(SHSRC) $(PEXSRC) version.h
	@if test -e osh.o && strings osh.o | grep HOME >/dev/null; then \
		rm -f $(SHOBJ); \
	fi
	@$(MAKE) SH=-DSH6 $@x

oshx: $(SHOBJ) pexec.o
	$(CC) $(LDFLAGS) -o osh $(SHOBJ) pexec.o $(LIBS)

sh6x: $(SHOBJ) pexec.o
	$(CC) $(LDFLAGS) -o sh6 $(SHOBJ) pexec.o $(LIBS)

$(SHOBJ): osh.h
$(ALLOBJ): version.h
osh-exec.o if.o fd2.o: pexec.h
pexec.o: $(PEXSRC)

if: if.o pexec.o
	$(CC) $(LDFLAGS) -o if if.o pexec.o $(LIBS)

goto: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

fd2: fd2.o pexec.o
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
# Do a little cleaning
#
clean-obj:
	rm -f $(ALLOBJ)

clean: clean-obj
	rm -f $(PROGS) sh6
