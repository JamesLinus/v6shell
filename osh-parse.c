/*
 * osh-parse.c - command-line parsing routines for osh and sh6
 */
/*-
 * Copyright (c) 2003, 2004, 2005
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
 *	@(#)osh-parse.c	1.7 (jneitzel) 2005/11/15
 */
/*
 *	Derived from: osh-020214/osh.c
 */
/*-
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
#include "version.h"
OSH_SOURCEID("osh-parse.c	1.7 (jneitzel) 2005/11/15");
#endif	/* !lint */

#include "osh.h"

/*
 * Special built-in commands
 */
static const char *const sbi[] = {
	":",		/* SBINULL     */
	"chdir",	/* SBICHDIR    */
	"exit",		/* SBIEXIT     */
	"login",	/* SBILOGIN    */
	"shift",	/* SBISHIFT    */
	"wait",		/* SBIWAIT     */
#ifndef	SH6
	"sigign",	/* SBISIGIGN   */
	"setenv",	/* SBISETENV   */
	"unsetenv",	/* SBIUNSETENV */
	"umask",	/* SBIUMASK    */
	"source",	/* SBISOURCE   */
	"exec",		/* SBIEXEC     */
#endif	/* !SH6 */
	NULL
};

static int	build_av(char *, int *, int);
static char	*nonblank(const char *);
static int	prep_cmd(int *, char **);
static int	psub(char *);
static int	pxpipe(char *, int, int);
static char	*subshell(const char *);
static ssize_t	substparm(char *, const char *);
static int	syntax(const char *);
static int	which(const char *);
#ifndef	SH6
static char	*get_tty(void);
static char	*get_user(void);
#endif	/* !SH6 */

/*
 * Return a pointer to the next unquoted character.
 * Return NULL on error.
 */
char *
quote(const char *s)
{
	char c;
	const char *p;

#ifdef	DEBUG
	if (s == NULL)
		error(SH_DEBUG, "quote: Invalid argument");
#endif

	for (p = s; *p != '\0'; p++) {
		switch (*p) {
		case '\"':
		case '\'':
			/*
			 * Everything within "double" or 'single' quotes
			 * is treated literally.
			 */
			for (c = *p++; *p != c; p++)
				if (*p == '\0')
					return NULL;	/* Unmatched " or ' */
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
	return (char *)p;
}

/*
 * Perform the steps needed to execute the buffered command line
 * pointed to by pb, a `\0'-terminated string w/ a maximum length
 * of LINEMAX - 1 characters.  Return on a substituted length of
 * 0 characters, on any error, or after attempting to execute it.
 */
void
execline(char *pb)
{
	ssize_t len;
	char xbuf[LINEMAX];
	char *xb;

#ifdef	DEBUG
	if (pb == NULL)
		error(SH_DEBUG, "execline: Invalid argument");
#endif

	/*
	 * First, do parameter substitution.
	 * For compatibility with the Sixth Edition Unix shell,
	 * it is imperative that this be done before trying to
	 * parse the command line for proper syntax.
	 */
	if ((len = substparm(xbuf, pb)) > 0) {
		/*
		 * Ignore any leading blanks in xbuf, and copy it to pb.
		 */
		xb = nonblank(xbuf);
		memcpy(pb, xb, (size_t)len - (xb - xbuf) + 1);
	} else
		return;

	/*
	 * Next, parse the substituted command line pointed to by pb.
	 */
	if (pxline(pb, PX_PARSE)) {
		/*
		 * Finally, attempt to execute the substituted
		 * command line pointed to by xb.  Then, fetch
		 * any asynchronous termination reports.
		 */
		pxline(xb, PX_EXEC);
		apwait(WNOHANG);
	} else
		error(-1, FMT1S, dgn[ERR_SYNTAX]);
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

#ifdef	DEBUG
	if (cl == NULL || (act != PX_PARSE && act != PX_EXEC))
		error(SH_DEBUG, "pxline: Invalid argument");
#endif

	for (done = 0, p = lpip = cl; ; p++) {

		if ((p = quote(p)) == NULL)
			break;
		if (*p == '(' && (p = subshell(p)) == NULL)
			break;

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
			flag = CMD_ASYNC;
			break;
		default:
			continue;
		}

		if (*(lpip = nonblank(lpip)) != '\0')
			if (!pxpipe(lpip, flag, act))
				break;

		if (done)
			return 1;
		lpip = p + 1;

	}
	return 0;
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
	int pfd[2] = { -1, -1 };
	int redir[3] = { -1, -1, -1 };
	char *lcmd, *p;

	for (p = lcmd = pl; ; p++) {

		if (*(p = quote(p)) == '(')
			p = subshell(p);

		switch (*p) {
		case '\0':
			flags &= ~CMD_PIPE;
			break;
		case '|':
		case '^':
			*p = '\0';
			flags |= CMD_PIPE;
			break;
		default:
			continue;
		}

		lcmd = nonblank(lcmd);
		if (act == PX_PARSE) {
			if (*lcmd == '\0' || !syntax(lcmd) || !psub(lcmd))
				return 0;
		} else {
			if (flags & CMD_PIPE) {
				redir[0] = pfd[0];
				if (pipe(pfd) == -1) {
					error(-1, FMT1S, dgn[ERR_PIPE]);
					break;
				}
				redir[1] = pfd[1];
				redir[2] = pfd[0];	/* see try_exec() */
			} else {
				redir[0] = pfd[0];
				redir[1] = -1;
				redir[2] = -1;
			}
			if (build_av(lcmd, redir, flags) == -1)
				break;
		}

		if (!(flags & CMD_PIPE))
			return 1;
		lcmd = p + 1;

	}
	close_pr(redir);
	if (redir[2] != -1)
		close(redir[2]);
	spwait(SPW_DOZAP);
	return 0;
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

#ifdef	DEBUG
	if (cmd == NULL || *nonblank(cmd) == '\0')
		error(SH_DEBUG, "psub: Invalid argument");
#endif

	for (a = cmd; *(a = quote(a)) != '\0'; a++)
		if (*a == '(') {
			*a = ' ';
			b = a + strlen(a);
			while (*--b != ')')
				;	/* nothing */
			*b = '\0';
			if (pxline(a, PX_PARSE))
				break;
			return 0;
		}
	return 1;
}

/*
 * Parse the string cmd to ensure that it is syntactically correct.
 * Return 1 on success.
 * Return 0 on error.
 */
static int
syntax(const char *cmd)
{
	unsigned nb, redir0, redir1, sub, wflg, word;
	const char *p;

#ifdef	DEBUG
	if (cmd == NULL || *nonblank(cmd) == '\0')
		error(SH_DEBUG, "syntax: Invalid argument");
#endif

	for (nb = redir0 = redir1 = sub = wflg = word = 0, p = cmd; ; p++) {
		switch (*p) {
		case '\"': case '\'': case '\\':
			p = quote(p);
			nb = wflg = 1;
			break;
		}
		switch (*p) {
		case '\0':
			/*
			 * Is it syntactically correct?
			 */
			if (wflg) {
				if (redir0 & 1)
					redir0 = 2;
				else if (redir1 & 1)
					redir1 = 2;
				else
					word = 1;
			}
			if ((sub ^ word) && !(redir0 & 1) && !(redir1 & 1))
				return 1;
			/* Syntax error */
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
					word = 1;
			}
			nb = 0;
			continue;
		case '(':
			/*
			 * Is it a valid subshell?
			 */
			if (!wflg && !word && (p = subshell(p)) != NULL) {
				p--;
				nb = sub = 1;
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
			if (!redir0 && !(redir1 & 1) && !nb) {
				redir0 = 1;
				continue;
			}
			/* Unexpected `<' */
			break;
		case '>':
			/*
			 * Is it a valid output redirection?
			 */
			if (!redir1 && !(redir0 & 1) && !nb) {
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
		break;
	}
	return 0;
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
subshell(const char *s)
{
	unsigned count, empty;
	const char *p;

#ifdef	DEBUG
	if (s == NULL || *s != '(')
		error(SH_DEBUG, "subshell: Invalid argument");
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
			count--;
			if (count == 0) {
				p++;
				switch (*nonblank(p)) {
				case '\0':
				case ';':
				case '&':
				case '|':
				case '^':
				case '<':
				case '>':
					return (char *)p;
				}
				break;
			}
			continue;
		default:
			empty = 0;
			continue;
		}
		break;
	}
	return NULL;
}

/*
 * Return a pointer to the first non-blank character of the string s.
 * Otherwise, return the empty string.
 */
static char *
nonblank(const char *s)
{
	const char *p;

	for (p = s; *p == ' ' || *p == '\t'; p++)
		;	/* nothing */
	return (char *)p;
}

/*
 * Substitute all unquoted $ parameters (if any) in the command line
 * src w/ the appropriate values while copying them to dst.  This is
 * a size-bounded copy, which copies up to LINEMAX - 1 characters and
 * `\0'-terminates the result.  Otherwise, an error is produced.
 *
 * Return the significant length of dst on success.
 * Return 0 if dst is the empty string.
 * Return -1 on error.
 */
static ssize_t
substparm(char *dst, const char *src)
{
	int n;
	char vbuf[32];
	char *bs, *d;
	const char *s, *v;

	for (d = dst, s = src; d < &dst[LINEMAX]; d++, s++) {

		/* Copy any quoted characters to dst. */
		if ((v = quote(s)) == NULL)
			return error(-1, FMT1S, dgn[ERR_SYNTAX]);
		while (s < v && d < &dst[LINEMAX - 1])
			*d++ = *s++;

		switch (*s) {
		case '\0':
			/*
			 * Ignore any insignificant trailing blanks in dst
			 * while properly accounting for any `\' characters.
			 * Then, return the significant length of dst or 0.
			 */
			while (--d >= dst && (*d == ' ' || *d == '\t')) {
				for (bs = d - 1; bs >= dst && *bs == '\\'; bs--)
					;	/* nothing */
				if ((d - bs - 1) & 1)
					break;
			}
			*++d = '\0';
			return d - dst;
		case '$':
			v = NULL;
			switch (*++s) {
			case '$':
				snprintf(vbuf, sizeof(vbuf), "%05u", si.pid);
				v = vbuf;
				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				n = *s - '0';
				if (n < ppac)
					v = (n > 0) ? ppav[n] : si.name;
				break;
#ifndef	SH6
			case 'd':
				v = getenv("OSHDIR");
				break;
			case 'e':
				v = getenv("EXECSHELL");
				break;
			case 'h':
				v = getenv("HOME");
				break;
			case 'n':
				n = (ppac > 1) ? ppac - 1 : 0;
				snprintf(vbuf, sizeof(vbuf), "%u", n);
				v = vbuf;
				break;
			case 'p':
				v = getenv("PATH");
				break;
			case 's':
				snprintf(vbuf, sizeof(vbuf), "%u", status);
				v = vbuf;
				break;
			case 't':
				if (si.tty == NULL)
					si.tty = get_tty();
				v = si.tty;
				break;
			case 'u':
				if (si.user == NULL)
					si.user = get_user();
				v = si.user;
				break;
#endif	/* !SH6 */
			default:
				s--;
			}
			/* Copy any parameter value to dst if needed. */
			if (v != NULL)
				while (*v != '\0' && d < &dst[LINEMAX])
					*d++ = *v++;
			d--;
			continue;
		}
		*d = *s;

	}
	return error(-1, FMT1S, dgn[ERR_TMCHARS]);
}

/*
 * First, build an argument vector for the string cmd.  The resulting
 * argument vector, which specifies the argument list for the command
 * to be executed, is a pointer to a NULL-terminated array of pointers
 * to `\0'-terminated strings.  On success, call execute() and return
 * its value (-1, 0, 1).  Otherwise, return 0 if `Too many args'.
 */
static int
build_av(char *cmd, int *redir, int flags)
{
	int ac, done, idx;
	char *av[ARGMAX];
	char *larg, *p;

#ifdef	DEBUG
	if (cmd == NULL || *nonblank(cmd) == '\0')
		error(SH_DEBUG, "build_av: Invalid argument");
#endif

	for (ac = done = 0, p = larg = cmd; ; p++) {

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
			if (ac + 1 >= ARGMAX)
				break;
			av[ac++] = larg;
		}

		if (done) {
			av[ac] = NULL;
#ifdef	DEBUG
			if (av[0] == NULL)
				error(SH_DEBUG, "build_av: av[0] == NULL");
#endif
			if (prep_cmd(&idx, av))
				flags |= CMD_REDIR;
			return execute(idx, av, redir, flags);
		}
		larg = p + 1;

	}
	error(-1, FMT1S, dgn[ERR_TMARGS]);
	close_pr(redir);
	return 0;
}

/*
 * Prepare the command for execution by determining which argument is
 * the command name and whether there are any redirection arguments.
 * Place the index value of the command name into the integer pointed
 * to by idx.  Return 1 if redirection argument(s) or 0 if not.
 */
static int
prep_cmd(int *idx, char **av)
{
	int gotr, i;
	const char *p;

	p = NULL;
	gotr = i = 0;
	do {
		switch (*av[i]) {
		case '<':
			if (*(av[i] + 1) == '\0')
				i++;
			gotr = 1;
			break;
		case '>':
			if (*(av[i] + 1) == '\0' ||
			    (*(av[i] + 1) == '>' && *(av[i] + 2) == '\0'))
				i++;
			gotr = 1;
			break;
		default:
			if (p == NULL)
				p = av[i];
		}
#ifdef	DEBUG
		if (av[i] == NULL)
			error(SH_DEBUG, "prep_cmd: Invalid redirection");
#endif
	} while (av[++i] != NULL);
	*idx = which(p);
	return gotr;
}

/*
 * Determine whether name is a special built-in command.
 * If so, return its index value.  Otherwise, return -1
 * as name is either external, or not a command at all.
 */
static int
which(const char *name)
{
	int i;

#ifdef	DEBUG
	if (name == NULL)
		error(SH_DEBUG, "which: Invalid command name");
#endif

	for (i = 0; sbi[i] != NULL; i++)
		if (EQUAL(name, sbi[i]))
			return i;
	return -1;
}

#ifndef	SH6
/*
 * Try to get the terminal name for $t.  If dupfd0 is associated
 * w/ a terminal device, save a copy of and return a pointer to
 * its name on success.  Otherwise, save a copy of and return a
 * pointer to the empty string.
 */
static char *
get_tty(void)
{
	const char *p;

	p = ttyname(dupfd0);

	return xstrdup((p != NULL) ? p : "");
}

/*
 * Try to get the effective user name for $u.  Save a copy of and
 * return a pointer to it on success.  Otherwise, save a copy of
 * and return a pointer to the empty string.
 */
static char *
get_user(void)
{
	struct passwd *pwu;

	pwu = getpwuid(si.euid);

	return xstrdup((pwu != NULL) ? pwu->pw_name : "");
}
#endif	/* !SH6 */
