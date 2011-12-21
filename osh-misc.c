/*
 * osh-misc.c - miscellaneous support routines for osh and sh6
 */
/*
 * Copyright (c) 2003, 2004
 *	Jeffrey Allen Neitzel <jneitzel@sdf.lonestar.org>.
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
 *	@(#)osh-misc.c	1.1 (jneitzel) 2004/12/28
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
 = "\100(#)osh-misc.c	1.1 (jneitzel) 2004/12/28";
#endif	/* !lint */

#include "osh.h"

#define	MSGSIZE	LINESIZE

static void	omsg(int, const char *, va_list);

/*
 * Wait, fetch, and report termination status for asynchronous
 * child processes and exorcise any stray zombies.
 */
void
apwait(int opt)
{
	pid_t apid;
	int cstat, i;

	while ((apid = waitpid(-1, &cstat, opt)) > 0) {
		/* Check if apid is a known asynchronous process. */
		for (i = 0; i < apc; i++) {
			if (aplist[i] == 0) /* already reported */
				continue;
			if (aplist[i] == apid) { /* report termination */
				aplist[i] = 0;
				if (cstat) {
					if (WIFSIGNALED(cstat))
						sigmsg(cstat, apid);
					else if (WIFEXITED(cstat))
						status = WEXITSTATUS(cstat);

					/* special cases, special attention */
					if (status == 0215)
						continue;
					if (status >= 0176 && !PROMPT(shtype)) {
						errflg = 1;
						seek_eof();
						if (shtype & COMMANDFILE)
							DOEXIT(status);
					}
				} else
					status = 0;
				break;
			}
		}
	}

	/* Reset apc if there are no more asynchronous processes. */
	if (apid == -1 && apc > 0)
		apc = 0;
	apstat = apid;
}

/*
 * Handle all errors detected by the shell.  This includes printing
 * the specified message and setting the exit status.  This function
 * may cause the shell to terminate as determined by the value of es.
 * es == 0:
 *	The error is fatal when the shell takes commands from a file.
 *	This causes it to cease execution of the file.  Fatal or not,
 *	exit status is set to the value of SH_ERR.
 * es > 0:
 *	The error is fatal unless it happens after a successful call
 *	to fork().  In such a case the child process terminates, but
 *	the main shell process ($$) continues.  Exit status is set to
 *	the value of es.
 */
void
error(int es, const char *msgfmt, ...)
{
	va_list va;

	if (msgfmt != NULL) {
		va_start(va, msgfmt);
		omsg(FD2, msgfmt, va);
		va_end(va);
	}
	if (es == 0) {
		status = SH_ERR;
		if ((shtype & INTERACTIVE) && !(shtype & DOSOURCE))
			return;
		seek_eof();
		if (shtype & (COMMANDFILE|OTHER) && !(shtype & DOSOURCE))
			DOEXIT(status);
		errflg = 1;
		return;
	}
	status = es;
	DOEXIT(status);
}

/*
 * Print the specified message to the file descriptor pfd.
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
 * Check if the file descriptor fd refers to an open file.
 */
int
isopen(int fd)
{
	struct stat sb;

	return(fstat(fd, &sb) == 0);
}

/*
 * Create and output the message specified by error() or fdprint() to
 * the file descriptor ofd.  The resulting message is terminated by
 * a newline unless it is too long, in which case it is truncated.
 */
static void
omsg(int ofd, const char *msgfmt, va_list va)
{
	char fmt[MSGSIZE];
	char msg[MSGSIZE + 32];

	snprintf(fmt, sizeof(fmt), "%s\n", msgfmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	write(ofd, msg, strlen(msg));
}

/*
 * Seek to the end of the shell's standard input.
 * If the standard input is from a pipe or a FIFO,
 * eat the remaining input and throw it away.
 */
void
seek_eof(void)
{

	if (lseek(FD0, (off_t)0, SEEK_END) == -1)
		if (errno == ESPIPE)
			while (getchar() != EOF)
				;	/* nothing */
}

/*
 * Print termination message according to signal s.
 * If p != -1, precede message w/ process ID p (e.g., `&' commands).
 */
void
sigmsg(int s, pid_t p)
{
	int signo;
	char buf[32];
	const char *msg1, *msg2;

	signo  = WTERMSIG(s);
	status = signo + 0200;

	/* First, determine which message to print (if any). */
	switch (signo) {
	/* signals required by POSIX */
	case SIGPIPE:
	case SIGINT:	return;		/* not reported */
	case SIGQUIT:	msg1 = "Quit";			break;
	case SIGHUP:	msg1 = "Hangup";		break;
	case SIGILL:	msg1 = "Illegal instruction";	break;
	case SIGFPE:	msg1 = "Floating exception";	break;
	case SIGKILL:	msg1 = "Killed";		break;
	case SIGSEGV:	msg1 = "Memory fault";		break;
	/* signals not required by POSIX */
#ifdef	SIGTRAP
	case SIGTRAP:	msg1 = "Trace/BPT trap";	break;
#endif
#ifdef	SIGIOT	/* old name for the POSIX SIGABRT signal */
	case SIGIOT:	msg1 = "IOT trap";		break;
#endif
#ifdef	SIGEMT
	case SIGEMT:	msg1 = "EMT trap";		break;
#endif
#ifdef	SIGBUS
	case SIGBUS:	msg1 = "Bus error";		break;
#endif
#ifdef	SIGSYS
	case SIGSYS:	msg1 = "Bad system call";	break;
#endif
	default:
		/* Report by signal number. */
		snprintf(buf, sizeof(buf), "Sig %d", signo);
		msg1 = buf;
	}
	msg2 = (s & 0200) ? " -- Core dumped" : "";

	/* Now, print the message. */
	if (p != -1)
		fdprint(FD2, "%d: %s%s", p, msg1, msg2);
	else
		fdprint(FD2, "%s%s", msg1, msg2);
}

/*
 * Allocate memory and check for error.
 */
void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL)
		error(SH_FATAL, "Out of memory");
	return(mp);
}

/*
 * Reallocate memory and check for error.
 */
void *
xrealloc(void *rp, size_t s)
{

	if ((rp = realloc(rp, s)) == NULL)
		error(SH_FATAL, "Out of memory");
	return(rp);
}
