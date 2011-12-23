# Makefile fuer osh

# CONFIGURATION

PREFIX     = /usr/local
BINDIR     = $(PREFIX)/bin
MANDIR     = $(PREFIX)/man
MANSECT    = $(MANDIR)/man1
INSTALL	   = /usr/ucb/install

# CONFIGURATION ENDS

XOPF       = -D_XOPEN_SOURCE=500L

.c.o: ; $(CC) -c $(CFLAGS) $(CPPFLAGS) $(XOPF) $(WARN) $<

all: osh goto if

osh: osh.o
	$(CC) $(LDFLAGS) osh.o $(LIBS) -o osh

goto: goto.o
	$(CC) $(LDFLAGS) goto.o $(LIBS) -o goto

if: if.o
	$(CC) $(LDFLAGS) if.o $(LIBS) -o if

clean:
	rm -f osh osh.o goto goto.o if if.o core log *~

install: all
	$(INSTALL) -c -s osh $(BINDIR)
	$(INSTALL) -c -s goto $(BINDIR)
	$(INSTALL) -c -s if $(BINDIR)
	$(INSTALL) -c -m 644 osh.1 $(MANSECT)
	$(INSTALL) -c -m 644 goto.1 $(MANSECT)
	$(INSTALL) -c -m 644 if.1 $(MANSECT)
