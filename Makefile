# Makefile for osh-040421

# Begin CONFIGURATION
#
# Defining CLONE gives `chdir' the same behaviour found in the
# Sixth Edition (V6) Unix shell; otherwise, `chdir' is enhanced
# to be more like the `cd' found in other shells as below:
#
#	`chdir'		changes to HOME directory
#	`chdir -'	changes to previous directory
#	`chdir dir'	changes to `dir'
#
# Defining CLONE also changes the globbing behaviour so that it
# is more compatible with /etc/glob from Sixth Edition Unix such
# that when no matches are found for an argument a diagnostic is
# printed and the command is not executed; otherwise, the argument
# is left unchanged and the command is executed as is generally
# the case with Bourne-type shells.
#
# CLONE is not defined by default.
# If you want CLONE behaviour, simply uncomment the `#CLONE=' line
# below by removing the initial `#' character.
#
#CLONE=		-DCLONE

# Compiler flags
#
CFLAGS+=	-Wall

# Choose where and how to install the binaries and manual pages.
#
PREFIX?=	/usr/local
BINDIR=		$(PREFIX)/bin
MANDIR=		$(PREFIX)/man
MANSECT=	$(MANDIR)/man1
INSTALL=	/usr/bin/install
#BINGRP=		-g bin
BINMODE=	-m 0555
#MANGRP=		-g bin
MANMODE=	-m 0444
#
# End CONFIGURATION

XOPF=		-D_XOPEN_SOURCE=500L

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) $(CLONE) $<

all: osh if goto man

osh: osh.o
	$(CC) $(LDFLAGS) -o osh osh.o $(LIBS)

if: if.o
	$(CC) $(LDFLAGS) -o if if.o $(LIBS)

goto: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

man: osh.1
osh.1: osh_clone.1 osh_not_clone.1
	@if test -n "$(CLONE)"; then\
		cp osh_clone.1 osh.1;\
	else\
		cp osh_not_clone.1 osh.1;\
	fi

# Run a minimal test suite.
check: all
	@if test ! -e tests/.check_done; then /bin/sh tests/run.sh; fi

# Generate new reference logs for the tests.
check-newref: all
	@/bin/sh tests/run.sh -newref

clean:
	rm -f osh osh.o osh.1 if if.o goto goto.o tests/.check_done

install: all
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(BINDIR)
	test -d $(MANSECT) || mkdir -p $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(MANSECT)
