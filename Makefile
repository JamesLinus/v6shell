# Makefile for osh-041231

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
#	software on FreeBSD, NetBSD, OpenBSD, or OS X.  This may
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
#	- Linking the shell, if, and goto statically can potentially
#	  give significant performance improvements when the shell is
#	  used to execute big and/or complex command files.
#
#CFLAGS+=	-O2
#CFLAGS+=	-g
CFLAGS+=	-Wall -W
#LDFLAGS+=	-static

# Build utilities (SHELL must be POSIX-compliant)
#
#CC=		/usr/bin/cc
INSTALL=	/usr/bin/install
SHELL=		/bin/sh

# Where does login(1) live on your system?  This is the utility used
# by the shell's login special command to replace an interactive shell
# with an new instance of login.
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

PROGS=		osh if goto
INSTALLPROGS=	install-osh install-if install-goto

SHSRC=		osh.h osh.c osh-parse.c osh-exec.c osh-misc.c
SHOBJ=		osh.o osh-parse.o osh-exec.o osh-misc.o
OBJ=		$(SHOBJ) if.o goto.o

DEFINES=	$(SH) -D_PATH_LOGIN='"$(PATH_LOGIN)"' $(XOPF)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFINES) $<

#
# Build targets (for osh, if, goto, and optionally sh6)
#
all: $(PROGS)

osh: $(SHSRC)
	@if test -e osh.o && \
		! ( strings -a osh.o | grep OSH_COMPAT >/dev/null ); then \
			rm -f $(SHOBJ); \
	fi
	@$(MAKE) $@x

sh6: $(SHSRC)
	@if test -e osh.o && \
		strings -a osh.o | grep OSH_COMPAT >/dev/null; then \
			rm -f $(SHOBJ); \
	fi
	@$(MAKE) SH=-DSH6 $@x

oshx: $(SHOBJ)
	$(CC) $(LDFLAGS) -o osh $(SHOBJ) $(LIBS)

sh6x: $(SHOBJ)
	$(CC) $(LDFLAGS) -o sh6 $(SHOBJ) $(LIBS)

$(SHOBJ): osh.h

if: if.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) if.c
	$(CC) $(LDFLAGS) -o if if.o $(LIBS)

goto: goto.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) goto.c
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

#
# Test targets
#
check: all
	@echo "Testing osh, if, and goto:" >&2
	@( cd tests && $(SHELL) run.sh $(PROGS) )

check-sh6: sh6 if goto
	@echo "Testing sh6, if, and goto:" >&2
	@( cd tests && $(SHELL) run.sh sh6 if goto )

check-newlog: all sh6
	@( cd tests && $(SHELL) run.sh -newlog $(PROGS) )

#
# Installation targets
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

#
# Do a little cleaning
#
clean-obj:
	rm -f $(OBJ)

clean: clean-obj
	rm -f osh sh6 if goto
