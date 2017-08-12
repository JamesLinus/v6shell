/*
 * osh - an enhanced port of the Sixth Edition (V6) UNIX Thompson shell
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

#ifndef	LIB_H
#define	LIB_H

#define	DO_TRIM(k)	((k) != SBI_CD && (k) != SBI_CHDIR)

/* osh.c */
extern	const char	*name;		/* $0 - shell command name  */
extern	bool		is_noexec;	/* not executable file flag */
extern	bool		no_lnum;	/* no line number flag      */

/* lib.c */
void	error(int, /*@null@*/ const char *);
void	error1(int, /*@null@*/ const char *, /*@null@*/ const char *);
void	error2(int, /*@null@*/ const char *, /*@null@*/ const char *, /*@null@*/ const char *);
/*@maynotreturn@*/ /*@null@*/ /*@only@*/
const char	**glob(enum sbikey, /*@only@*/ char **);
/*@null@*/
char		*gchar(/*@returned@*/ const char *);
void		xfree(/*@null@*/ /*@only@*/ void *);
/*@maynotreturn@*/ /*@out@*/
void		*xmalloc(size_t);
void		vfree(/*@null@*/ char **);
/*@maynotreturn@*/
void		*xrealloc(/*@only@*/ void *, size_t);

#endif	/* !LIB_H */
