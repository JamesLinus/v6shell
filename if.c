/*
 * if: a port of the Sixth Edition (V6) Unix conditional command.
 */
/*
 * Modified by Jeffrey Allen Neitzel, 2003, 2004.
 *
 * Modified by Gunnar Ritter, Freiburg i. Br., Germany, February 2002.
 */
/*
 *	Derived from:
 *		- Sixth Edition Unix	/usr/source/s1/if.c
 *		- Seventh Edition Unix	/usr/src/cmd/test.c
 */
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   Redistributions of source code and documentation must retain the
 *    above copyright notice, this list of conditions and the following
 *    disclaimer.
 *   Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *   All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 *   Neither the name of Caldera International, Inc. nor the names of
 *    other contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)if.c	1.3 (gritter) 2/14/02
 */

#ifndef	lint
const char vid[]
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
   __attribute__((used))
# elif defined(__GNUC__)
   __attribute__((unused))
# endif
 = "@(#)osh-041018: if.c	1.66 (jneitzel) 2004/10/16";
#endif	/* !lint */

#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARGMAX		256	/* maximum number of arguments for command */
#define	FATAL		2	/* exit status for errors */
#define SPRIMARY	1	/* for the `-s' primary */

int	ac;
int	ap;
char	**av;

int	doex(int);
int	e1(void);
int	e2(void);
int	e3(void);
int	equal(const char *, const char *);
void	error(int, const char *, const char *);
int	expr(void);
int	ifaccess(const char *, int);
int	ifstat(const char *, int);
char	*nxtarg(int);
void	quit(int);

int
main(int argc, char **argv)
{
	int rtn;

	/* Set-id execution is not supported. */
	if (geteuid() != getuid() || getegid() != getgid())
		error(FATAL, NULL, "Set-ID execution not supported");

	ac = argc; av = argv; ap = 1;
	if (argc < 2)
		return(1);
	if ((rtn = expr()))
		doex(0);
	return(rtn ? 0 : 1);
}

int
expr(void)
{
	int p1;

	p1 = e1();
	if (equal(nxtarg(1), "-o")) return(p1 | expr());
	ap--;
	return(p1);
}

int
e1(void)
{
	int p1;

	p1 = e2();
	if (equal(nxtarg(1), "-a")) return(p1 & e1());
	ap--;
	return(p1);
}

int
e2(void)
{
	if (equal(nxtarg(1), "!")) return(!e3());
	ap--;
	return(e3());
}

int
e3(void)
{
	pid_t cpid;
	int ccode, p1;
	char *a, *b;

	ccode = 0;
	a = nxtarg(0);

	/*
	 * Deal w/ parentheses for grouping.
	 */
	if (equal(a, "(")) {
		p1 = expr();
		if (!equal(nxtarg(1), ")"))
			error(FATAL, a, ") expected");
		return(p1);
	}

	/*
	 * Execute command within braces to obtain its exit status.
	 */
	if (equal(a, "{")) {
		if ((cpid = fork()) == -1)
			error(FATAL, NULL, "Cannot fork");
		if (cpid == 0) { /*child*/
			if (doex(1) == FATAL)
				error(FATAL, a, "} expected");
			error(FATAL, a, "command expected");
		} else /*parent*/
			wait(&ccode);
		while ((a = nxtarg(1)) && !equal(a, "}"))
			;	/* nothing */
		return(ccode ? 0 : 1);
	}

	/*
	 * file existence/permission tests
	 */
	if (equal(a, "-e"))
		return(ifaccess(nxtarg(0), F_OK));
	if (equal(a, "-r"))
		return(ifaccess(nxtarg(0), R_OK));
	if (equal(a, "-w"))
		return(ifaccess(nxtarg(0), W_OK));
	if (equal(a, "-x"))
		return(ifaccess(nxtarg(0), X_OK));

	/*
	 * other file tests
	 */
	if (equal(a, "-d"))
		return(ifstat(nxtarg(0), S_IFDIR));
	if (equal(a, "-f"))
		return(ifstat(nxtarg(0), S_IFREG));
	/* always false if S_IFLNK is not defined */
	if (equal(a, "-h"))
#ifdef	S_IFLNK
		return(ifstat(nxtarg(0), S_IFLNK));
#else
		return(nxtarg(0), 0);
#endif
	if (equal(a, "-s"))
		return(ifstat(nxtarg(0), SPRIMARY));
	if (equal(a, "-t")) {
		/* Does the descriptor refer to a terminal device? */
		b = nxtarg(1);
		if (b == NULL || *b == '\0')
			error(FATAL, a, "digit expected");
		if (*b >= '0' && *b <= '9' && *(b + 1) == '\0')
			return(isatty(*b - '0'));
		error(FATAL, b, "not a digit");
	}

	/*
	 * Compare the string arguments.
	 */
	if ((b = nxtarg(1)) == NULL)
		error(FATAL, a, "operator expected");
	if (equal(b, "="))
		return(equal(a, nxtarg(0)));
	if (equal(b, "!="))
		return(!equal(a, nxtarg(0)));
	error(FATAL, b, "unknown operator");
	/* NOTREACHED */
	return(0);
}

int
doex(int earg)
{
	int np;
	char *nargv[ARGMAX], *na;

	np = 0;
	while ((na = nxtarg(1)) != NULL) {
		if (earg && equal(na, "}")) break;
		if (np + 1 >= ARGMAX)
			error(FATAL, NULL, "Too many args");
		nargv[np++] = na;
	}
	if (earg && (!equal(na, "}"))) return(FATAL);
	if (np == 0) return(earg);
	nargv[np] = NULL;

	/* Use a built-in exit since there is no external exit utility. */
	if (equal(nargv[0], "exit"))
		quit(0);

	execvp(nargv[0], nargv);
	if (errno == ENOENT || errno == ENOTDIR)
		error(127, nargv[0], "not found");
	error(126, nargv[0], "cannot execute");
	/* NOTREACHED */
	return(0);
}

/*
 * Check access permission for file according to mode while
 * dealing w/ special cases for the superuser and `-x':
 *	- Always grant search access on directories.
 *	- If not a directory, require at least one execute bit.
 * Return 1 if access is granted.
 * Return 0 if access is denied.
 */
int
ifaccess(const char *file, int mode)
{
	struct stat sb;
	int ra;

	if (*file == '\0')
		return(0);

	ra = (access(file, mode) == 0) ? 1 : 0;

	if (ra && mode == X_OK && geteuid() == 0) {
		if (stat(file, &sb) < 0)
			return(0);
		if (S_ISDIR(sb.st_mode))
			return(1);
		return((sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ? 1 : 0);
	}
	return(ra);
}

int
ifstat(const char *file, int type)
{
	struct stat sb;

	if (*file == '\0')
		return(0);

#ifdef	S_IFLNK
	if (type == S_IFLNK) {
		if (lstat(file, &sb) < 0)
			return(0);
		return((sb.st_mode & S_IFMT) == type);
	}
#endif
	if (stat(file, &sb) < 0)
		return(0);
	if (type == S_IFDIR || type == S_IFREG)
		return((sb.st_mode & S_IFMT) == type);
	if (type == SPRIMARY)
		return(sb.st_size > 0);
	/* NOTREACHED */
	return(0);
}

char *
nxtarg(int m)
{
	if (ap >= ac) {
		if (m) {
			ap++;
			return(NULL);
		}
		error(FATAL, av[ap - 1], "argument expected");
	}
	return(av[ap++]);
}

int
equal(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return(0);
	return(strcmp(a, b) == 0);
}

void
error(int es, const char *msg1, const char *msg2)
{
	if (msg1 != NULL)
		fprintf(stderr, "if: %s: %s\n", msg1, msg2);
	else
		fprintf(stderr, "if: %s\n", msg2);
	quit(es);
}

void
quit(int status)
{
	if (lseek(STDIN_FILENO, (off_t)0, SEEK_END) == -1 && errno == ESPIPE)
		while (getchar() != EOF);	/* nothing */
	exit(status);
}
