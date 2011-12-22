/*
 * if - enhanced port of the Sixth Edition (V6) Unix conditional command
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
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
#  define	ID_ATTR	__attribute__((used))
# elif defined(__GNUC__)
#  define	ID_ATTR	__attribute__((unused))
# else
#  define	ID_ATTR
# endif
const char revision[] ID_ATTR = "@(#)if.c	1.65 (jneitzel) 2004/06/28";
#endif	/* !lint */

#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ARGMAX	50	/* maximum number of arguments for executed commands */

int	ac;
int	ap;
char	**av;

int	doex(int);
int	e1(void);
int	e2(void);
int	e3(void);
int	equal(const char *, const char *);
void	error(const char *, const char *);
int	expr(void);
char	*nxtarg(int);
void	quit(int);

int
main(int argc, char **argv)
{
	int res;

	/* Revoke set-id privileges. */
	setgid(getgid());
	setuid(getuid());

	ac = argc;
	av = argv;
	ap = 1;

	if (argc < 2)
		return(1);
	if ((res = expr()))
		doex(0);
	return(res ? 0 : 1);
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
	struct stat sb;
	pid_t cpid;
	int p1, ccode;
	char *a, *b;

	ccode = 0;
	a = nxtarg(0);
	if (equal(a, "(")) {
		p1 = expr();
		if (!equal(nxtarg(1), ")")) error("(", ") expected");
		return(p1);
	}

	/* file tests */
	if (equal(a, "-r"))
		return(access(nxtarg(0), R_OK) == 0);
	if (equal(a, "-w"))
		return(access(nxtarg(0), W_OK) == 0);
	if (equal(a, "-e"))
		return(access(nxtarg(0), F_OK) == 0);
	if (equal(a, "-d"))
		return(stat(nxtarg(0), &sb) == 0 && S_ISDIR(sb.st_mode));
	if (equal(a, "-f"))
		return(stat(nxtarg(0), &sb) == 0 && S_ISREG(sb.st_mode));

	/* Execute bracketed command to obtain its exit status. */
	if (equal(a, "{")) {
		if ((cpid = fork()) == -1) {
			fputs("if: cannot fork\n", stderr);
			quit(2);
		} else if (cpid == 0) { /*child*/
			doex(1);
			error(NULL, "error");
		} else /*parent*/
			wait(&ccode);
		while ((a = nxtarg(1)) && !equal(a, "}"))
			;	/* nothing */
		return(!ccode ? 1 : 0);
	}

	/* Compare the string operands. */
	if ((b = nxtarg(1)) == NULL) error(a, "operator expected");
	if (equal(b, "="))
		return(equal(a, nxtarg(0)));
	if (equal(b, "!="))
		return(!equal(a, nxtarg(0)));

	error(b, "unknown operator");
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
		if (np + 1 >= ARGMAX) {
			fputs("if: too many args\n", stderr);
			quit(2);
		}
		nargv[np++] = na;
	}
	if (earg && (!equal(na, "}"))) return(2);
	if (np == 0) return(earg);
	nargv[np] = NULL;

	/* Use a built-in exit since there is no external exit utility. */
	if (equal(nargv[0], "exit"))
		quit(0);

	execvp(nargv[0], nargv);
	if (errno == ENOENT || errno == ENOTDIR) {
		if (earg) return(2);
		fprintf(stderr, "if: %s: not found\n", nargv[0]);
		quit(127);
	}
	if (earg) return(2);
	fprintf(stderr, "if: %s: cannot execute\n", nargv[0]);
	quit(126);
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
		error(av[ap - 1], "argument expected");
	}
	return(av[ap++]);
}

int
equal(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return(0);
	while (*a == *b++)
		if (*a++ == '\0')
			return(1);
	return(0);
}

void
error(const char *op, const char *msg)
{
	if (op != NULL)
		fprintf(stderr, "if: %s: %s\n", op, msg);
	else
		fprintf(stderr, "if: %s\n", msg);
	exit(2);
}

void
quit(int status)
{
	lseek(STDIN_FILENO, (off_t)0, SEEK_END);
	exit(status);
}
