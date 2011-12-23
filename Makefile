# Makefile for osh-031012

# Begin CONFIGURATION
#
# Uncomment the following line (i.e., define CLONE) if you want osh(1)
# and if(1) to try locating external commands in the current directory
# before searching PATH.  This is the historically correct behaviour,
# but I have chosen to only search PATH by default.  Simply prepending
# . (dot) to PATH at runtime will have the same effect as defining CLONE
# but also gives users the flexibility to change PATH at any time.
#
# Also, define CLONE to give chdir the original behaviour; otherwise,
# chdir is enhanced to be more like modern shells and allows 'chdir'
# to HOME directory, 'chdir -' to previous directory, and the normal
# 'chdir dir' to any other directory.
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
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(MANSECT)

