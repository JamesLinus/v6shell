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
static const char vid[]
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
   __attribute__((used))
# elif defined(__GNUC__)
   __attribute__((unused))
# endif
 = "@(#)osh-041028: parse.c	1.2 (jneitzel) 2004/10/28";
#endif	/* !lint */

#include "osh.h"

static void	build_args(char *, int *, int);
static char	*de_blank(char *);
static int	psyntax(char *);
static int	pxpipe(char *, int, int);
static int	strip_sub(char *);
static char	*subshell(char *);

/*
 * Return pointer to the next unquoted character.
 * Return NULL if syntax error.
 */
char *
quote(char *s)
{
	char ch, *p;

	for (p = s; *p != '\0'; ++p) {
		switch (*p) {
		case '\"':
		case '\'':
			/* Everything within "double" or 'single' quotes
			 * is treated literally.
			 */
			for (ch = *p++; *p != ch; ++p)
				if (*p == '\0')
					return(NULL); /* unmatched " or ' */
			break;
		case '\\':
			/* The next character is quoted (escaped). */
			if (*++p == '\0')
				return(p);
			break;
		default:
			return(p);
		}
	}
	return(p);
}

/*
 * Substitute unquoted occurrences of $...
 * Return 0 on failure (e.g., syntax error or too many characters).
 * Return 1 on success.
 */
int
substparm(char *s)
{
	int argn;
	unsigned copy;
	char dbuf[LINESIZE], tbuf[32];
	char *d, *eos, *p, *t;

	eos = dbuf + LINESIZE - 1;
	for (copy = 0, d = dbuf, p = de_blank(s); *p != '\0'; d++, p++) {

		/* Skip quoted characters. */
		if ((t = quote(p)) == NULL) {
			error(0, "syntax error");
			return(0);
		}
		while (p < t) {
			if (d >= eos)
				goto substerr;
			*d++ = *p++;
		}
		if (*p == '\0')
			break;

		if (d >= eos)
			goto substerr;

		if (*p == '$') {
			copy = 1;
			t = NULL;
			if (*++p >= '0' && *p <= '9') {
				argn = *p - '0';
				if (argn < posac)
					t = (argn > 0) ? posav[argn] : si.name;
			} else if (*p == '$') {
				snprintf(tbuf, sizeof(tbuf), "%05u", si.pid);
				t = tbuf;
			} else {
#ifndef	SH6
				if (clone)
					p--;
				else if (*p == 'u')
					t = si.user;
				else if (*p == 't')
					t = si.tty;
				else if (*p == 'p')
					t = si.path;
				else if (*p == 'h')
					t = si.home;
				else if (*p == 'n') {
					snprintf(tbuf, sizeof(tbuf), "%d",
					    (posac > 0) ? posac - 1 : 0);
					t = tbuf;
				} else if (*p == 's') {
					snprintf(tbuf, sizeof(tbuf), "%d",
					    status);
					t = tbuf;
				} else
#endif	/* !SH6 */
					p--;
			}
			if (t != NULL) {
				/* Copy parameter value to buffer now. */
				while (*t != '\0') {
					if (d >= eos)
						goto substerr;
					*d++ = *t++;
				}
				d--;
			} else
				d--;
		} else
			*d = *p;
	}
	*d = '\0';
	if (copy) {
		strncpy(s, dbuf, (size_t)LINESIZE - 1);
		s[LINESIZE - 1] = '\0';
	}
	return(1);

substerr:
	error(0, "Too many characters");
	return(0);
}

/*
 * Parse entire command line for proper syntax if act != PX_EXEC;
 * otherwise, break it down into individual pipelines and execute.
 * Return 1 on success.
 * Return 0 on failure.
 */
int
pxline(char *cl, int act)
{
	int flag;
	unsigned doex, done;
	char cl_sav[LINESIZE];
	char *lpip, *p;

	if (*(cl = de_blank(cl)) == '\0')
		return(1);

	if (act == PX_SYNEXEC) {
		/* First, save a copy of the command line. */
		strncpy(cl_sav, cl, (size_t)LINESIZE - 1);
		cl_sav[LINESIZE - 1] = '\0';
	}

	for (doex = done = spc = 0, p = cl, lpip = cl; ; ++p) {
		if ((p = quote(p)) == NULL)
			return(0);
		if (*p == '(' && (p = subshell(p)) == NULL)
			return(0);
		if (*p == '\0') {
			flag = 0;
			done = 1;
		} else {
			if (*p == ';')
				flag = 0;
			else if (*p == '&')
				flag = FL_ASYNC;
			else
				continue;
			*p = '\0';
		}
		lpip = de_blank(lpip);
		if (*lpip != '\0') {
			if (act != PX_EXEC) {
				if (pxpipe(lpip, flag, PX_SYN) == 0)
					return(0);
				doex = 1;
			} else if (pxpipe(lpip, flag, PX_EXEC) == 0)
				return(0);
		}
		if (done) {
			if (act == PX_SYNEXEC && doex) {
				if (apc > 0 || apstat >= 0)
					apwait(WNOHANG);
				pxline(cl_sav, PX_EXEC);
			}
			return(1);
		}
		lpip = p + 1;
	}
	/* NOTREACHED */
}

/*
 * Parse pipeline for proper syntax if act == PX_SYN; otherwise,
 * break it down into individual commands, connected by pipes if
 * needed, and pass each command to build_args() for execution.
 * Return 1 on success.
 * Return 0 on failure.
 */
static int
pxpipe(char *pl, int flags, int act)
{
	int pfd[2] = { -1, -1 };
	int redir[3] = { -1, -1, -1 };
	unsigned piped;
	char *lcmd, *p;

	for (piped = 0, p = pl, lcmd = pl; ; ++p) {
		if (act == PX_EXEC && spc == -1) {
			/* too many concurrent processes */
			if (pfd[0] != -1)
				close(pfd[0]);
			return(0);
		}
		if (*(p = quote(p)) == '(')
			p = subshell(p);
		switch (*p) {
		case '|':
		case '^':
			*p = '\0';
			piped = 1;
			break;
		case '\0':
			piped = 0;
			break;
		default:
			continue;
		}
		lcmd = de_blank(lcmd);
		if (act == PX_SYN) {
			if (*lcmd == '\0')
				return(0); /* unexpected `|', `^', or `\0' */
			if (psyntax(lcmd) == 0)
				return(0);
			if (strip_sub(lcmd) == 0)
				return(0);
			if (!piped)
				return(1);
		} else {
			if (piped) {
				redir[0] = pfd[0];
				if (pipe(pfd) == -1) {
					error(0, "Cannot pipe");
					apstat = 1;
					if (pfd[0] != -1)
						close(pfd[0]);
					return(0);
				}
				redir[1] = pfd[1];
				redir[2] = pfd[0];
				build_args(lcmd, redir, flags | FL_PIPE);
			} else {
				redir[0] = pfd[0];
				redir[1] = -1;
				redir[2] = -1;
				build_args(lcmd, redir, flags);
				return(1);
			}
		}
		lcmd = p + 1;
	}
	/* NOTREACHED */
}

/*
 * Build argument vector for command.
 * Call execute() if successful; otherwise,
 * fail with a 'Too many args' error.
 */
static void
build_args(char *cmd, int *redir, int flags)
{
	unsigned done, i;
	char *args[ARGMAX];
	char *larg, *p;

	for (done = i = 0, p = cmd, larg = cmd; ; ++p) {
		if (*(p = quote(p)) == '(')
			p = subshell(p);
		switch (*p) {
		case ' ':
		case '\t':
			*p = '\0';
			break;
		case '\0':
			done = 1;
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
			args[i++] = larg;
		}
		if (done) {
			args[i] = NULL;
			execute(args, redir, flags);
			return;
		}
		larg = p + 1;
	}
	/* NOTREACHED */
}

/*
 * Strip away the outermost `... (' and `) ...' from command and
 * call pxline() to parse the command line contained within.
 * This function has no effect on simple commands.
 * Return 1 on success.
 * Return 0 on failure.
 */
static int
strip_sub(char *cmd)
{
	char *a, *b;

	/* Locate the first unquoted `(' if any. */
	for (a = cmd; ; ++a) {
		a = quote(a);
		if (*a == '(' || *a == '\0')
			break;
	}

	if (*a == '(') {
		/* Strip away the `(' and `) ...' now. */
		*a = ' ';
		b = a + strlen(a);
		while (*--b != ')')
			;	/* nothing */
		*b = '\0';

		/* Check syntax. */
		if (pxline(a, PX_SYN) == 0)
			return(0);
	}
	return(1);
}

/*
 * Parse individual command for proper syntax.
 * Return 1 on success.
 * Return 0 on failure.
 */
static int
psyntax(char *s)
{
	unsigned r_in, r_out, sub, word, wtmp;
	char *p;

	for (r_in = r_out = sub = word = wtmp = 0, p = s; ; ++p) {
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			if ((p = quote(p)) == NULL)
				return(0);
			if (!wtmp)
				wtmp = 1;
		}
		switch (*p) {
		case ' ':
		case '\t':
			if (wtmp) {
				wtmp = 0;
				if (r_in && r_in != 2)
					++r_in;
				else if (r_out && r_out != 2)
					++r_out;
				else
					word = 1;
			}
			continue;
		case '<':
			if (r_in || (r_out && r_out != 2))
				return(0); /* unexpected `<' */
			++r_in;
			continue;
		case '>':
			if (r_out || (r_in && r_in != 2))
				return(0); /* unexpected `>' or `>>' */
			if (*(p + 1) == '>')
				++p;
			++r_out;
			continue;
		case ')':
			/* unexpected `)' */
			return(0);
		case '(':
			/* Perhaps it is a subshell? */
			if (wtmp || word)
				return(0); /* unexpected `(' */

			if ((p = subshell(p)) == NULL)
				return(0);
			/* Yes, it is a subshell. */
			sub = 1;
			if (*p != '\0')
				break;
			/* FALLTHROUGH */
		case '\0':
			if (wtmp) {
				wtmp = 0;
				if (r_in && r_in != 2)
					++r_in;
				else if (r_out && r_out != 2)
					++r_out;
				else
					word = 1;
			}

			if ((r_in && r_in != 2) || (r_out && r_out != 2))
				return(0); /* redirect: missing file name */
			if ((!sub && !word) || (sub && word))
				return(0);
			return(1);
		default:
			if (!wtmp)
				wtmp = 1;
			break;
		}
	}
	/* NOTREACHED */
}

/*
 * Return pointer to the character following `( ... )' or NULL
 * if a syntax error has been detected.
 *
 * XXX: This function expects the first character of its argument
 *	to be a `('; otherwise, undefined behaviour will result.
 *	See pxline(), pxpipe(), psyntax(), and build_args() for
 *	the correct context in which to call this function.
 */
static char *
subshell(char *s)
{
	unsigned count, empty;
	char *p, *q;

	for (count = empty = 1, p = s, ++p; ; ++p) {
		p = de_blank(p);
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			if ((p = quote(p)) == NULL)
				return(NULL);
			empty = 0;
		}
		switch (*p) {
		case '(':
			empty = 1;
			++count;
			continue;
		case ')':
			if (empty)
				return(NULL);
			if (--count == 0) {
				++p;
				goto subend;
			}
			continue;
		case '\0':
			/* too many `(' */
			return(NULL);
		default:
			empty = 0;
			break;
		}
	}

subend:
	q = de_blank(p);
	switch (*q) {
	case '\0':
	case ';': case '&':
	case '|': case '^':
		break;
	case '<':
	case '>':
		/* blank-space(s) required before redirection arguments */
		if (q > p)
			break;
		/* FALLTHROUGH */
	default:
		/* too many `)' or trailing garbage */
		return(NULL);
	}
	return(p);
}

/*
 * Skip blanks at beginning of string.
 */
static char *
de_blank(char *s)
{
	for ( ; *s == ' ' || *s == '\t'; ++s)
		;	/* nothing */
	return(s);
}
