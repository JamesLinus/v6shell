/*
 * if - port of the Sixth Edition (V6) UNIX conditional command
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

/* if.c,v 1.5 2003/08/11 jneitzel */

#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define ARGMAX	50	/* original value used by the V6 if command */

int	ap;
int	ac;
char	**av;

int	tcreat(char *);
int	tio(char *, int);
char	*nxtarg(void);
int	e1(void);
int	e2(void);
int	e3(void);
int	eq(char *, char *);
int	exp(void);
int	doex(int);

int
main(int argc, char **argv)
{
	argv[argc] = 0;
	ac = argc; av = argv; ap = 1;
	if (argc < 2) return(1);
	if (exp())
		if (doex(0)) {
			write(1, "no command\n", 11);
			lseek(0, (off_t)0, SEEK_END);
		}
	return(0);
}

char *
nxtarg()
{
	if (ap > ac) return(0/**ap++*/);
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
	if (eq(nxtarg(), "!"))
		return(!e3());
	ap--;
	return(e3());
}

int
e3()
{
	int p1, ccode;
	register char *a, *b;

	ccode = 0;
	if ((a = nxtarg()) == 0) goto err;
	if (eq(a, "(")) {
		p1 = exp();
		if (!eq(nxtarg(), ")")) goto err;
		return(p1);
	}

	if (eq(a, "-r"))
		return(tio(nxtarg(), O_RDONLY));

	if (eq(a, "-w"))
		return(tio(nxtarg(), O_WRONLY));

	/* XXX: This primitive is not documented. */
	if (eq(a, "-c"))
		return(tcreat(nxtarg()));

	if (eq(a, "{")) { /* execute a command for exit code */
		if (fork()) /*parent*/ wait(&ccode);
		else { /*child*/
			doex(1);
			goto err;
		}
		while ((a = nxtarg()) && (!eq(a, "}")))
			continue;
		return(ccode ? 0 : 1);
	}

	b = nxtarg();
	if (b == 0) goto err;
	if (eq(b, "="))
		return(eq(a, nxtarg()));

	if (eq(b, "!="))
		return(!eq(a, nxtarg()));
err:
	write(1, "if error\n", 9);
	exit(9);
}

int
tio(char *a, int f)
{
	int i;
	i = open(a, f);
	if (i >= 0) {
		close(i);
		return(1);
	}
	return(0);
}

/* XXX: 'a' is unused parameter.  What is the purpose of the '-c' primitive? */
int
tcreat(char *a)
{
	return(1);
}

int
eq(char *a, char *b)
{
	register int i;

	i = 0;
l:
	if (a == NULL || b == NULL || a[i] != b[i])
		return(0);
	if (a[i++] == '\0')
		return(1);
	goto l;
}

int
doex(int earg)
{
	register int np;
	char *nargv[ARGMAX], *na;

	np = 0;
	while ((na = nxtarg()) != NULL) {
		if (earg && eq(na, "}")) break;
		if (np < ARGMAX)
			nargv[np++] = na;
		else {
			/* perhaps simply returning the 'if error' is better? */
			write(1, "if: too many args\n", 18);
			exit(9);
		}
	}
	if (earg && (!eq(na, "}"))) return(9);
	nargv[np] = NULL;
	if (np == 0) return(earg);
#ifdef	CLONE
	execv(nargv[0], nargv);
#endif
	execvp(nargv[0], nargv);
	return(1);
}

