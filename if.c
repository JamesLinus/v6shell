/*
 * if - port of the Sixth Edition (V6) UNIX conditional command
 *
 * Modified by Jeffrey Allen Neitzel, July-October 2003.
 *
 * Modified by Gunnar Ritter, Freiburg i. Br., Germany, February 2002.
 *
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
const char revision[] = "@(#)if.c	1.60 (jneitzel) 2003/10/12";
#endif	/* !lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>

#define ARGMAX	50	/* maximum number of arguments for executed commands */
#define	EFD	2	/* file descriptor used for error messages */

int	ap;
int	ac;
char	**av;

char	*nxtarg(void);
int	exp(void);
int	e1(void);
int	e2(void);
int	e3(void);
int	tio(char *, int);
/*int	tcreat(char *);*/
int	eq(const char *, const char *);
int	doex(int);

int
main(int argc, char **argv)
{
	ac = argc; av = argv; ap = 1;
	if (argc < 2) return(1);
	if (exp())
		if (doex(0)) { /* command not found or cannot be executed */
			write(EFD, "no command\n", 11);
			lseek(0, (off_t)0, SEEK_END);
		}
	return(0);
}

char *
nxtarg()
{
	if (ap > ac) return(NULL);
	return(av[ap++]);
}

int
exp()
{
	int p1;

	p1 = e1();
	if (eq(nxtarg(), "-o")) return(p1 | exp());
	ap--;
	return(p1);
}

int
e1()
{
	int p1;

	p1 = e2();
	if (eq(nxtarg(), "-a")) return(p1 & e1());
	ap--;
	return(p1);
}

int
e2()
{
	if (eq(nxtarg(), "!")) return(!e3());
	ap--;
	return(e3());
}

int
e3()
{
	int p1, ccode;
	pid_t cpid;
	register char *a;
	char *b, *c;

	ccode = 0;
	if ((a = nxtarg()) == NULL) goto err;
	if (eq(a, "(")) {
		p1 = exp();
		if (!eq(nxtarg(), ")")) goto err;
		return(p1);
	}

	/* 'if -r' or 'if -w' is not an error and matches original behaviour. */
	if (eq(a, "-r"))
		return(tio(nxtarg(), R_OK));

	if (eq(a, "-w"))
		return(tio(nxtarg(), W_OK));

	/* XXX: undocumented and useless */
	/*if (eq(a, "-c"))
	 *	return(tcreat(nxtarg()));*/

	/* Execute bracketed command to obtain its exit status. */
	if (eq(a, "{")) {
		if ((cpid = fork()) == -1) {
			write(EFD, "if: cannot fork\n", 16);
			exit(9);
		} else if (cpid > 0) /*parent*/
			wait(&ccode);
		else { /*child*/
			doex(1);
			goto err;
		}
		while ((a = nxtarg()) && (!eq(a, "}")));
		return(ccode ? 0 : 1);
	}

	if ((b = nxtarg()) != NULL && (c = nxtarg()) != NULL) {
		if (eq(b, "="))
			return(eq(a, c));
		if (eq(b, "!="))
			return(!eq(a, c));
	}
err:
	write(EFD, "if error\n", 9);
	exit(9);
}

/*
 * Use access(2) instead of open(2) for the '-r' and '-w' tests.
 * The primary reason for this change is because using open() to test
 * if a directory is writable will always fail.  The old code had this
 * problem.  Interestingly, test(1) in V7 also has the same problem.
 */
int
tio(char *f, int m)
{
	if (f == NULL || *f == '\0')
		return(0);

	return(access(f, m) == 0);
}

/*
 * XXX: The purpose of this is unclear since it is undocumented.
 *	It tests for nothing and always returns true.  This code
 *	will be removed unless its purpose becomes clear.
 */
/*int
 *tcreat(char *a)
 *{
 *	return(1);
 *}*/

int
eq(register const char *a, register const char *b)
{
	if (a == NULL || b == NULL)
		return(0);

	while (*a == *b) {
		if (*a == '\0')
			return(1);
		++a;
		++b;
	}
	return(0);
}

int
doex(int earg)
{
	register int np;
	char *nargv[ARGMAX], *na;

	np = 0;
	while ((na = nxtarg()) != NULL) {
		if (earg && eq(na, "}")) break;
		if (np >= ARGMAX - 1) {
			write(EFD, "if: too many args\n", 18);
			exit(9);
		}
		nargv[np++] = na;
	}
	if (earg && (!eq(na, "}"))) return(9);
	nargv[np] = NULL;
	if (np == 0) return(earg);

	/* Documentation says command in '{ ... }' must not be another if. */
	if (earg && eq(nargv[0], "if")) return(9);

	/* Use a built-in exit since there is no external exit utility. */
	if (eq(nargv[0], "exit")) {
		lseek(0, (off_t)0, SEEK_END);
		if (earg)
			exit(0);
		else
			return(0);
	}
#ifdef	CLONE
	execv(nargv[0], nargv);
#endif
	execvp(nargv[0], nargv);
	return(1);
}

