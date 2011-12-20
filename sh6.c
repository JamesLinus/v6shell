/*
 * sh6 - a port of the Sixth Edition (V6) Unix Thompson shell
 */
/*-
 * Copyright (c) 2004, 2006
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
 *	@(#)sh6.c	1.2 (jneitzel) 2006/01/09
 */
/*
 *	Derived from: Sixth Edition Unix /usr/source/s2/sh.c
 */
/*-
 * Copyright (C) Caldera International Inc.  2001-2002.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	lint
#include "version.h"
OSH_SOURCEID("sh6.c	1.2 (jneitzel) 2006/01/09");
#endif	/* !lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pexec.h"

#define	EQUAL(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#define	LINSIZ		1000
#define	ARGSIZ		50
#define	TRESIZ		100
#ifdef	_POSIX_OPEN_MAX
#define	NOFILE		_POSIX_OPEN_MAX
#else
#define	NOFILE		20
#endif

#define	QUOTE		0200
#define	ASCII		0177

#define	FAND		0001
#define	FCAT		0002
#define	FPIN		0004
#define	FPOU		0010
#define	FPAR		0020
#define	FINT		0040
#define	FPRS		0100

#define	TCOM		1
#define	TPAR		2
#define	TFIL		3
#define	TLST		4

#define	DTYP		0
#define	DLEF		1
#define	DRIT		2
#define	DFLG		3
#define	DSPR		4
#define	DCOM		5

#define	XNSIG		14
static const char *const mesg[XNSIG] = {
	NULL,
	"Hangup",
	NULL,
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"IOT trap",
	"EMT trap",
	"Floating exception",
	"Killed",
	"Bus error",
	"Memory fault",
	"Bad system call",
	NULL
};

static char		*arginp;
static char		**argp;
static int		dolc;
static char		*dolp;
static char		**dolv;
static sigjmp_buf	errjmp;
static int		error;
static int		gflg;
static char		*linep;
static int		onelflg;
static char		peekc;
static const char	*promp;
static int		setintr;
static pid_t		shpid;		/* actual process ID of shell   */
static char		shpidstr[6];	/* for 5-digit ASCII process ID */
static int		status;
static intptr_t		*treep;

static char	line[LINSIZ];
static char	*args[ARGSIZ];
static intptr_t	trebuf[TRESIZ];

static void	main1(void);
static void	word(void);
static int	xgetc(int);
static int	readc(void);
static intptr_t	*tree(int);
static intptr_t	*syntax(char **, char **);
static intptr_t	*syn1(char **, char **);
static intptr_t	*syn2(char **, char **);
static intptr_t	*syn3(char **, char **);
static void	scan(intptr_t *, int (*)(int));
static int	tglob(int);
static int	trim(int);
static void	execute(intptr_t *, int *, int *);
static void	err(const char *, int);
static void	prs(const char *);
static void	xputc(int);
static void	prn(int);
static int	any(int, const char *);
static void	pwait(pid_t);
static void	sh_init(void);

int
main(int argc, char **argv)
{

	sh_init();
	if (argv[0] == NULL || *argv[0] == '\0')
		err("Invalid argument list", 2);

	if (argc > 1) {
		if (*argv[1] == '-') {
			setintr = 1;
			if (argv[1][1] == 'c' && argc > 2)
				arginp = argv[2];
			else if (argv[1][1] == 't')
				onelflg = 2;
		} else {
			close(STDIN_FILENO);
			if (open(argv[1], O_RDONLY, 0) != STDIN_FILENO) {
				prs(argv[1]);
				err(": cannot open", 2);
			}
		}
	} else {
		setintr = 1;
		if (isatty(STDIN_FILENO) && isatty(STDERR_FILENO))
			promp = geteuid() ? "% " : "# ";
	}
	if (setintr) {
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if (promp != NULL)
			signal(SIGTERM, SIG_IGN);
	}
	dolv = argv + 1;
	dolc = argc - 1;

loop:
	if (promp != NULL)
		prs(promp);
	main1();
	goto loop;

	/* NOTREACHED */
	return status;
}

static void
main1(void)
{
	intptr_t *t = NULL;
	char *wp;

	argp  = args;
	linep = line;
	error = gflg = 0;
	do {
		wp = linep;
		word();
	} while (*wp != '\n');
	treep = trebuf;
	if (gflg == 0) {
		if (error == 0) {
			sigsetjmp(errjmp, 1);
			if (error != 0) {
				err("Command line overflow", 2);
				return;
			}
			t = syntax(args, argp);
		}
		if (error != 0)
			err("syntax error", 2);
		else
			execute(t, NULL, NULL);
	}
}

static void
word(void)
{
	char c, c1;

	*argp++ = linep;

loop:
	switch (c = xgetc(1)) {
	case ' ':
	case '\t':
		goto loop;

	case '"':
	case '\'':
		c1 = c;
		while ((c = xgetc(0)) != c1) {
			if (c == '\n') {
				error++;
				peekc = c;
				*linep++ = '\0';
				return;
			}
			*linep++ = c | QUOTE;
		}
		break;

	case '\\':
		if ((c = xgetc(0)) == '\n')
			goto loop;
		c |= QUOTE;
		peekc = c;
		break;

	case '&':
	case ';':
	case '<':
	case '>':
	case '(':
	case ')':
	case '|':
	case '^':
	case '\n':
		*linep++ = c;
		*linep++ = '\0';
		return;

	default:
		peekc = c;
	}

	for (;;) {
		if ((c = xgetc(1)) == '\\') {
			if ((c = xgetc(0)) == '\n')
				c = ' ';
			else
				c |= QUOTE;
		}
		if (any(c, " \t\"';&<>()|^\n")) {
			peekc = c;
			if (any(c, "\"'"))
				goto loop;
			*linep++ = '\0';
			return;
		}
		*linep++ = c;
	}
}

static int
xgetc(int dsub)
{
	char c;

	if (peekc != '\0') {
		c = peekc;
		peekc = '\0';
		return c;
	}
	if (argp > &args[ARGSIZ - 5]) {
		argp -= 10;
		while (xgetc(0) != '\n')
			;	/* nothing */
		argp += 10;
		err("Too many args", 2);
		gflg++;
		return '\n';
	}
	if (linep > &line[LINSIZ - 5]) {
		linep -= 10;
		while ((c = xgetc(0)) != '\n' && c != '\0')
			;	/* nothing */
		linep += 10;
		err("Too many characters", 2);
		gflg++;
		return '\n';
	}
getd:
	if (dolp != NULL) {
		c = *dolp++;
		if (c != '\0')
			return c;
		dolp = NULL;
	}
	c = readc();
	if (c == '$' && dsub) {
		c = readc();
		if (c >= '0' && c <= '9') {
			if (c - '0' < dolc)
				dolp = dolv[c - '0'];
			goto getd;
		}
		if (c == '$') {
			dolp = shpidstr;
			goto getd;
		}
	}
	return c & ASCII;
}

static int
readc(void)
{
	ssize_t rr;
	char c;

	if (arginp != NULL) {
		if (arginp == (char *)-1)
			exit(status);
		if ((c = *arginp++) == '\0') {
			arginp = (char *)-1;
			c = '\n';
		}
		return (int)c;
	}
	if (onelflg == 1)
		exit(status);
	if ((rr = read(STDIN_FILENO, &c, (size_t)1)) != 1) {
		if (rr == 0)
			exit(status);
		else
			exit(2);
	}
	if (c == '\n' && onelflg)
		onelflg--;
	return (int)c;
}

intptr_t *
tree(int n)
{
	intptr_t *t;

	t = treep;
	treep += n;
	if (treep >= &trebuf[TRESIZ]) {
		error++;
		siglongjmp(errjmp, 1);
	}
	return t;
}

/*
 * syntax
 *	empty
 *	syn1
 */
static intptr_t *
syntax(char **p1, char **p2)
{

	while (p1 != p2)
		if (any(**p1, ";&\n"))
			p1++;
		else
			return syn1(p1, p2);
	return NULL;
}

/*
 * syn1
 *	syn2
 *	syn2 & syntax
 *	syn2 ; syntax
 */
static intptr_t *
syn1(char **p1, char **p2)
{
	char **p;
	intptr_t *t, *t1;
	int l;

	for (l = 0, p = p1; p != p2; p++)
		switch (**p) {
		case '(':
			l++;
			continue;

		case ')':
			l--;
			if (l < 0)
				error++;
			continue;

		case '&':
		case ';':
		case '\n':
			if (l == 0) {
				l = **p;
				t = tree(DSPR);
				t[DTYP] = TLST;
				t[DLEF] = (intptr_t)syn2(p1, p);
				t[DFLG] = 0;
				if (l == '&') {
					t1 = (intptr_t *)t[DLEF];
					t1[DFLG] |= FAND | FPRS | FINT;
				}
				t[DRIT] = (intptr_t)syntax(p + 1, p2);
				return t;
			}
		}

	if (l == 0)
		return syn2(p1, p2);
	error++;
	return NULL;
}

/*
 * syn2
 *	syn3
 *	syn3 | syn2
 */
static intptr_t *
syn2(char **p1, char **p2)
{
	char **p;
	intptr_t *t;
	int l;

	for (l = 0, p = p1; p != p2; p++)
		switch (**p) {
		case '(':
			l++;
			continue;

		case ')':
			l--;
			continue;

		case '|':
		case '^':
			if (l == 0) {
				t = tree(DSPR);
				t[DTYP] = TFIL;
				t[DLEF] = (intptr_t)syn3(p1, p);
				t[DRIT] = (intptr_t)syn2(p + 1, p2);
				t[DFLG] = 0;
				return t;
			}
		}

	return syn3(p1, p2);
}

/*
 * syn3
 *	( syn1 ) [ < in ] [ > out ]
 *	  word*  [ < in ] [ > out ]
 */
static intptr_t *
syn3(char **p1, char **p2)
{
	char **lp, **p, **rp;
	intptr_t *t;
	intptr_t flg, i, o;
	int c, l, n;

	flg = 0;
	if (**p2 == ')')
		flg |= FPAR;

	for (i = o = 0, l = n = 0, lp = rp = NULL, p = p1; p != p2; p++)
		switch (c = **p) {
		case '(':
			if (l == 0) {
				if (lp != NULL)
					error++;
				lp = p + 1;
			}
			l++;
			continue;

		case ')':
			l--;
			if (l == 0)
				rp = p;
			continue;

		case '>':
			p++;
			if (p != p2 && **p == '>')
				flg |= FCAT;
			else
				p--;
			/* FALLTHROUGH */

		case '<':
			if (l == 0) {
				p++;
				if (p == p2) {
					error++;
					p--;
				}
				if (any(**p, "<>("))
					error++;
				if (c == '<') {
					if (i != 0)
						error++;
					i = (intptr_t)*p;
					continue;
				}
				if (o != 0)
					error++;
				o = (intptr_t)*p;
			}
			continue;

		default:
			if (l == 0)
				p1[n++] = *p;
		}

	if (lp != NULL) {
		if (n != 0)
			error++;
		t = tree(DCOM);
		t[DTYP] = TPAR;
		t[DSPR] = (intptr_t)syn1(lp, rp);
		goto out;
	}
	if (n == 0)
		error++;
	p1[n++] = NULL;
	t = tree(n + DCOM);
	t[DTYP] = TCOM;
	for (l = 0; l < n; l++)
		t[l + DCOM] = (intptr_t)p1[l];

out:
	t[DFLG] = flg;
	t[DLEF] = i;
	t[DRIT] = o;
	return t;
}

static void
scan(intptr_t *at, int (*f)(int))
{
	char *p, c;
	intptr_t *t;

	t = at + DCOM;
	while ((p = (char *)*t++) != NULL)
		while ((c = *p) != '\0')
			*p++ = (*f)(c);
}

static int
tglob(int c)
{

	if (any(c, "*?["))
		gflg = 1;
	return c;
}

static int
trim(int c)
{

	return c & ASCII;
}

static void
execute(intptr_t *t, int *pf1, int *pf2)
{
	pid_t cpid;
	int i, f, pv[2];
	intptr_t *t1;
	const char *cmd;

	if (t == NULL)
		return;

	switch (t[DTYP]) {
	case TCOM:
		cmd = (char *)t[DCOM];
		if (EQUAL(cmd, ":")) {
			status = 0;
			return;
		}
		if (EQUAL(cmd, "chdir")) {
			if (t[DCOM + 1] == 0) {
				err("chdir: arg count", 2);
				return;
			}
			if (chdir((char *)t[DCOM + 1]) == -1)
				err("chdir: bad directory", 2);
			else
				status = 0;
			return;
		}
		if (EQUAL(cmd, "exit")) {
			if (promp == NULL) {
				lseek(STDIN_FILENO, (off_t)0, SEEK_END);
				if (getpid() != shpid)
					_exit(status);
				exit(status);
			}
			return;
		}
		if (EQUAL(cmd, "login")) {
			if (promp != NULL) {
				signal(SIGINT, SIG_DFL);
				signal(SIGQUIT, SIG_DFL);
				pexec(_PATH_LOGIN, (char **)t + DCOM);
				signal(SIGINT, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
			}
			err("login: cannot execute", 2);
			return;
		}
		if (EQUAL(cmd, "newgrp")) {
			if (promp != NULL) {
				signal(SIGINT, SIG_DFL);
				signal(SIGQUIT, SIG_DFL);
				pexec(_PATH_NEWGRP, (char **)t + DCOM);
				signal(SIGINT, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
			}
			err("newgrp: cannot execute", 2);
			return;
		}
		if (EQUAL(cmd, "shift")) {
			if (dolc <= 1) {
				err("shift: no args", 2);
				return;
			}
			dolv[1] = dolv[0];
			dolv++;
			dolc--;
			status = 0;
			return;
		}
		if (EQUAL(cmd, "wait")) {
			pwait(-1);
			return;
		}
		/* FALLTHROUGH */

	case TPAR:
		f = t[DFLG];
		cpid = 0;
		if ((f & FPAR) == 0)
			cpid = fork();
		if (cpid == -1) {
			err("try again", 2);
			return;
		}
		/* Parent process */
		if (cpid != 0) {
			if ((f & FPIN) != 0) {
				close(pf1[0]);
				close(pf1[1]);
			}
			if ((f & FPRS) != 0) {
				prn(cpid);
				prs("\n");
			}
			if ((f & FAND) != 0)
				return;
			if ((f & FPOU) == 0)
				pwait(cpid);
			return;
		}
		/* Child process */
		/*
		 * Redirect input from file.
		 *
		 * Fixed oddity or bug (see sh6(1)):
		 * Do not attempt to open `file' in a pipeline of
		 * the following form:
		 *
		 *	% echo hello | cat <file | grep h
		 *
		 * ...
		 * since `cat' will never read from the opened file.
		 * This fix silently ignores the ambiguous redirect.
		 */
		if (t[DLEF] != 0 && (f & FPIN) == 0) {
			if ((i = open((char *)t[DLEF], O_RDONLY, 0)) == -1) {
				prs((char *)t[DLEF]);
				err(": cannot open", 2);
			}
			if (dup2(i, STDIN_FILENO) == -1)
				err("dup2 failed", 2);
			close(i);
		}
		/*
		 * Redirect output to file.
		 *
		 * Fixed oddity or bug (see sh6(1)):
		 * Do not attempt to create `file' in a pipeline of
		 * the following form:
		 *
		 *	% echo hello | cat >file | grep h
		 *
		 * ...
		 * since `cat' will never write to the created file.
		 * This fix silently ignores the ambiguous redirect.
		 */
		if (t[DRIT] != 0 && (f & FPOU) == 0) {
			if ((f & FCAT) != 0)
				i = O_WRONLY | O_APPEND | O_CREAT;
			else
				i = O_WRONLY | O_TRUNC | O_CREAT;
			if ((i = open((char *)t[DRIT], i, 0666)) == -1) {
				prs((char *)t[DRIT]);
				err(": cannot create", 2);
			}
			if (dup2(i, STDOUT_FILENO) == -1)
				err("dup2 failed", 2);
			close(i);
		}
		/* Read input from pipe. */
		if ((f & FPIN) != 0) {
			if (dup2(pf1[0], STDIN_FILENO) == -1)
				err("dup2 failed", 2);
			close(pf1[0]);
			close(pf1[1]);
		}
		/* Write output to pipe. */
		if ((f & FPOU) != 0) {
			if (dup2(pf2[1], STDOUT_FILENO) == -1)
				err("dup2 failed", 2);
			close(pf2[0]);
			close(pf2[1]);
		}
		/*
		 * Redirect input from `/dev/null' for `&' commands.
		 *
		 * Fixed oddity or bug (see sh6(1)):
		 * Avoid breaking pipelines of the following form:
		 *
		 *	% ( ( echo hello ) | ( cat ) | ( grep h ) ) >file &
		 *
		 * ...
		 * This fix further limits when `/dev/null' is opened
		 * as input for `&' commands.
		 */
		if (t[DLEF] == 0 && (f & (FPIN|FINT|FPRS)) == (FINT|FPRS)) {
			close(STDIN_FILENO);
			open("/dev/null", O_RDONLY, 0);
		}
		if ((f & FINT) == 0 && setintr) {
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		}
		signal(SIGTERM, SIG_DFL);
		if (t[DTYP] == TPAR) {
			if ((t1 = (intptr_t *)t[DSPR]) != NULL)
				t1[DFLG] |= f & FINT;
			execute(t1, NULL, NULL);
			_exit(status);
		}
		gflg = 0;
		scan(t, tglob);
		if (gflg) {
			cmd = "glob6";
			t[DSPR] = (intptr_t)cmd;
			pexec(cmd, (char **)t + DSPR);
		} else {
			scan(t, trim);
			cmd = (char *)t[DCOM];
			pexec(cmd, (char **)t + DCOM);
		}
		if (errno == ENOEXEC)
			err("No shell!", 125);
		if (errno != ENOENT && errno != ENOTDIR) {
			prs(cmd);
			err(": cannot execute", 126);
		}
		prs(cmd);
		err(": not found", 127);
		/* NOTREACHED */

	case TFIL:
		if (pipe(pv) == -1) {
			err("pipe failed", 2);
			if (pf1 != NULL) {
				close(pf1[0]);
				close(pf1[1]);
			}
			return;
		}
		f = t[DFLG];
		t1 = (intptr_t *)t[DLEF];
		t1[DFLG] |= FPOU | (f & (FPIN | FINT | FPRS));
		execute(t1, pf1, pv);
		t1 = (intptr_t *)t[DRIT];
		t1[DFLG] |= FPIN | (f & (FAND | FPOU | FINT | FPRS));
		execute(t1, pv, pf2);
		/*
		 * Close the pipe descriptors if needed to avoid a
		 * file descriptor leak when the command to have
		 * written to or read from the pipe was one of
		 * the shell's special commands.
		 */
		close(pv[0]);
		close(pv[1]);
		return;

	case TLST:
		f = t[DFLG] & FINT;
		if ((t1 = (intptr_t *)t[DLEF]) != NULL)
			t1[DFLG] |= f;
		execute(t1, NULL, NULL);
		if ((t1 = (intptr_t *)t[DRIT]) != NULL)
			t1[DFLG] |= f;
		execute(t1, NULL, NULL);
		return;
	}
}

static void
err(const char *msg, int es)
{

	prs(msg);
	prs("\n");
	status = es;
	if (promp == NULL) {
		lseek(STDIN_FILENO, (off_t)0, SEEK_END);
		if (getpid() != shpid)
			_exit(status);
		exit(status);
	}
	if (getpid() != shpid)
		_exit(status);
}

static void
prs(const char *as)
{
	const char *s;

	s = as;
	while (*s != '\0')
		xputc(*s++);
}

static void
xputc(int c)
{
	char cc;

	cc = c;
	write(STDERR_FILENO, &cc, (size_t)1);
}

static void
prn(int n)
{
	int a;

	if ((a = n / 10) != 0)
		prn(a);
	xputc(n % 10 + '0');
}

static int
any(int c, const char *as)
{
	const char *s;

	s = as;
	while (*s != '\0')
		if (*s++ == c)
			return 1;
	return 0;
}

static void
pwait(pid_t i)
{
	pid_t p;
	int e, s;

	for (;;) {
		p = wait(&s);
		if (p == -1)
			break;
		if (s) {
			if (WIFSIGNALED(s)) {
				e = WTERMSIG(s);
				if (e >= XNSIG || mesg[e] != NULL) {
					if (p != i) {
						prn(p);
						prs(": ");
					}
					if (e < XNSIG)
						prs(mesg[e]);
					else {
						prs("Sig ");
						prn(e);
					}
#ifdef	WCOREDUMP
					if (WCOREDUMP(s))
						prs(" -- Core dumped");
#endif
				}
				err("", 0200 + e);
			} else if (WIFEXITED(s))
				status = WEXITSTATUS(s);
		} else
			status = 0;
		if (p == i)
			break;
	}
}

static void
sh_init(void)
{
	pid_t p;
	long li;
	int i;

	/*
	 * Set-ID execution is not supported.
	 *
	 * NOTE: Do not remove the following code w/o understanding
	 *	 the ramifications of doing so.  For further info,
	 *	 see the SECURITY section of the sh6(1) manual page.
	 */
	if (geteuid() != getuid() || getegid() != getgid())
		err("Set-ID execution denied", 2);

	if ((li = sysconf(_SC_OPEN_MAX)) == -1 || li > INT_MAX)
		li = NOFILE;
	for (i = li, i--; i > STDERR_FILENO; i--)
		close(i);

	shpid = p = getpid();
	for (i = 4; i >= 0; i--) {
		shpidstr[i] = p % 10 + '0';
		p /= 10;
	}

	/*
	 * Set the SIGCHLD signal to its default action.
	 * Correct operation of the shell requires that zombies
	 * be created for its children when they terminate.
	 */
	signal(SIGCHLD, SIG_DFL);
}
