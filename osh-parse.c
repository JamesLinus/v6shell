/*
 * osh-parse.c - command-line parsing routines for osh and sh6
 */
/*
 * Copyright (c) 2003, 2004, 2005
 *	Jeffrey Allen Neitzel <jneitzel@sdf1.org>.
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
 *	@(#)osh-parse.c	1.5 (jneitzel) 2005/02/15
 */
/*
 *	Derived from: osh-020214/osh.c
 */
/*
 * Copyright (c) 1999, 2000
 *	Gunnar Ritter.  All rights reserved.
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
 *	This product includes software developed by Gunnar Ritter
 *	and his contributors.
 * 4. Neither the name of Gunnar Ritter nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GUNNAR RITTER AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL GUNNAR RITTER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)osh.c	1.6 (gritter) 2/14/02
 */

#ifndef	lint
static const char version[]
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
   __attribute__((used))
# elif defined(__GNUC__)
   __attribute__((unused))
# endif
 = "\100(#)osh-parse.c	1.5 (jneitzel) 2005/02/15";
#endif	/* !lint */

#include "osh.h"

static void	build_av(char *, int *, int);
static char	*nonblank(char *);
static int	psub(char *);
static int	pxpipe(char *, int, int);
static char	*subshell(char *);
static int	substparm(char *, char *);
static int	syntax(char *);

/*
 * Return a pointer to the next unquoted character.
 * Return NULL on error.
 */
char *
quote(char *s)
{
	char ch, *p;

#ifdef	DEBUG
	if (s == NULL)
		error(SH_DEBUG, "quote: Internal error, s == NULL");
#endif

	for (p = s; *p != '\0'; p++) {
		switch (*p) {
		case '\"':
		case '\'':
			/*
			 * Everything within "double" or 'single' quotes
			 * is treated literally.
			 */
			for (ch = *p++; *p != ch; p++)
				if (*p == '\0')
					return(NULL);	/* unmatched " or ' */
			continue;
		case '\\':
			/*
			 * The next non-NUL character is quoted (escaped).
			 */
			if (*++p == '\0')
				break;
			continue;
		}
		break;
	}
	return(p);
}

/*
 * Try to perform all of the steps needed to execute the buffered
 * command line pointed to by linep.  Return on error or after
 * attempting to execute it.
 */
void
execline(char *linep)
{
	char linexbuf[LINESIZE];	/* same size as the linep buffer */
	char *linex;

	/*
	 * First, do parameter substitution.
	 * For compatibility with the Sixth Edition Unix shell,
	 * it is imperative that this be done before trying to
	 * parse the command line for proper syntax.  Correct
	 * operation of the shell relies on this simple fact.
	 */
	switch (substparm(linexbuf, linep)) {
	case 1:
		linex = linexbuf;
		break;
	case 2:
		/*
		 * Locate the first non-blank character of linexbuf.
		 * Then, recopy the non-empty string back to linep.
		 */
		if (*(linex = nonblank(linexbuf)) == '\0')
			return;
		strcpy(linep, linex);
		break;
	default:
		return;
	}

	/*
	 * Next, parse the command line.
	 */
	if (pxline(linep, PX_PARSE)) {
		/*
		 * Finally, attempt to execute it after fetching any
		 * termination reports for asynchronous processes.
		 */
		if (apc > 0 || apstat == 0)
			apwait(WNOHANG);
		pxline(linex, PX_EXEC);
	} else
		error(0, "syntax error");
}

/*
 * If act == PX_PARSE, parse for syntactically-correct command line;
 * otherwise, break it down into individual pipelines, and pass each
 * one to pxpipe() to prepare it for execution.
 * Return 1 on success.
 * Return 0 on error.
 */
int
pxline(char *cl, int act)
{
	int done, flag;
	char *lpip, *p;

	for (done = spc = 0, p = lpip = cl; ; p++) {
		if ((p = quote(p)) == NULL)
			return(0);
		if (*p == '(' && (p = subshell(p)) == NULL)
			return(0);
		switch (*p) {
		case '\0':
			done = 1;
			flag = 0;
			break;
		case ';':
			*p = '\0';
			flag = 0;
			break;
		case '&':
			*p = '\0';
			flag = FL_ASYNC;
			break;
		default:
			continue;
		}
		lpip = nonblank(lpip);
		if (*lpip != '\0') {
			if (act == PX_PARSE) {
				if (pxpipe(lpip, flag, PX_PARSE) == 0)
					return(0);
			} else
				if (pxpipe(lpip, flag, PX_EXEC) == 0)
					return(0);
		}
		if (done)
			return(1);
		lpip = p + 1;
	}
	/* NOTREACHED */
}

/*
 * If act == PX_PARSE, parse for syntactically-correct pipeline; otherwise,
 * break it down into individual commands (connected by pipes if needed),
 * and pass each one to build_av() to prepare it for execution.
 * Return 1 on success.
 * Return 0 on error.
 */
static int
pxpipe(char *pl, int flags, int act)
{
	unsigned done;
	int pfd[2] = { -1, -1 };
	int redir[3] = { -1, -1, -1 };
	char *lcmd, *p;

	for (done = 0, p = lcmd = pl; ; p++) {
		if (*(p = quote(p)) == '(')
			p = subshell(p);
		switch (*p) {
		case '\0':
			done = 1;
			break;
		case '|':
		case '^':
			*p = '\0';
			break;
		default:
			continue;
		}
		lcmd = nonblank(lcmd);
		if (act == PX_PARSE) {
			if (*lcmd=='\0' || syntax(lcmd)==0 || psub(lcmd)==0)
				return(0);
		} else {
			if (spc == -1) {
				/* too many concurrent processes */
				if (pfd[0] != -1)
					close(pfd[0]);
				return(0);
			}
			if (!done) {
				redir[0] = pfd[0];
				if (pipe(pfd) == -1) {
					error(0, "Cannot pipe");
					apstat = 0;
					if (pfd[0] != -1)
						close(pfd[0]);
					return(0);
				}
				redir[1] = pfd[1];
				redir[2] = pfd[0];
				build_av(lcmd, redir, flags | FL_PIPE);
			} else {
				redir[0] = pfd[0];
				redir[1] = -1;
				redir[2] = -1;
				build_av(lcmd, redir, flags);
			}
		}
		if (done)
			return(1);
		lcmd = p + 1;
	}
	/* NOTREACHED */
}

/*
 * If cmd is a list contained within parentheses, parse this
 * subshelled list to ensure that it is syntactically correct.
 * Otherwise, do nothing if cmd is a simple command.
 * Return 1 on success.
 * Return 0 on error.
 */
static int
psub(char *cmd)
{
	char *a, *b;

	for (a = cmd; ; a++) {
		a = quote(a);
		if (*a == '(' || *a == '\0')
			break;
	}

	if (*a == '(') {
		*a = ' ';
		b = a + strlen(a);
		while (*--b != ')')
			;	/* nothing */
		*b = '\0';
		if (pxline(a, PX_PARSE) == 0)
			return(0);
	}
	return(1);
}

/*
 * Parse an individual command for proper syntax.
 * Return 1 on success.
 * Return 0 on error.
 */
static int
syntax(char *cmd)
{
	unsigned issub, isword, nb, redir0, redir1, wflg;
	char *p;

	for (issub = isword = nb = redir0 = redir1 = wflg = 0, p = cmd; ; p++) {
		switch (*p) {
		case '\"': case '\'': case '\\':
			p = quote(p);
			nb = wflg = 1;
			break;
		}
		switch (*p) {
		case '\0':
			if (wflg) {
				if (redir0 & 1)
					redir0 = 2;
				else if (redir1 & 1)
					redir1 = 2;
				else
					isword = 1;
			}

			if ((issub ^ isword) && !(redir0 & 1) && !(redir1 & 1))
				return(1);
			/*
			 * Bad subshell, Missing command, or Missing file name
			 */
			break;
		case ' ':
		case '\t':
			if (wflg) {
				wflg = 0;
				if (redir0 & 1)
					redir0 = 2;
				else if (redir1 & 1)
					redir1 = 2;
				else
					isword = 1;
			}
			nb = 0;
			continue;
		case '(':
			/*
			 * Is it a valid subshell?
			 */
			if (!isword && !wflg && (p = subshell(p)) != NULL) {
				p--;
				issub = nb = 1;
				continue;
			}
			/* Unexpected `(' or Bad subshell */
			break;
		case ')':
			/*
			 * Too many `)' - see subshell() for further info.
			 */
			break;
		case '<':
			/*
			 * Is it a valid input redirection?
			 */
			if (!redir0 && !(redir1 & 1) && (!nb || p == cmd)) {
				redir0 = 1;
				continue;
			}
			/* Unexpected `<' */
			break;
		case '>':
			/*
			 * Is it a valid output redirection?
			 */
			if (!redir1 && !(redir0 & 1) && (!nb || p == cmd)) {
				if (*(p + 1) == '>')
					p++;
				redir1 = 1;
				continue;
			}
			/* Unexpected `>' or `>>' */
			break;
		default:
			nb = wflg = 1;
			continue;
		}

		/*
		 * syntax error
		 */
		return(0);
	}
	/* NOTREACHED */
}

/*
 * Return a pointer to the character following `( list )'.
 * Return NULL on any of the following errors:
 *
 *	1) Too many `('
 *
 *	2) Too many `)'
 *	   o Note that subshell() cannot detect all cases of this error.
 *	     For this reason, the final detection of this type of error
 *	     is handled by syntax().  This approach allows subshell()
 *	     to be simpler than it might otherwise have to be.
 *
 *	3) Trailing garbage after `)'
 *
 *	4) Empty `(   )'
 */
static char *
subshell(char *s)
{
	unsigned count, empty;
	char *p;

#ifdef	DEBUG
	if (s == NULL || *s != '(')
		error(SH_DEBUG,
		    "subshell: Internal error, s == NULL || *s != '('");
#endif

	for (count = empty = 1, p = s, p++; ; p++) {
		p = nonblank(p);
		switch (*p) {
		case '\"': case '\'': case '\\':
			p = quote(p);
			empty = 0;
			break;
		}
		switch (*p) {
		case '\0':
			break;
		case '(':
			empty = 1;
			count++;
			continue;
		case ')':
			if (empty)
				break;
			if (--count == 0) {
				p++;
				switch (*nonblank(p)) {
				case '\0':
				case ';':
				case '&':
				case '|':
				case '^':
				case '<':
				case '>':
					return(p);
				}
				break;
			}
			continue;
		default:
			empty = 0;
			continue;
		}

		/*
		 * syntax error
		 */
		return(NULL);
	}
	/* NOTREACHED */
}

/*
 * Return a pointer to the first non-blank character of the string s.
 * Otherwise, return the empty string.
 */
static char *
nonblank(char *s)
{

	for ( ; *s == ' ' || *s == '\t'; s++)
		;	/* nothing */
	return(s);
}

/*
 * Substitute all unquoted $ parameters (if any) in the command line
 * src w/ the appropriate values while copying it to dst.  This is a
 * size-bounded copy, which copies up to LINESIZE - 1 characters and
 * NUL-terminates the result.  Otherwise, an error is produced.
 *
 * Return values:
 *	2	Substitution has occurred; dst is not equal to src.
 *	1	No substitution has occurred; dst is equal to src.
 *	0	An error has occurred.
 */
static int
substparm(char *dst, char *src)
{
	int gotp, ppn;
	char vbuf[32];
	char *dsteos, *dstmax;
	char *d, *s, *v;

	dstmax = dst + LINESIZE;
	dsteos = dstmax - 1;
	for (gotp = 0, d = dst, s = src; d < dstmax; d++, s++) {

		/* Copy quoted characters to dst. */
		if ((v = quote(s)) == NULL) {
			error(0, "syntax error");
			return(0);
		}
		while (s < v && d < dsteos)
			*d++ = *s++;

		switch (*s) {
		case '\0':
			*d = '\0';
			return(1 + gotp);
		case '$':
			gotp = 1;
			v = NULL;
			if (*++s >= '0' && *s <= '9') {
				ppn = *s - '0';
				if (ppn < ppac)
					v = (ppn > 0) ? ppav[ppn] : si.name;
			} else if (*s == '$') {
				snprintf(vbuf, sizeof(vbuf), "%05u", si.pid);
				v = vbuf;
			} else {
#ifndef	SH6
				if (clone)
					s--;
				else if (*s == 'u')
					v = si.user;
				else if (*s == 't')
					v = si.tty;
				else if (*s == 'p')
					v = si.path;
				else if (*s == 'h')
					v = si.home;
				else if (*s == 'n') {
					snprintf(vbuf, sizeof(vbuf), "%d",
					    (ppac > 1) ? ppac - 1 : 0);
					v = vbuf;
				} else if (*s == 's') {
					snprintf(vbuf, sizeof(vbuf), "%d",
					    status);
					v = vbuf;
				} else
#endif	/* !SH6 */
					s--;
			}
			if (v != NULL) {
				/* Copy parameter value to dst. */
				while (*v != '\0' && d < dstmax)
					*d++ = *v++;
				d--;
			} else
				d--;
			break;
		default:
			*d = *s;
		}
	}

	error(0, "Too many characters");
	return(0);
}

/*
 * Build an argument vector for the command, and execute() it.
 * Otherwise, fail with a `Too many args' error.
 */
static void
build_av(char *cmd, int *redir, int flags)
{
	unsigned done, i;
	char *av[ARGMAX];
	char *larg, *p;

#ifdef	DEBUG
	if (cmd == NULL || *cmd == '\0')
		error(SH_DEBUG,
		    "build_av: Internal error, cmd == NULL || *cmd == '\\0'");
#endif

	for (done = i = 0, p = larg = cmd; ; p++) {
		if (*(p = quote(p)) == '(')
			p = subshell(p);
		switch (*p) {
		case '\0':
			done = 1;
			break;
		case ' ':
		case '\t':
			*p = '\0';
			break;
		default:
			continue;
		}
		if (*(p - 1) != '\0') {
			/* error if too many args */
			if (i + 1 >= ARGMAX) {
				error(0, "Too many args");
				return;
			}
			av[i++] = larg;
		}
		if (done) {
			av[i] = NULL;
#ifdef	DEBUG
			if (av[0] == NULL)
				error(SH_DEBUG,
				    "build_av: Internal error, av[0] == NULL");
#endif
			execute(av, redir, flags);
			return;
		}
		larg = p + 1;
	}
	/* NOTREACHED */
}
