/*
 * osh-misc.c - miscellaneous support routines for osh(1)
 */
/*-
 * Copyright (c) 2003, 2004, 2005, 2006
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
 *	@(#)osh-misc.c	1.5 (jneitzel) 2006/01/20
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
OSH_SOURCEID("osh-misc.c	1.5 (jneitzel) 2006/01/20");
#endif	/* !lint */

#include "osh.h"

#define	FMTSIZE		64
#if LINEMAX >= PATHMAX
#define	MSGSIZE		(FMTSIZE + LINEMAX)
#else
#define	MSGSIZE		(FMTSIZE + PATHMAX)
#endif

#if defined(SIGIOT)
#define	XSIGIOT		SIGIOT
#elif defined(SIGABRT)
#define	XSIGIOT		SIGABRT
#endif

static void	omsg(int, const char *, va_list);

/*
 * Close file descriptors associated w/ pipes and/or redirects.
 */
void
close_pr(int *redir)
{

	if (redir[0] != -1)
		close(redir[0]);
	if (redir[1] != -1)
		close(redir[1]);
}

/*
 * Handle all errors detected by the shell.  This includes printing any
 * specified message to the standard error and setting the exit status.
 * If this function returns, the return value is always -1.  Otherwise,
 * it may cause the shell to terminate according to the value of es.
 * es > 0:
 *	The error is fatal unless it happens after a successful call
 *	to fork().  In such a case the child process terminates, but
 *	the main shell process ($$) continues.  Exit status is set to
 *	the value of es.
 * Otherwise:
 *	The error may be fatal according to the value of the global
 *	variable shtype.  If the shell is non-interactive and is reading
 *	commands from a file, a shell-detected error causes it to cease
 *	execution of that file.  If es == -1, exit status is set to the
 *	value of SH_ERR.  If es == 0, exit status is the value of the
 *	global variable status.
 */
int
error(int es, const char *msgfmt, ...)
{
	va_list va;

	if (msgfmt != NULL) {
		va_start(va, msgfmt);
		omsg(FD2, msgfmt, va);
		va_end(va);
	}
	switch (es) {
	case -1:
		status = SH_ERR;
		/* FALLTHROUGH */
	case 0:
		if (shtype & SOURCE) {
			lseek(FD0, (off_t)0, SEEK_END);
			err_source = 1;
			return -1;
		}
		if (shtype & RCFILE && (status == 0202 || status == 0203)) {
			lseek(FD0, (off_t)0, SEEK_END);
			return -1;
		}
		if (!(shtype & INTERACTIVE)) {
			if (!(shtype & CTOPTION))
				lseek(FD0, (off_t)0, SEEK_END);
			break;
		}
		return -1;
#ifdef	DEBUG
	case SH_DEBUG:
		abort();
#endif
	default:
		status = es;
	}
	EXIT(status);
}

/*
 * Print any specified message to the file descriptor pfd.
 */
void
fdprint(int pfd, const char *msgfmt, ...)
{
	va_list va;

	if (msgfmt != NULL) {
		va_start(va, msgfmt);
		omsg(pfd, msgfmt, va);
		va_end(va);
	}
}

/*
 * Check if the file descriptor fd refers to an open file
 * of the specified type; return 1 if true or 0 if false.
 */
int
fdtype(int fd, int type)
{
	struct stat sb;
	int rv = 0;

	if (fstat(fd, &sb) < 0) {
#ifdef	DEBUG
		fdprint(FD2, "fdtype: %d: %s", fd, strerror(errno));
#endif
		return rv;
	}

	switch (type) {
	case FD_OPEN:
		/* Descriptor refers to any type of open file. */
		rv = 1;
		break;
	case FD_DIROK:
		/* Does descriptor refer to an existent directory? */
		if (S_ISDIR(sb.st_mode))
			rv = sb.st_nlink > 0;
		break;
	case S_IFDIR:
	case S_IFREG:
		rv = (sb.st_mode & S_IFMT) == (mode_t)type;
		break;
	}
	return rv;
}

/*
 * Create and output the message specified by error() or fdprint() to
 * the file descriptor ofd.  The resulting message is terminated by
 * a newline unless it is too long, in which case it is truncated.
 */
static void
omsg(int ofd, const char *msgfmt, va_list va)
{
	char fmt[FMTSIZE];
	char msg[MSGSIZE];

	snprintf(fmt, sizeof(fmt), "%s\n", msgfmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	write(ofd, msg, strlen(msg));
}

/*
 * Print a termination message for a process according to signal s.
 * If p is greater than 0, precede the message w/ the process ID p.
 * Return a value of 0200 + signal s (signo), which can be used as
 * the exit status of the terminated process.
 */
int
sigmsg(int s, pid_t p)
{
	int signo;
	char buf[32];
	const char *core, *msg;

	/* First, determine which message to print (if any). */
	switch (signo = WTERMSIG(s)) {
	case SIGHUP:	msg = "Hangup";			break;
	case SIGINT:
		if (p > 0)
			goto msg_done;
		msg = "";
		break;
	case SIGQUIT:	msg = "Quit";			break;
	case SIGILL:	msg = "Illegal instruction";	break;
#ifdef	SIGTRAP
	case SIGTRAP:	msg = "Trace/BPT trap";		break;
#endif
#ifdef	XSIGIOT	/* This corresponds to SIGIOT or SIGABRT. */
	case XSIGIOT:	msg = "IOT trap";		break;
#endif
#ifdef	SIGEMT
	case SIGEMT:	msg = "EMT trap";		break;
#endif
	case SIGFPE:	msg = "Floating exception";	break;
	case SIGKILL:	msg = "Killed";			break;
#ifdef	SIGBUS
	case SIGBUS:	msg = "Bus error";		break;
#endif
	case SIGSEGV:	msg = "Memory fault";		break;
#ifdef	SIGSYS
	case SIGSYS:	msg = "Bad system call";	break;
#endif
#ifdef	DEBUG
	case SIGPIPE:	msg = "Broken pipe";		break;
#else
	case SIGPIPE:	goto msg_done;	/* never reported */
#endif
	default:
		/* Report by signal number. */
		snprintf(buf, sizeof(buf), "Sig %u", signo);
		msg = buf;
	}
	core = "";
#ifdef	WCOREDUMP
	if (WCOREDUMP(s))
		core = " -- Core dumped";
#endif

	/* Now, print the message. */
	if (p > 0)
		fdprint(FD2, "%u: %s%s", p, msg, core);
	else
		fdprint(FD2, "%s%s", msg, core);

msg_done:
	return 0200 + signo;
}

/*
 * Allocate memory, and check for error.
 * Return a pointer to the allocated space on success.
 * Do not return on error (ENOMEM).
 */
void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL)
		error(SH_ERR, FMT1S, dgn[ERR_NOMEM]);
	return mp;
}

/*
 * Allocate memory for a copy of the string src, and copy it to dst.
 * Return a pointer to dst on success.
 * Do not return on error (ENOMEM).
 */
char *
xstrdup(const char *src)
{
	size_t siz;
	char *dst;

	siz = strlen(src) + 1;
	dst = xmalloc(siz);
	memcpy(dst, src, siz);
	return dst;
}
