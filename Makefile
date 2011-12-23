# Makefile for osh-030730

# CONFIGURATION
#
# Uncomment the following line (i.e., define CLONE) if you want osh
# and if to try locating external commands in the current directory
# before searching PATH.  This is actually the historically correct
# behaviour.  However, I have chosen to only search PATH by default.
# Define CLONE for historical accuracy.  However, the same effect
# can be achieved by simply prepending . (dot) to PATH at runtime.

#CLONE=		-DCLONE
CFLAGS+=	-Wall
PREFIX=		/usr/local
BINDIR=		$(PREFIX)/bin
MANDIR=		$(PREFIX)/man
MANSECT=	$(MANDIR)/man1
INSTALL=	/usr/bin/install
#BINGRP=		-g bin
BINMODE=	-m 0555
#MANGRP=		-g bin
MANMODE=	-m 0444

# CONFIGURATION ENDS

XOPF=		-D_XOPEN_SOURCE=500L

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) $(CLONE) $(WARN) $<

all: osh goto if man

osh: osh.o
	$(CC) $(LDFLAGS) osh.o $(LIBS) -o osh

goto: goto.o
	$(CC) $(LDFLAGS) goto.o $(LIBS) -o goto

if: if.o
	$(CC) $(LDFLAGS) if.o $(LIBS) -o if

man: osh_src.1
	@if test -z "$(CLONE)"; then \
		rm -f osh.1; sed 69d osh_src.1 > osh.1; \
	else \
		rm -f osh.1; cp osh_src.1 osh.1; \
	fi

clean:
	rm -f osh osh.o osh.1 goto goto.o if if.o

install: all
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto $(BINDIR)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if $(BINDIR)
	$(INSTALL) -c $(MANGRP) $(MANMODE) osh.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) goto.1 $(MANSECT)
	$(INSTALL) -c $(MANGRP) $(MANMODE) if.1 $(MANSECT)

