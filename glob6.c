/*
 * glob6 - a port of the Sixth Edition (V6) Unix global command
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
 *	@(#)glob6.c	1.2 (jneitzel) 2006/01/09
 */
/*
 *	Derived from: Sixth Edition Unix /usr/source/s1/glob.c
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
OSH_SOURCEID("glob6.c	1.2 (jneitzel) 2006/01/09");
#endif	/* !lint */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pexec.h"

/*
 * Define as 1 to exit w/ a `No directory' error when xopendir() returns NULL.
 * Otherwise, define as 0 to simply treat such a case as a failed match.
 */
#define	ERR_NODIR	1

#ifdef	_POSIX_ARG_MAX
#define	BUFMAX	_POSIX_ARG_MAX
#else
#define	BUFMAX	4096
#endif
#define	ARGMAX	512

#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		1024
#endif

#define	QUOTE	0200
#define	ASCII	0177

static char	*av[ARGMAX];	/* array of pointers to generated arguments */
static char	**avp = av;
static char	buf[BUFMAX];	/* buffer into which argument pointers point */
static char	*bp = buf;
static int	pmc;		/* pattern-match count */

static char	*cat(const char *, const char *);
static void	error(const char *, int);
static void	expand(char *);
static int	gmatch(const char *, const char *);
static void	sort(char **);
static DIR	*xopendir(const char *);

/*
 * NAME
 *	glob6 - global command (generate command arguments)
 *
 * SYNOPSIS
 *	glob6 command [arg ...]
 *
 * DESCRIPTION
 *	The glob6(1) utility attempts to generate file-name arguments for
 *	the specified command according to the specified pattern arguments:
 *
 *		`*'		- Matches any string of characters.
 *		`?'		- Matches any single character.
 *		`[...]'		- Matches any single character in the class.
 *				  A `-' has special meaning.  For example:
 *		`[0-9]'		- Matches `0' through `9'.
 *		`[A-Z]'		- Matches `A' through `Z'.
 *		`[a-z]'		- Matches `a' through `z'.
 *		`[0-9A-Za-z]'	- Matches any alphanumeric character.
 *
 *	Glob6 attempts to invoke the specified command, which may itself
 *	be specified as a pattern, with an argument list which is generated
 *	as described below.
 *
 *	If the argument does not contain `*', `?', or `[', glob6 uses it
 *	as is.  Otherwise, glob6 searches for file names in the current
 *	(or specified) directory which match the pattern argument.  It
 *	then sorts them in ascending ASCII order and uses them in the
 *	argument list of the specified command.
 *
 *	If an error is encountered, glob6 prints an appropriate diagnostic
 *	and exits with a non-zero status.
 */
int
main(int argc, char **argv)
{

	/*
	 * Set-ID execution is not supported.
	 */
	if (geteuid() != getuid() || getegid() != getgid())
		error("Set-ID execution denied\n", 2);

	if (argc < 2)
		error("Arg count\n", 2);

	argv += 1;
	argc -= 1;
	*avp = *argv;
	while (*argv != NULL)
		expand(*argv++);
	if (pmc == 0)
		error("No match\n", 2);

	pexec(av[0], av);
	if (errno == ENOEXEC)
		error("No shell!\n", 125);
	if (errno == E2BIG)
		error("Arg list too long\n", 126);
	error("Command not found.\n", 127);
	/* NOTREACHED */
	return 2;
}

static char *
cat(const char *src1, const char *src2)
{
	int c;
	char *d, *dst;
	const char *s;

	if (avp >= &av[ARGMAX - 1])
		error("Arg list too long\n", 126);

	dst = d = bp;
	s = src1;
	while ((c = *s++) != '\0') {
		if (d >= &buf[BUFMAX])
			error("Arg list too long\n", 126);
		if ((c &= ASCII) == '\0') {
			*d++ = '/';
			break;
		}
		*d++ = c;
	}
	s = src2;
	do {
		if (d >= &buf[BUFMAX])
			error("Arg list too long\n", 126);
		*d++ = c = *s++;
	} while (c != '\0');
	bp = d;
	return dst;
}

static void
error(const char *msg, int es)
{

	write(STDERR_FILENO, msg, strlen(msg));
	exit(es);
}

static void
expand(char *as)
{
	DIR *dirp;
	struct dirent *entry;
	char **oavp;
	char *ps;
	const char *ds;

	ds = ps = as;
	while (*ps != '*' && *ps != '?' && *ps != '[')
		if (*ps++ == '\0') {
			*avp++ = cat(ds, "");
			return;
		}
	if (strlen(as) >= PATHMAX)
		error("Pattern too long\n", 2);
	for (;;) {
		if (ps == ds) {
			ds = "";
			break;
		}
		if (*--ps == '/') {
			*ps = '\0';
			if (ds == ps)
				ds = "/";
			break;
		}
	}
	if ((dirp = xopendir(*ds != '\0' ? ds : ".")) == NULL)
#if ERR_NODIR
		error("No directory\n", 2);
#else
		return;
#endif
	if (*ps == '\0')
		*ps++ = (char)QUOTE;
	oavp = avp;
	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_name[0] == '.' && *ps != '.')
			continue;
		if (gmatch(entry->d_name, ps)) {
			*avp++ = cat(ds, entry->d_name);
			pmc++;
		}
	}
	closedir(dirp);
	sort(oavp);
}

static int
gmatch(const char *ename, const char *pattern)
{
	int c, ec, ok, pc;
	const char *e, *p;

	e = ename;
	p = pattern;
	if ((ec = *e++) != '\0')
		if ((ec &= ASCII) == '\0')
			ec = QUOTE;

	switch (pc = *p++) {
	case '\0':
		return ec == '\0';

	case '*':
		/*
		 * Ignore all but the last `*' in a group of consecutive
		 * `*' characters to avoid unnecessary gmatch() recursion.
		 */
		while (*p++ == '*')
			;	/* nothing */
		if (*--p == '\0')
			return 1;
		e--;
		while (*e != '\0')
			if (gmatch(e++, p))
				return 1;
		break;

	case '?':
		if (ec != '\0')
			return gmatch(e, p);
		break;

	case '[':
		c = ok = 0;
		while ((pc = *p++) != '\0') {
			if (pc == ']' && c != '\0') {
				if (ok)
					return gmatch(e, p);
				break;
			}
			if (*p == '\0')
				break;
			if (pc == '-' && c != '\0' && *p != ']') {
				pc = *p++ & ASCII;
				if (*p == '\0')
					break;
				if (c <= ec && ec <= pc)
					ok = 1;
			} else {
				c = pc & ASCII;
				if (ec == c)
					ok = 1;
			}
		}
		break;

	default:
		if ((pc & ASCII) == ec)
			return gmatch(e, p);
	}
	return 0;
}

static void
sort(char **oavp)
{
	char **a1, **a2, *sav;

	a1 = oavp;
	while (a1 < avp - 1) {
		a2 = a1;
		while (++a2 < avp)
			if (strcmp(*a1, *a2) > 0) {
				sav = *a1;
				*a1 = *a2;
				*a2 = sav;
			}
		a1++;
	}
}

static DIR *
xopendir(const char *dname)
{
	char *b, buf[PATHMAX];
	const char *d;

	for (b = buf, d = dname; b < &buf[PATHMAX]; b++, d++)
		if ((*b = (*d & ASCII)) == '\0')
			break;
	return (b >= &buf[PATHMAX]) ? NULL : opendir(buf);
}
