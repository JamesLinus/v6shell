/*
 * osh - a reimplementation of the Sixth Edition (V6) Unix Thompson shell
 */
/*-
 * Copyright (c) 2003, 2004, 2005, 2006
 *	Jeffrey Allen Neitzel <jneitzel (at) sdf1 (dot) org>.
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
 *	@(#)osh.h	1.8 (jneitzel) 2006/01/20
 */

#ifndef	OSH_H
#define	OSH_H

/*
 * This is the global header file for osh(1), an enhanced,
 * backward-compatible reimplementation of the standard
 * command interpreter from Sixth Edition Unix.
 */

/*
 * Required headers
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Define DEBUG to add some additional debugging checks and diagnostics,
 * but note that it should always be undefined in production code.
 */
#undef	DEBUG

/*
 * Various constants
 */
#define	ARGMAX		512	/* 50   in the Sixth Edition Unix shell */
#define	LINEMAX		2048	/* 1000 ...                             */
#ifdef	_POSIX_OPEN_MAX
#define	NOFILE		_POSIX_OPEN_MAX
#else
#define	NOFILE		20
#endif
#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		1024
#endif

/*
 * Following standard conventions, file descriptors 0, 1, and 2 are used
 * for standard input, standard output, and standard error respectively.
 */
#define	FD0		STDIN_FILENO
#define	FD1		STDOUT_FILENO
#define	FD2		STDERR_FILENO

/*
 * The following file descriptors are reserved for special use by osh.
 */
#define	DUPFD0		7	/* used for input redirection w/ `<-' */
#define	OWD		8	/* see do_chdir() in osh-exec.c       */
#define	SAVFD0		9	/* see do_source() in osh-exec.c      */

/*
 * These are the initialization files for osh.
 * The `_PATH_DOT_*' files are in the user's HOME directory.
 */
#define	_PATH_SYSTEM_LOGIN	"/etc/osh.login"
#define	_PATH_DOT_LOGIN		".osh.login"
#define	_PATH_DOT_OSHRC		".oshrc"

/*
 * These are the nonstandard symbolic names for some of the type values
 * used by the fdtype() function.
 */
#define	FD_OPEN		1	/* Is descriptor any type of open file? */
#define	FD_DIROK	2	/* Is descriptor an existent directory? */

/*
 * Action values for the pxline() and pxpipe() functions
 */
#define	PX_PARSE	1	/* parse command line for correct syntax      */
#define	PX_EXEC		2	/* execute syntactically-correct command line */

/*
 * Option values for the spwait() function
 */
#define	SPW_DOZAP	1	/* zap processes w/o setting exit status */
#define	SPW_DOSBI	2	/* if end of pipeline is special command */

/*
 * Values for non-zero exit status and the error() function
 */
#define	SH_ERR		2	/* shell-detected error (e.g., syntax error) */
#ifdef	DEBUG
#define	SH_DEBUG	6	/* call abort(3) on debugging error          */
#endif

/*
 * Child interrupt flags
 */
#define	CH_SIGINT	01
#define	CH_SIGQUIT	02

/*
 * Command flags
 */
#define	CMD_ASYNC	01
#define	CMD_PIPE	02
#define	CMD_REDIR	04

/*
 * Shell type
 */
#define	CTOPTION	001
#define	COMMANDFILE	002
#define	INTERACTIVE	004
#define	RCFILE		010
#define	SOURCE		020
#define	PROMPT(t)	(((t) & 037) == INTERACTIVE)

/*
 * Diagnostics
 */
#define	ERR_FORK	0
#define	ERR_PIPE	1
#define	ERR_READ	2
#define	ERR_REDIR	3
#define	ERR_TRIM	4
#define	ERR_CLOVERFLOW	5
#define	ERR_BADARGLIST	6
#define	ERR_NOMATCH	7
#define	ERR_NOMEM	8
#define	ERR_SETID	9
#define	ERR_TMARGS	10
#define	ERR_TMCHARS	11
#define	ERR_ARGCOUNT	12
#define	ERR_BADDIR	13
#define	ERR_CREATE	14
#define	ERR_EXEC	15
#define	ERR_OPEN	16
#define	ERR_NOARGS	17
#define	ERR_NOSHELL	18
#define	ERR_NOTFOUND	19
#define	ERR_SYNTAX	20
#define	ERR_BADMASK	21
#define	ERR_BADNAME	22
#define	ERR_BADSIGNAL	23
#define	ERR_SIGIGN	24
#define	ERR_BADHOME	25
#define	ERR_BADOLD	26
#define	FMT1S		"%s"
#define	FMT2S		"%s: %s"
#define	FMT3S		"%s: %s: %s"

/*
 * Special built-in commands
 */
#define	SBINULL		0	/* :        */
#define	SBICHDIR	1	/* chdir    */
#define	SBIEXIT		2	/* exit     */
#define	SBILOGIN	3	/* login    */
#define	SBINEWGRP	4	/* newgrp   */
#define	SBISHIFT	5	/* shift    */
#define	SBIWAIT		6	/* wait     */
#define	SBISIGIGN	7	/* sigign   */
#define	SBISETENV	8	/* setenv   */
#define	SBIUNSETENV	9	/* unsetenv */
#define	SBIUMASK	10	/* umask    */
#define	SBISOURCE	11	/* source   */
#define	SBIEXEC		12	/* exec     */

/*
 * Macros
 */
#define	EQUAL(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)
#define	EXIT(s)		((getpid() == si.pid) ? exit((s)) : _exit((s)))

/*
 * This structure is for shell and user information.
 */
struct shinfo {
	const char	*name;	/* $0 - shell command name    */
	const char	*tty;	/* $t - terminal name         */
	const char	*user;	/* $u - effective user name   */
	pid_t		pid;	/* $$ - process ID of shell   */
	uid_t		euid;	/* effective user ID of shell */
};

/*
 * External variable declarations
 */
extern const char *const dgn[];
extern int		chintr;
extern int		dupfd0;
extern int		err_source;
extern int		ppac;
extern char	*const	*ppav;
extern int		shtype;
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
char	*quote(const char *);

/* osh-exec.c */
void	apwait(int);
int	execute(int, char **, int *, int);
void	spwait(int);

/* osh-misc.c */
void	close_pr(int *);
int	error(int, const char *, ...);
void	fdprint(int, const char *, ...);
int	fdtype(int, int);
int	sigmsg(int, pid_t);
void	*xmalloc(size_t);
char	*xstrdup(const char *);

#endif	/* OSH_H */
