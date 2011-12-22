/*
 * osh - a reimplementation of the Sixth Edition (V6) Unix shell
 */
/*
 * Copyright (c) 2003, 2004
 *	Jeffrey Allen Neitzel.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jeffrey Allen Neitzel.
 * 4. Neither the name of Jeffrey Allen Neitzel nor the names of his
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)osh-041028: osh.h	1.2 (jneitzel) 2004/10/28
 */

#ifndef	OSH_H
#define	OSH_H

/*
 * This is the global header file for osh and sh6.
 * Osh is an enhanced, backward-compatible reimplementation of the
 * standard command interpreter from Sixth Edition Unix.  Sh6 is
 * simply a backward-compatible version of the shell (i.e., osh
 * w/o the enhancements).  See the manual pages for more info.
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
#define	FMTSIZE		64	/* used for messages printed by the shell */

#ifndef	SH6
/*
 * Initialization files for osh
 */
#define	_PATH_SYSTEM_LOGIN	"/etc/osh.login"
#define	_PATH_DOT_LOGIN		".osh.login"
#define	_PATH_DOT_OSHRC		".oshrc"

/*
 * Following standard conventions, file descriptors 0, 1, and 2 are used
 * for standard input, standard output, and standard error respectively.
 * In addition, the following file descriptors are reserved for special
 * use by osh.
 */
#define	DUPFD0		7	/* used for input redirection w/ `<-'	*/
#define	OWD		8	/* see do_chdir() in exec.c		*/
#define	SAVFD0		9	/* see do_source() in exec.c		*/
#endif	/* !SH6 */

/*
 * Action values for command parsing/execution
 */
#define	PX_SYN		0	/* check syntax only */
#define	PX_SYNEXEC	1	/* check syntax and execute if syntax is OK */
#define	PX_EXEC		2	/* execute only */

/*
 * Values for non-zero exit status
 */
#define	SH_ERR		1	/* common shell errors */
#define	SH_FATAL	2	/* fatal shell errors */

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
	char	*user;		/* login (or user) name		*/
	char	*tty;		/* terminal name		*/
	char	*path;		/* command search path		*/
	char	*home;		/* home (login) directory	*/
#endif	/* !SH6 */
};

/*
 * External variable declarations
 */
extern struct shinfo	si;
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

/*
 * Function prototypes
 */
/* osh.c */
void	cmd_loop(char *, int);
void	error(int, const char *, ...);
void	sh_write(int, const char *, ...);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);

/* parse.c */
int	pxline(char *, int);
char	*quote(char *);
int	substparm(char *);

/* exec.c */
void	apwait(int);
void	execute(char **, int *, int);

#endif	/* !OSH_H */
