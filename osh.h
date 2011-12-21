/*
 * osh - a reimplementation of the Sixth Edition (V6) Unix shell
 */
/*
 * Copyright (c) 2003, 2004
 *	Jeffrey Allen Neitzel <jneitzel@sdf.lonestar.org>.
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JEFFREY ALLEN NEITZEL ``AS IS'', AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL JEFFREY ALLEN NEITZEL BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)osh.h	1.3 (jneitzel) 2004/12/28
 */

#ifndef	OSH_H
#define	OSH_H

/*
 * This is the global header file for osh and sh6.
 * Osh is an enhanced, backward-compatible reimplementation of the
 * standard command interpreter from Sixth Edition Unix.  Sh6 is
 * simply a backward-compatible version of the shell (i.e., osh
 * w/o the enhancements).  For further info, see the shell manual
 * pages, osh(1) and sh6(1).
 */

/*
 * Headers required by the shell
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#ifndef	SH6
#include <limits.h>
#include <pwd.h>
#endif	/* !SH6 */
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*
 * Define DEBUG to add some additional debugging checks and diagnostics.
 * This should *always* be *undefined* in production code, but it might
 * be helpful when modifying the shell.
 */
#undef	DEBUG

/*
 * Constants
 */
#define	ARGMAX		256	/* 50   in the Sixth Edition Unix shell */
#define	LINESIZE	1024	/* 1000 ... */
#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		256	/* includes the terminating `\0' */
#endif
#define	PLSIZE		16	/* # of processes for *[as]plist allocations */

/*
 * Following standard conventions, file descriptors 0, 1, and 2 are used
 * for standard input, standard output, and standard error respectively.
 */
#define	FD0	STDIN_FILENO
#define	FD1	STDOUT_FILENO
#define	FD2	STDERR_FILENO

#ifndef	SH6
/*
 * Initialization files for osh
 */
#define	_PATH_SYSTEM_LOGIN	"/etc/osh.login"
#define	_PATH_DOT_LOGIN		".osh.login"
#define	_PATH_DOT_OSHRC		".oshrc"

/*
 * The following file descriptors are reserved for special use by osh.
 */
#define	DUPFD0		7	/* used for input redirection w/ `<-'	*/
#define	OWD		8	/* see do_chdir() in osh-exec.c		*/
#define	SAVFD0		9	/* see do_source() in osh-exec.c	*/
#endif	/* !SH6 */

/*
 * Action values for command-line parsing/execution
 */
#define	PX_PARSE	0	/* parse command line for correct syntax */
#define	PX_EXEC		1	/* execute syntactically-correct command line */

/*
 * Values for non-zero exit status
 */
#define	SH_ERR		1	/* common shell errors */
#define	SH_FATAL	2	/* fatal shell errors */
#ifdef	DEBUG
#define	SH_DEBUG	255	/* for the debugging checks */
#endif

/*
 * Command-type flags
 */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

/*
 * Shell type
 */
#define	INTERACTIVE	0x01
#define	COMMANDFILE	0x02
#define	OTHER		0x04
#define	RCFILE		0x10
#define	DOSOURCE	0x20
#define	TYPEMASK	0x37
#define	PROMPT(f)	((f & TYPEMASK) == INTERACTIVE)

/*
 * Interrupt flags
 */
#define	DO_SIGINT	01
#define	DO_SIGQUIT	02

/*
 * Macros
 */
#define	DOEXIT(x)	((getpid() == si.pid) ? exit(x) : _exit(x))
#define	EQUAL(a, b)	(strcmp(a, b) == 0)

/*
 * This structure is for shell and user information.
 */
struct shinfo {
	pid_t	pid;		/* process ID of shell		*/
	uid_t	euid;		/* effective user ID of shell	*/
	char	*name;		/* command name (used for $0)	*/
#ifndef	SH6
	char	*user;		/* effective user name		*/
	char	*tty;		/* terminal name		*/
	char	*path;		/* command search path		*/
	char	*home;		/* home (login) directory	*/
#endif	/* !SH6 */
};

/*
 * External variable declarations
 */
#ifndef	SH6
extern unsigned		clone;
extern int		dupfd0;
#endif	/* !SH6 */
extern int		apc;
extern int		aplim;
extern pid_t		*aplist;
extern pid_t		apstat;
extern int		dointr;
extern int		errflg;
extern int		posac;
extern char		**posav;
extern int		shtype;
extern int		spc;
extern int		splim;
extern pid_t		*splist;
extern int		status;
extern struct shinfo	si;

/*
 * Function prototypes
 */
/* osh.c */
void	cmd_loop(int);

/* osh-parse.c */
void	execline(char *);
int	pxline(char *, int);
char	*quote(char *);

/* osh-exec.c */
void	execute(char **, int *, int);

/* osh-misc.c */
void	apwait(int);
void	error(int, const char *, ...);
void	fdprint(int, const char *, ...);
int	isopen(int);
void	seek_eof(void);
void	sigmsg(int, pid_t);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);

#endif	/* OSH_H */
