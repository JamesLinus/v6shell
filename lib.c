/*
 * lib.c - osh library routines
 */
/*-
 * Copyright (c) 2004-2017
 *	Jeffrey Allen Neitzel <jan (at) etsh (dot) io>.
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
 *	@(#)$Id$
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

#define	OSH_SHELL

#include "defs.h"
#include "sh.h"
#include "err.h"
#include "lib.h"

/*
 * **** Global Variables ****
 */
static	const char	**gavp;	/* points to current gav position     */
static	const char	**gave;	/* points to current gav end          */
static	unsigned	gavmult;/* GAVNEW reallocation multiplier     */
static	size_t		gavtot;	/* total bytes used for all arguments */

/*
 * **** Function Prototypes ****
 */
/*@null@*/
static	char		*gtrim(/*@returned@*/ UChar *);
static	const char	**gnew(/*@only@*/ const char **);
/*@null@*/
static	char		*gcat(/*@null@*/ const char *,
			      /*@null@*/ const char *, bool);
static	const char	**glob1(enum sbikey, /*@only@*/ const char **,
				char *, int *, bool *);
static	bool		glob2(const UChar *, const UChar *);
static	void		gsort(const char **);
/*@null@*/
static	DIR		*gopendir(/*@out@*/ char *, const char *);

/*
 * Print diagnostic w/ $0, line number, and so forth if possible.
 */
void
error(int s, const char *m)
{
	long ln;

	if (m == NULL) {
		err(ESTATUS, FMT3S, getmyname(), __func__, strerror(EINVAL));
		/*NOTREACHED*/
	}

	ln = (is_noexec || no_lnum) ? -1 : get_lnum();
	if (name != NULL) {
		if (ln != -1)
			err(s, FMT3LS, getmyname(), name, ln, m);
		else
			err(s, FMT3S, getmyname(), name, m);
	} else {
		if (ln != -1)
			err(s, FMT2LS, getmyname(), ln, m);
		else
			err(s, FMT2S, getmyname(), m);
	}
}

/*
 * Print diagnostic w/ $0, line number, and so forth if possible.
 */
void
error1(int s, const char *f, const char *m)
{
	long ln;

	if (f == NULL || m == NULL) {
		err(ESTATUS, FMT3S, getmyname(), __func__, strerror(EINVAL));
		/*NOTREACHED*/
	}

	ln = (is_noexec || no_lnum) ? -1 : get_lnum();
	if (name != NULL) {
		if (ln != -1)
			err(s, FMT4LS, getmyname(), name, ln, f, m);
		else
			err(s, FMT4S, getmyname(), name, f, m);
	} else {
		if (ln != -1)
			err(s, FMT3LFS, getmyname(), ln, f, m);
		else
			err(s, FMT3S, getmyname(), f, m);
	}
}

/*
 * Print diagnostic w/ $0, line number, and so forth if possible.
 */
void
error2(int s, const char *c, const char *a, const char *m)
{
	long ln;

	if (c == NULL || a == NULL || m == NULL) {
		err(ESTATUS, FMT3S, getmyname(), __func__, strerror(EINVAL));
		/*NOTREACHED*/
	}

	ln = (is_noexec || no_lnum) ? -1 : get_lnum();
	if (name != NULL) {
		if (ln != -1)
			err(s, FMT5LS, getmyname(), name, ln, c, a, m);
		else
			err(s, FMT5S, getmyname(), name, c, a, m);
	} else {
		if (ln != -1)
			err(s, FMT4LFS, getmyname(), ln, c, a, m);
		else
			err(s, FMT4S, getmyname(), c, a, m);
	}
}

/*
 * Prepare possible glob() pattern pointed to by ap.
 *
 *	1) Remove (trim) any unquoted quote characters;
 *	2) Escape (w/ backslash `\') any previously quoted
 *	   glob or quote characters as needed;
 *	3) Reallocate memory for (if needed), make copy of,
 *	   and return pointer to new glob() pattern, nap.
 *
 * This function returns NULL on error.
 */
static char *
gtrim(UChar *ap)
{
	size_t siz;
	UChar *a, *b, *nap;
	UChar buf[PATHMAX], c;
	bool d;

	*buf = UCHAR(EOS);
	for (a = ap, b = buf; b < &buf[PATHMAX]; a++, b++) {
		switch (*a) {
		case EOS:
			*b = UCHAR(EOS);
			siz = (b - buf) + 1;
			nap = ap;
			if (siz > strlen((const char *)ap) + 1) {
				xfree(nap);
				nap = xmalloc(siz);
			}
			(void)memcpy(nap, buf, siz);
			return (char *)nap;
		case DQUOT:
		case SQUOT:
			c = *a++;
			d = (c == DQUOT) ? true : false;
			while (*a != c && b < &buf[PATHMAX]) {
				switch (*a) {
				case BQUOT:
					if (d && *(a + 1) == DOLLAR)
						a++;
					/*FALLTHROUGH*/
				case ASTERISK: case QUESTION:
				case LBRACKET: case RBRACKET: case HYPHEN:
				case DQUOT:    case SQUOT:
					*b = UCHAR(BQUOT);
					if (++b >= &buf[PATHMAX])
						goto gterr;
					break;
				}
				*b++ = *a++;
			}
			b--;
			continue;
		case BQUOT:
			switch (*++a) {
			case EOS:
				a--, b--;
				continue;
			case ASTERISK: case QUESTION:
			case LBRACKET: case RBRACKET: case HYPHEN:
			case DQUOT:    case SQUOT:    case BQUOT:
				*b = UCHAR(BQUOT);
				if (++b >= &buf[PATHMAX])
					goto gterr;
				break;
			}
			break;
		}
		*b = *a;
	}

gterr:
	error(-2, (gchar((const char *)ap) != NULL) ?
	      ERR_PATTOOLONG : strerror(ENAMETOOLONG));
	return NULL;
}

/*
 * Return pointer to first unquoted glob character (`*', `?', `[')
 * in argument pointed to by ap.  Otherwise, return NULL pointer
 * on error or if argument contains no glob characters.
 */
char *
gchar(const char *ap)
{
	char c;
	const char *a;

	for (a = ap; *a != EOS; a++)
		switch (*a) {
		case DQUOT:
		case SQUOT:
			for (c = *a++; *a != c; a++)
				if (*a == EOS)
					return NULL;
			continue;
		case BQUOT:
			if (*++a == EOS)
				return NULL;
			continue;
		case ASTERISK:
		case QUESTION:
		case LBRACKET:
			return (char *)a;
		}
	return NULL;
}

/*
 * Deallocate the memory allocation pointed to by p.
 */
void
xfree(void *p)
{

	if (p != NULL) {
		free(p);
		p = NULL;
	}
}

/*
 * Allocate memory, and check for error.
 * Return a pointer to the allocated space on success.
 * Do not return on ENOMEM error.
 */
void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL) {
		error(ESTATUS, ERR_NOMEM);
		/*NOTREACHED*/
	}
	return mp;
}

/*
 * Deallocate the argument vector pointed to by vp.
 */
void
vfree(char **vp)
{
	char **p;

	if (vp != NULL) {
		for (p = vp; *p != NULL; p++) {
			free(*p);
			*p = NULL;
		}
		free(vp);
		vp = NULL;
	}
}

/*
 * Reallocate memory, and check for error.
 * Return a pointer to the reallocated space on success.
 * Do not return on ENOMEM error.
 */
void *
xrealloc(void *p, size_t s)
{
	void *rp;

	if ((rp = realloc(p, s)) == NULL) {
		error(ESTATUS, ERR_NOMEM);
		/*NOTREACHED*/
	}
	return rp;
}

/*
 * Attempt to generate file-name arguments which match the given
 * pattern arguments in av.  Return pointer to newly allocated
 * argument vector, gav, on success.  Return NULL on error.
 */
const char **
glob(enum sbikey key, char **av)
{
	char *tpap;		/* temporary pattern argument pointer  */
	char **oav;		/* points to original argument vector  */
	const char **gav;	/* points to generated argument vector */
	int pmc = 0;		/* pattern match count                 */
	bool gerr = false;	/* glob error flag                     */

	gavmult = 1;
	gavtot  = 0;

	oav  = av;
	gav  = xmalloc(GAVNEW * sizeof(char *));
	*gav = NULL;
	gavp = gav;
	gave = &gav[GAVNEW - 1];
	while (*av != NULL) {
		if ((tpap = gtrim(UCPTR(*av))) == NULL) {
			*gavp = NULL;
			gerr  = true;
			break;
		}
		*av = tpap;/* for successful vfree(oav); */
		gav = glob1(key, gav, tpap, &pmc, &gerr);
		if (gerr)
			break;
		av++;
	}
	gavp = NULL;

	if (pmc == 0 && !gerr) {
		error(-2, ERR_NOMATCH);
		gerr = true;
	}
	if (gerr) {
		vfree((char **)gav);
		gav = NULL;
	}
	vfree(oav);
	oav = NULL;
	return gav;
}

static const char **
gnew(const char **gav)
{
	size_t siz;
	ptrdiff_t gidx;

	if (gavp == gave) {
		gavmult *= GAVMULT;
#ifdef	DEBUG
#ifdef	DEBUG_GLOB
		fd_print(FD2, "%s: gavmult == %u;\n", __func__, gavmult);
#endif
#endif
		gidx = (ptrdiff_t)(gavp - gav);
		siz  = (size_t)((gidx + (GAVNEW * gavmult)) * sizeof(char *));
#ifdef	DEBUG
#ifdef	DEBUG_GLOB
		fd_print(FD2, "    : (GAVNEW * gavmult) == %u, siz == %zu;\n",
		    (GAVNEW * gavmult), siz);
#endif
#endif
		gav  = xrealloc(gav, siz);
		gavp = gav + gidx;
		gave = &gav[gidx + (GAVNEW * gavmult) - 1];
	}
	return gav;
}

static char *
gcat(const char *src1, const char *src2, bool slash)
{
	size_t siz;
	char *b, buf[PATHMAX], c, *dst;
	const char *s;

	if (src1 == NULL || src2 == NULL) {
		/* never true, but appease splint(1) */
		error1(-2, __func__, "Invalid argument");
		return NULL;
	}

	*buf = EOS;
	b = buf;
	s = src1;
	while ((c = *s++) != EOS) {
		if (b >= &buf[PATHMAX - 1]) {
			error(-2, strerror(ENAMETOOLONG));
			return NULL;
		}
		*b++ = c;
	}
	if (slash)
		*b++ = SLASH;
	s = src2;
	do {
		if (b >= &buf[PATHMAX]) {
			error(-2, strerror(ENAMETOOLONG));
			return NULL;
		}
		*b++ = c = *s++;
	} while (c != EOS);
	b--;

	siz = (b - buf) + 1;
	gavtot += siz;
	if (gavtot > GAVMAX) {
		error(-2, ERR_E2BIG);
		return NULL;
	}
#ifdef	DEBUG
#ifdef	DEBUG_GLOB
	fd_print(FD2, "%s: siz == %zu, (%p < %p) == %s;\n", __func__,
	    siz, (void *)b, (void *)&buf[PATHMAX],
	    (b < &buf[PATHMAX]) ? "true" : "false");
	fd_print(FD2, "    : strlen(buf) == %zu;\n", strlen(buf));
#endif
#endif
	dst = xmalloc(siz);

	(void)memcpy(dst, buf, siz);
	return dst;
}

static const char **
glob1(enum sbikey key, const char **gav, char *as, int *pmc, bool *gerr)
{
	DIR *dirp;
	struct dirent *entry;
	ptrdiff_t gidx;
	bool slash;
	char dirbuf[PATHMAX], *p, *ps;
	const char *ds;

	ds = as;
	slash = false;
	if ((ps = gchar(as)) == NULL) {
		gav = gnew(gav);
		if (DO_TRIM(key))
			(void)atrim(UCPTR(as));
		if ((p = gcat(as, "", slash)) == NULL) {
			*gavp = NULL;
			*gerr = true;
			return gav;
		}
		*gavp++ = p;
		*gavp = NULL;
		return gav;
	}
	for (;;) {
		if (ps == ds) {
			ds = "";
			break;
		}
		if (*--ps == SLASH) {
			*ps = EOS;
			if (ds == ps)
				ds = "/";
			else
				slash = true;
			ps++;
			break;
		}
	}
	if ((dirp = gopendir(dirbuf, *ds != EOS ? ds : ".")) == NULL) {
		error(-2, ERR_NODIR);
		*gavp = NULL;
		*gerr = true;
		return gav;
	}
	if (*ds != EOS)
		ds = dirbuf;
	gidx = (ptrdiff_t)(gavp - gav);
	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_name[0] == DOT && *ps != DOT)
			continue;
		if (glob2(UCPTR(entry->d_name), UCPTR(ps))) {
			gav = gnew(gav);
			if ((p = gcat(ds, entry->d_name, slash)) == NULL) {
				(void)closedir(dirp);
				*gavp = NULL;
				*gerr = true;
				return gav;
			}
			*gavp++ = p;
			(*pmc)++;
		}
	}
	(void)closedir(dirp);
	gsort(gav + gidx);
	*gavp = NULL;
	*gerr = false;
	return gav;
}

static bool
glob2(const UChar *ename, const UChar *pattern)
{
	int cok, rok;		/* `[...]' - cok (class), rok (range) */
	UChar c, ec, pc;
	const UChar *e, *n, *p;

	e = ename;
	p = pattern;

	ec = *e++;
	switch (pc = *p++) {
	case EOS:
		return ec == EOS;

	case ASTERISK:
		/*
		 * Ignore all but the last `*' in a group of consecutive
		 * `*' characters to avoid unnecessary glob2() recursion.
		 */
		while (*p++ == ASTERISK)
			;	/* nothing */
		if (*--p == EOS)
			return true;
		e--;
		while (*e != EOS)
			if (glob2(e++, p))
				return true;
		break;

	case QUESTION:
		if (ec != EOS)
			return glob2(e, p);
		break;

	case LBRACKET:
		if (*p == EOS)
			break;
		for (c = UCHAR(EOS), cok = rok = 0, n = p + 1; ; ) {
			pc = *p++;
			if (pc == RBRACKET && p > n) {
				if (cok > 0 || rok > 0)
					return glob2(e, p);
				break;
			}
			if (*p == EOS)
				break;
			if (pc == HYPHEN && c != EOS && *p != RBRACKET) {
				if ((pc = *p++) == BQUOT)
					pc = *p++;
				if (*p == EOS)
					break;
				if (c <= ec && ec <= pc)
					rok++;
				else if (c == ec)
					cok--;
				c = UCHAR(EOS);
			} else {
				if (pc == BQUOT) {
					pc = *p++;
					if (*p == EOS)
						break;
				}
				c = pc;
				if (ec == c)
					cok++;
			}
		}
		break;

	case BQUOT:
		if (*p != EOS)
			pc = *p++;
		/*FALLTHROUGH*/

	default:
		if (pc == ec)
			return glob2(e, p);
	}
	return false;
}

static void
gsort(const char **ogavp)
{
	const char **p1, **p2, *sap;

	p1 = ogavp;
	while (gavp - p1 > 1) {
		p2 = p1;
		while (++p2 < gavp)
			if (strcmp(*p1, *p2) > 0) {
				sap = *p1;
				*p1 = *p2;
				*p2 = sap;
			}
		p1++;
	}
}

static DIR *
gopendir(char *buf, const char *dname)
{
	char *b;
	const char *d;

	for (*buf = EOS, b = buf, d = dname; b < &buf[PATHMAX]; b++, d++)
		if ((*b = *d) == EOS) {
			(void)atrim(UCPTR(buf));
			break;
		}
	return (b >= &buf[PATHMAX]) ? NULL : opendir(buf);
}
