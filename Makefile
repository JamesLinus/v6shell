# Makefile for osh-031112

# Begin CONFIGURATION
#
# The original Sixth Edition shell and if(1) always used '.:/bin:/usr/bin'
# as the sequence of directories to search for external commands.
#
# Defining CLONE will produce osh(1) and if(1) binaries which approximate
# the original behaviour as closely as possible.  CLONE is not defined by
# default.
#
# Defining CLONE will use '.:$PATH' to search for external commands;
# otherwise, only '$PATH' will be used.
#
# Defining CLONE also gives chdir the original behaviour; otherwise,
# chdir is enhanced to be more like modern shells as below:
#
#	'chdir'		change to HOME directory
#	'chdir -'	change to previous directory
#	'chdir dir'	change to 'dir'
#
# If you want CLONE behaviour, simply uncomment the '#CLONE=' line below
# by removing the '#' character.
#
#CLONE=		-DCLONE

# Compiler flags
#
CFLAGS+=	-Wall

# Choose where and how to install the binaries and man pages.
#
PREFIX=		/usr/local
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
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) $(CLONE) $(WARN) $<

all: osh goto if

osh: osh.o
	$(CC) $(LDFLAGS) -o osh osh.o $(LIBS)

goto: goto.o
	$(CC) $(LDFLAGS) -o goto goto.o $(LIBS)

if: if.o
	$(CC) $(LDFLAGS) -o if if.o $(LIBS)

man: osh.1.src
	@if test -z "$(CLONE)"; then \
		rm -f osh.1; sed 73d osh.1.src > osh.1; \
	else \
		rm -f osh.1; cp osh.1.src osh.1; \
	fi

clean:
	rm -f osh osh.o osh.1 goto goto.o if if.o

install: all man
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(BINDIR)
	test -d $(MANSECT) || mkdir -p $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(MANSECT)

