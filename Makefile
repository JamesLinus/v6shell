# Makefile for etsh
#
# @(#)$Id$
#
# Begin CONFIGURATION
#
# See the INSTALL file for build and install instructions.

#
# Choose where and how to install the binaries and manual pages.
#
DESTDIR?=
PREFIX?=	/usr/local
BINDIR?=	$(PREFIX)/bin
LIBEXECDIR?=	$(PREFIX)/libexec/$(OSH_VERSION)
LIBEXECDIROSH?=	$(PREFIX)/libexec/$(OSH_VERSION)/etsh
LIBEXECDIRSH6?=	$(PREFIX)/libexec/$(OSH_VERSION)/tsh
DOCDIR?=	$(PREFIX)/share/doc/etsh
EXPDIR?=	$(PREFIX)/share/examples/etsh
MANDIR?=	$(PREFIX)/man/man1
SYSCONFDIR?=	$(PREFIX)/etc
#BINGRP=		-g bin
BINMODE=	-m 0555
#MANGRP=		-g bin
MANMODE=	-m 0444

#
# Build utilities (SHELL must be POSIX-compliant)
#
INSTALL?=	/usr/bin/install
SHELL=		/bin/sh

#
# Preprocessor, compiler, and linker flags
#
#	If the compiler gives errors about any of flags specified
#	by `OPTIONS' or `WARNINGS' below, comment the appropriate
#	line(s) with a `#' character to fix the compiler errors.
#	Then, try to build again by doing a `make clean ; make'.
#
#CPPFLAGS=
OPTIONS=	-std=c99 -pedantic
#OPTIONS+=	-fstack-protector-strong
WARNINGS=	-Wall -Wextra
#WARNINGS+=	-Wstack-protector
#CFLAGS+=	-g
CFLAGS+=	$(OPTIONS) $(WARNINGS)
#LDFLAGS+=	-static

#
# End CONFIGURATION
#
# !!! ================= Developer stuff below... ================= !!!
#

#
# Adjust CFLAGS for OSXCFLAGS and LDFLAGS for OSXLDFLAGS if needed.
#
OSXCFLAGS?=
CFLAGS+=	$(OSXCFLAGS)
OSXLDFLAGS?=
LDFLAGS+=	$(OSXLDFLAGS)

#
# Include OSH_DATE and OSH_VERSION macros.
include	./Makefile.config
# Include osh and sh6 binary name macros.
include	./mkconfig.tmp
#

OSH=	osh
SH6=	sh6 glob
UBIN=	fd2 goto if
GHEAD=	config.h defs.h
ERRSRC=	err.h err.c
LIBSRC=	lib.h lib.c
PEXSRC=	pexec.h pexec.c
SIGSRC=	sasignal.h sasignal.c
INTSRC=	strtoint.h strtoint.c
OBJ=	err.o fd2.o glob.o goto.o if.o lib.o osh.o pexec.o sasignal.o sh6.o strtoint.o util.o v.o
OSHMAN=	osh.1.out
SH6MAN=	sh6.1.out glob.1.out
UMAN=	fd2.1.out goto.1.out if.1.out
MANALL=	$(OSHMAN) $(SH6MAN) $(UMAN)
SEDSUB=	-e 's|@OSH_DATE@|$(OSH_DATE)|' \
	-e 's|@OSH_VERSION@|$(OSH_VERSION)|' \
	-e 's|@LIBEXECDIROSH@|$(LIBEXECDIROSH)|' \
	-e 's|@LIBEXECDIRSH6@|$(LIBEXECDIRSH6)|' \
	-e 's|@OBN@|$(OBN)|g' -e 's|@OBN1@|$(OBN1)|' -e 's|@OBNC@|$(OBNC)|' \
	-e 's|@SBN@|$(SBN)|' -e 's|@SBN1@|$(SBN1)|' -e 's|@SBNC@|$(SBNC)|' \
	-e 's|@SYSCONFDIR@|$(SYSCONFDIR)|'

DEFS=	-DOSH_VERSION='"$(OSH_VERSION)"' -DLIBEXECDIROSH='"$(LIBEXECDIROSH)"' -DLIBEXECDIRSH6='"$(LIBEXECDIRSH6)"' -DSYSCONFDIR='"$(SYSCONFDIR)"' -DSH6_BINARY_NAME='"$(SBN)"'

.SUFFIXES: .1 .1.out .c .o

.1.1.out:
	sed $(SEDSUB) <$< >$@

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFS) $<

#
# Build targets
#
all: oshall sh6all

oshall: $(OSH) $(OSHMAN) $(UMAN)

sh6all: $(SH6) $(SH6MAN) utils

utils: $(UBIN) $(UMAN)

man: $(MANALL)

osh: sh.h v.c osh.c util.c $(GHEAD) $(ERRSRC) $(LIBSRC) $(PEXSRC) $(SIGSRC) $(INTSRC)
	@$(MAKE) $@bin

sh6: sh.h v.c sh6.c $(GHEAD) $(ERRSRC) $(PEXSRC) $(SIGSRC)
	@$(MAKE) $@bin

glob: v.c glob.c $(GHEAD) $(ERRSRC) $(PEXSRC)
	@$(MAKE) $@bin

if: v.c if.c $(GHEAD) $(ERRSRC) $(PEXSRC) $(SIGSRC) $(INTSRC)
	@$(MAKE) $@bin

goto: v.c goto.c $(GHEAD) $(ERRSRC)
	@$(MAKE) $@bin

fd2: v.c fd2.c $(GHEAD) $(ERRSRC) $(PEXSRC)
	@$(MAKE) $@bin

$(OBJ)                                                              : $(GHEAD)
fd2.o glob.o goto.o if.o lib.o osh.o pexec.o sh6.o util.o strtoint.o: err.h
fd2.o glob.o if.o osh.o sh6.o util.o                                : pexec.h
if.o osh.o sh6.o                                                    : sasignal.h
if.o osh.o util.o                                                   : strtoint.h
osh.o sh6.o util.o                                                  : sh.h
err.o                                                               : $(ERRSRC)
lib.o                                                               : $(LIBSRC)
pexec.o                                                             : $(PEXSRC)
sasignal.o                                                          : $(SIGSRC)
strtoint.o                                                          : $(INTSRC)

config.h: Makefile Makefile.config configure mkconfig mkconfig.tmp
	@if test ! -e config.h ; then echo 'Please run ./configure first.' >&2 ; exit 1 ; fi

oshbin: v.o osh.o err.o lib.o util.o pexec.o sasignal.o strtoint.o
	$(CC) $(LDFLAGS) -o osh v.o osh.o err.o lib.o util.o pexec.o sasignal.o strtoint.o $(LIBS)

sh6bin: v.o sh6.o err.o pexec.o sasignal.o
	$(CC) $(LDFLAGS) -o sh6 v.o sh6.o err.o pexec.o sasignal.o $(LIBS)

globbin: v.o glob.o err.o pexec.o
	$(CC) $(LDFLAGS) -o glob v.o glob.o err.o pexec.o $(LIBS)

ifbin: v.o if.o err.o pexec.o sasignal.o strtoint.o
	$(CC) $(LDFLAGS) -o if v.o if.o err.o pexec.o sasignal.o strtoint.o $(LIBS)

gotobin: v.o goto.o err.o
	$(CC) $(LDFLAGS) -o goto v.o goto.o err.o $(LIBS)

fd2bin: v.o fd2.o err.o pexec.o
	$(CC) $(LDFLAGS) -o fd2 v.o fd2.o err.o pexec.o $(LIBS)

$(MANALL): config.h

#
# Test targets
#
# NOTE: $(.CURDIR) for BSD make || $(CURDIR) for GNU make
#
check-pre: $(OSH) clean-sh6
	@if test X$(.CURDIR) != X ; then $(MAKE) LIBEXECDIRSH6=$(.CURDIR) sh6all ; else $(MAKE) LIBEXECDIRSH6=$(CURDIR) sh6all ; fi >/dev/null 2>&1

check: check-pre
	@( trap '' INT QUIT && cd tests && ../osh run osh sh6 )
	@$(MAKE) check-post

check-oshall: check-osh
check-osh: $(OSH)
	@( trap '' INT QUIT && cd tests && ../osh run osh )

check-sh6all: check-sh6
check-sh6: check-pre
	@( trap '' INT QUIT && cd tests && ../osh run sh6 )
	@$(MAKE) check-post

check-newlog: check-pre
	@( trap '' INT QUIT && cd tests && ../osh run -newlog osh sh6 )
	@$(MAKE) check-post

check-post: $(OSH) clean-sh6
	@$(MAKE) sh6all >/dev/null 2>&1

#
# Install targets
#
DESTBINDIR=		$(DESTDIR)$(BINDIR)
DESTLIBEXECDIR=		$(DESTDIR)$(LIBEXECDIR)
DESTLIBEXECDIROSH=	$(DESTDIR)$(LIBEXECDIROSH)
DESTLIBEXECDIRSH6=	$(DESTDIR)$(LIBEXECDIRSH6)
DESTDOCDIR=		$(DESTDIR)$(DOCDIR)
DESTEXPDIR=		$(DESTDIR)$(EXPDIR)
DESTMANDIR=		$(DESTDIR)$(MANDIR)
install: install-oshall install-sh6all

install-oshall: oshall install-osh install-uman

install-sh6all: sh6all install-sh6 install-utils

install-utils: install-ubin install-uman

install-osh: $(OSH) $(OSHMAN) install-dest install-destlibexecosh
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) osh        $(DESTBINDIR)/$(OBN)
	$(INSTALL) -c    $(MANGRP) $(MANMODE) osh.1.out  $(DESTMANDIR)/$(OBN).1

install-sh6: $(SH6) $(SH6MAN) install-dest install-destlibexecsh6
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) sh6        $(DESTBINDIR)/$(SBN)
	$(INSTALL) -c    $(MANGRP) $(MANMODE) sh6.1.out  $(DESTMANDIR)/$(SBN).1
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) glob       $(DESTLIBEXECDIRSH6)
	$(INSTALL) -c    $(MANGRP) $(MANMODE) glob.1.out $(DESTMANDIR)/glob.1

install-ubin: $(UBIN) install-dest install-destlibexec
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) fd2        $(DESTLIBEXECDIRSH6)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) goto       $(DESTLIBEXECDIRSH6)
	$(INSTALL) -c -s $(BINGRP) $(BINMODE) if         $(DESTLIBEXECDIRSH6)

install-uman: $(UMAN) install-dest
	$(INSTALL) -c    $(MANGRP) $(MANMODE) fd2.1.out  $(DESTMANDIR)/fd2.1
	$(INSTALL) -c    $(MANGRP) $(MANMODE) goto.1.out $(DESTMANDIR)/goto.1
	$(INSTALL) -c    $(MANGRP) $(MANMODE) if.1.out   $(DESTMANDIR)/if.1

install-dest:
	test -d $(DESTBINDIR) || { umask 0022 && mkdir -p $(DESTBINDIR) ; }
	test -d $(DESTMANDIR) || { umask 0022 && mkdir -p $(DESTMANDIR) ; }

install-destlibexec:
	test -d $(DESTLIBEXECDIR) || { umask 0022 && mkdir -p $(DESTLIBEXECDIR) ; }
	$(INSTALL) -c $(MANGRP) $(MANMODE) README.libexec $(DESTLIBEXECDIR)/README

install-destlibexecosh: install-destlibexec
	test -d $(DESTLIBEXECDIROSH) || { umask 0022 && mkdir -p $(DESTLIBEXECDIROSH) ; }
	$(INSTALL) -c $(MANGRP) $(MANMODE) libexec.etsh/README $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(MANGRP) $(MANMODE) libexec.etsh/SetP $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(MANGRP) $(MANMODE) libexec.etsh/SetTandCTTY $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(MANGRP) $(MANMODE) libexec.etsh/SetV $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(BINGRP) $(BINMODE) libexec.etsh/etshdir $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(BINGRP) $(BINMODE) libexec.etsh/history $(DESTLIBEXECDIROSH)
	$(INSTALL) -c $(MANGRP) $(MANMODE) libexec.etsh/history.help $(DESTLIBEXECDIROSH)

install-destlibexecsh6: install-destlibexec
	test -d $(DESTLIBEXECDIRSH6) || { umask 0022 && mkdir -p $(DESTLIBEXECDIRSH6) ; }
	$(INSTALL) -c $(MANGRP) $(MANMODE) README.libexec.tsh $(DESTLIBEXECDIRSH6)/README

install-doc:
	test -d $(DESTDOCDIR) || { umask 0022 && mkdir -p    $(DESTDOCDIR) ; }
	$(INSTALL) -c    $(MANGRP) $(MANMODE) [ACDILNP]* R*E $(DESTDOCDIR)

#
# Notice that doing a:
#
#	make [variable=value ...] EXPDIR=expdir install-exp
#
# is a simple way to install your example .etsh* and etsh* files
# into expdir here in the source tree on the fly.
#
install-exp: config.h
	cd examples && $(SHELL) ready_rc_files $(OBN) $(PREFIX) $(BINDIR) $(SYSCONFDIR)
	test -d $(DESTEXPDIR) || { umask 0022 && mkdir -p $(DESTEXPDIR) ; }
	$(INSTALL) -c    $(MANGRP) $(MANMODE) examples.install/.$(OBN)* examples.install/$(OBN)* $(DESTEXPDIR)
	rm -rf examples.install

#
# Cleanup targets
#
clean-sh6: clean-obj
	@rm -f $(SH6) $(UBIN)
clean-man:
	@rm -f $(MANALL)
clean-obj:
	@rm -f $(OBJ)
clean: clean-sh6 clean-man clean-obj
	@rm -f $(OSH) mkconfig.tmp config.h
	@touch mkconfig.tmp
