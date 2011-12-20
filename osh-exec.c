/*
 * osh-exec.c - command execution routines for osh and sh6
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
 *	@(#)osh-exec.c	1.5 (jneitzel) 2005/10/19
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
OSH_SOURCEID("osh-exec.c	1.5 (jneitzel) 2005/10/19");
#endif	/* !lint */

#include "osh.h"
#include "pexec.h"

#ifndef	SH6
#define	RED_OPEN(c)	((c) == -1 || (c) == SBIEXEC)
#else	/* SH6 */
#define	RED_OPEN(c)	((c) == -1)
#endif	/* SH6 */

#define	PLMAX		64	/* maximum # of process IDs in each list */

static int	apc;		/* asynchronous process count            */
static pid_t	aplst[PLMAX];	/* list of asynchronous process IDs      */
static int	spc;		/* sequential process count              */
static pid_t	splst[PLMAX];	/* list of sequential process IDs        */

#ifndef	SH6
/*
 * (NSIG - 1) is the maximum signal number value accepted by `sigign'.
 */
# ifndef	NSIG
#  define	NSIG		16
# endif

/* signal state flags */
# define	SS_SIGINT	01
# define	SS_SIGQUIT	02
# define	SS_SIGTERM	04

static int	sigs;		/* signal state for SIGINT, SIGQUIT, SIGTERM */
#endif	/* !SH6 */

static int	gchar(const char *);
static void	glob_av(char **);
static char	*gtrim(char *);
static int	redirect(int, char **, int *);
static void	trim(char *);
static void	try_exec(char **, int *, int);
static int	try_fork(char **, int *, int);
static int	try_sbic(int, char **, int *, int);
#ifndef	SH6
static int	do_chdir(char **);
static int	do_sig(char **);
static int	do_source(char **);
static void	sigflags(void (*)(int), int);
#endif	/* !SH6 */

/*
 * Wait, and try to generate an appropriate termination report, for
 * each asynchronous child process.  The value of opt may be either
 * 0 or WNOHANG.  Its value determines how the wait is performed.
 */
void
apwait(int opt)
{
	pid_t apid;
	int cstat, gap, i, j;

	if (apc == 0)
		return;

#ifdef	DEBUG
	fprintf(stderr, "apwait: aplst (%u) == ", apc);
	for (i = 0; i < apc; i++)
		fprintf(stderr, "%u%s", aplst[i], (i == apc - 1) ? "\n" : ", ");
#endif

	gap = 0;
	while ((apid = waitpid(-1, &cstat, opt)) > 0)
		for (i = 0; i < apc; i++)
			if (aplst[i] == apid) {
				if (cstat && WIFSIGNALED(cstat))
					sigmsg(cstat, aplst[i]);
				aplst[i] = 0;
				gap = 1;
				break;
			}

	/* Close any gaps in aplst, and modify apc as needed. */
	if (apid == 0 && gap) {
		i = j = 0;
		do {
			while (i < apc && aplst[i] == 0)
				i++;
			if (i < apc)
				aplst[j++] = aplst[i++];
		} while (i < apc);
		apc = j;
	} else if (apid == -1)
		apc = 0;
}

/*
 * Wait for all sequential child processes.  The value of opt may be
 * 0, SPW_DOZAP, or SPW_DOSBI.  Its value determines how termination
 * of a sequential child process is reported.
 */
void
spwait(int opt)
{
	pid_t spid;
	int cstat, i, is_err;

	if (spc == 0)
		return;

	for (i = is_err = 0; i < spc; i++) {
		if (waitpid(splst[i], &cstat, 0) == -1) {
			error(-1, "%u: %s", splst[i], strerror(ESRCH));
			break;
		}
		if (opt == SPW_DOZAP)
			continue;
		if (cstat) {
			if (WIFSIGNALED(cstat)) {
				if (opt == SPW_DOSBI)
					spid = splst[i];
				else
					spid = (i == spc - 1) ? 0 : splst[i];
				status = sigmsg(cstat, spid);
			} else if (WIFEXITED(cstat))
				status = WEXITSTATUS(cstat);
			if (status == 0215)
				continue;
			if (status >= 0175)
				is_err = 1;
		} else
			status = 0;
	}
	if (opt == 0 && is_err)
		error(0, NULL);
	spc = 0;
}

/*
 * Do all of the final command preparations.  Then, call try_fork()
 * to execute an external command or try_sbic() to execute a special
 * built-in command.  In either case, return the value returned by
 * the called function, which is -1, 0, or 1.
 */
int
execute(int idx, char **av, int *redir, int flags)
{
	int dowait = 0, i, rv, ss;

	/*
	 * Try to setup I/O redirection as needed.
	 */
	if (flags & CMD_REDIR)
		if (!redirect(idx, av, redir))
			return -1;

	if (idx == -1)
		return try_fork(av, redir, flags);

	/*
	 * Prepare for special built-in commands as needed.
	 */
#ifndef	SH6
	if (idx != SBIEXEC) {
#endif	/* !SH6 */
		if (redir[0] != -1 || redir[1] != -1) {
			if (redir[1] == -1 && spc > 0)
				dowait = 1;
			close_pr(redir);	/* widow open pipes */
		}
		if (idx != SBICHDIR && idx != SBILOGIN)
			for (i = 1; av[i] != NULL; i++)
				trim(av[i]);
#ifndef	SH6
	}
#endif	/* !SH6 */

	rv = try_sbic(idx, av, redir, flags);
	if (dowait) {
		/* Save and restore current status while waiting... */
		ss = status;
		spwait(SPW_DOSBI);
		status = ss;
	}
	return rv;
}

/*
 * Try to execute the special built-in command specified by idx.
 * When try_exec() is called for `login' or `exec', do not return.
 * Otherwise, return 0 on success or -1 on error.
 */
static int
try_sbic(int idx, char **av, int *redir, int flags)
{
#ifndef	SH6
	mode_t m;
	const char *p;
#endif	/* !SH6 */

	switch (idx) {
	case SBINULL:
		/*
		 * Do nothing and set the exit status to zero.
		 */
		status = 0;
		return 0;

	case SBICHDIR:
		/*
		 * Change the shell's working directory.
		 */
#ifndef	SH6
		return do_chdir(av);
#else	/* SH6 */
		if (av[1] == NULL)
			return error(-1, FMT2S, av[0], dgn[ERR_ARGCOUNT]);
		trim(av[1]);
		if (chdir(av[1]) == -1)
			return error(-1, FMT2S, av[0], dgn[ERR_BADDIR]);
		status = 0;
		return 0;
#endif	/* SH6 */

	case SBIEXIT:
		/*
		 * If the shell is invoked w/ the `-c' or `-t' option, or is
		 * executing an initialization file, exit the shell outright
		 * if it is not sourcing another file in the current context.
		 * Otherwise, cause the shell to cease execution of a file of
		 * commands by seeking to the end of the file (and explicitly
		 * exiting the shell only if the file is not being sourced).
		 */
		if (!PROMPT(shtype)) {
			if (shtype & (CTOPTION|RCFILE) && !(shtype & SOURCE))
				EXIT(status);
			lseek(FD0, (off_t)0, SEEK_END);
			if (!(shtype & SOURCE))
				EXIT(status);
		}
		return 0;

	case SBILOGIN:
		/*
		 * Replace the current interactive shell w/ an
		 * instance of login(1).
		 */
		if (PROMPT(shtype) &&
		    !(flags & (CMD_ASYNC|CMD_PIPE)) && redir[0] == -1) {
			av[0] = xstrdup(_PATH_LOGIN);
			break;
		}
		return error(-1, FMT2S, av[0], dgn[ERR_EXEC]);

	case SBISHIFT:
		/*
		 * Shift all positional-parameter values to the left by 1.
		 * The value of $0 (si.name) does not shift.
		 */
		if (ppac <= 1)
			return error(-1, FMT2S, av[0], dgn[ERR_NOARGS]);
		ppav = &ppav[1];
		ppac--;
		status = 0;
		return 0;

	case SBIWAIT:
		/*
		 * Wait for all asynchronous processes to terminate,
		 * reporting on abnormal terminations.
		 */
		apwait(0);
		status = 0;
		return 0;

#ifndef	SH6
	case SBISIGIGN:
		/*
		 * Ignore (or unignore) the specified signals, or
		 * print a list of the signals currently ignored
		 * because of `sigign'.
		 *
		 * usage: sigign [+ | - signal_number ...]
		 */
		return do_sig(av);

	case SBISETENV:
		/*
		 * Set the specified environment variable.
		 *
		 * usage: setenv name [value]
		 */
		if (av[1] != NULL && (av[2] == NULL || av[3] == NULL)) {

			for (p = av[1]; *p != '=' && *p != '\0'; p++)
				;	/* nothing */
			if (*av[1] == '\0' || *p == '=')
				return error(-1, FMT3S, av[0],
					    av[1], dgn[ERR_BADNAME]);
			p = (av[2] != NULL) ? av[2] : "";
			if (setenv(av[1], p, 1) == -1)
				error(SH_ERR, FMT1S, dgn[ERR_NOMEM]);

			status = 0;
			return 0;

		}
		return error(-1, FMT2S, av[0], dgn[ERR_ARGCOUNT]);

	case SBIUNSETENV:
		/*
		 * Unset the specified environment variable.
		 *
		 * usage: unsetenv name
		 */
		if (av[1] != NULL && av[2] == NULL) {

			for (p = av[1]; *p != '=' && *p != '\0'; p++)
				;	/* nothing */
			if (*av[1] == '\0' || *p == '=')
				return error(-1, FMT3S, av[0],
					    av[1], dgn[ERR_BADNAME]);
			unsetenv(av[1]);

			status = 0;
			return 0;

		}
		return error(-1, FMT2S, av[0], dgn[ERR_ARGCOUNT]);

	case SBIUMASK:
		/*
		 * Set the file creation mask to the specified
		 * octal value, or print its current value.
		 *
		 * usage: umask [mask]
		 */
		if (av[1] != NULL && av[2] != NULL)
			return error(-1, FMT2S, av[0], dgn[ERR_ARGCOUNT]);
		if (av[1] == NULL) {
			umask(m = umask(0));
			fdprint(FD1, "%04o", m);
		} else {
			for (m = 0, p = av[1]; *p >= '0' && *p <= '7'; p++)
				m = m * 8 + (*p - '0');
			if (*av[1] == '\0' || *p != '\0' || m > 0777)
				return error(-1, FMT3S, av[0],
					    av[1], dgn[ERR_BADMASK]);
			umask(m);
		}
		status = 0;
		return 0;

	case SBISOURCE:
		/*
		 * Read and execute commands from file and return.
		 *
		 * usage: source file [arg1 ...]
		 */
		if (av[1] == NULL)
			return error(-1, FMT2S, av[0], dgn[ERR_ARGCOUNT]);
		return do_source(av);

	case SBIEXEC:
		/*
		 * Replace the current shell w/ an instance of
		 * the specified command.
		 *
		 * usage: exec command [arg ...]
		 */
		if (av[1] != NULL && !(flags & (CMD_ASYNC|CMD_PIPE))) {
			spwait(SPW_DOSBI);
			av = &av[1];
			break;
		}
		return error(-1, FMT2S, av[0],
			    dgn[(av[1] == NULL) ? ERR_ARGCOUNT : ERR_EXEC]);
#endif	/* !SH6 */

	default:
		return error(-1, FMT2S, av[0], dgn[ERR_EXEC]);
	}
	try_exec(av, redir, flags);
	EXIT(SH_ERR);
}

/*
 * Try to fork and execute an external command, add its process ID to
 * the appropriate process list, and wait for it to terminate if needed.
 *
 * Return values:
 *	1	The shell created a new process and waited for it,
 *		and possibly other processes, to terminate.
 *	0	The shell created a new process and has not yet
 *		waited for it to terminate.
 *	-1	The shell cannot create a new process.
 */
static int
try_fork(char **av, int *redir, int flags)
{
	pid_t cpid;

	if ((!(flags&CMD_ASYNC) && spc<PLMAX) || (flags&CMD_ASYNC && apc<PLMAX))
		cpid = fork();
	else
		cpid = -1;

	switch (cpid) {
	case -1:
		return error(-1, FMT1S, dgn[ERR_FORK]);

	case 0:
		/**** Child! ****/
		try_exec(av, redir, flags);
		_exit(SH_ERR);

	default:
		/**** Parent! ****/
		close_pr(redir);

		/* Track processes for termination reporting. */
		if (flags & CMD_ASYNC)
			aplst[apc++] = cpid;
		else
			splst[spc++] = cpid;

		if (flags & CMD_PIPE)
			return 0;
		if (flags & CMD_ASYNC) {
			fdprint(FD2, "%u", cpid);
			return 0;
		}
		spwait(0);
	}
	return 1;
}

/*
 * Try to perform the steps needed to replace the current process w/ a
 * new process (i.e., execute a new command).  The name of the command
 * to be executed is specified by av[0].  This function never returns.
 */
static void
try_exec(char **av, int *redir, int flags)
{
	int i;
	static int asub;
	char *p;

	/* pipes and I/O redirection */
	for (i = 0; i < 2; i++)
		if (redir[i] != -1) {
			if (dup2(redir[i], i) == -1)
				error(SH_ERR, "%s %u", dgn[ERR_REDIR], i);
			close(redir[i]);
		}

	/*
	 * If open, redir[2] is a reference to the read end of the
	 * pipe for the next command in the pipeline and must now
	 * be closed for this process.
	 */
	if (redir[2] != -1)
		close(redir[2]);

	/*
	 * Set the action for the SIGINT, SIGQUIT, and SIGTERM signals.
	 * Also, redirect input for `&' commands from `/dev/null' if needed.
	 */
	if (flags & CMD_ASYNC) {
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if (redir[0] == -1) {
			close(FD0);
			if (open("/dev/null", O_RDWR, 0) != FD0)
				error(SH_ERR,FMT2S,"/dev/null",dgn[ERR_OPEN]);
		}
	} else if (!asub) {
#ifndef	SH6
		if (!(sigs & SS_SIGINT) && chintr & CH_SIGINT)
			signal(SIGINT, SIG_DFL);
		if (!(sigs & SS_SIGQUIT) && chintr & CH_SIGQUIT)
			signal(SIGQUIT, SIG_DFL);
	}
	if (!(sigs & SS_SIGTERM))
		signal(SIGTERM, SIG_DFL);
#else	/* SH6 */
		if (chintr & CH_SIGINT)
			signal(SIGINT, SIG_DFL);
		if (chintr & CH_SIGQUIT)
			signal(SIGQUIT, SIG_DFL);
	}
	signal(SIGTERM, SIG_DFL);
#endif	/* SH6 */

	if (*av[0] == '(') {
		if (flags & CMD_ASYNC)
			asub = 1;
		*av[0] = ' ';
		p = av[0] + strlen(av[0]);
		while (*--p != ')')
			;	/* nothing */
		*p = '\0';
		spc = 0;
		pxline(av[0], PX_EXEC);
		_exit(status);
	}
	glob_av(av);

	/*
	 * NOTE: The Sixth Edition Unix shell always used the equivalent
	 *	 of `.:/bin:/usr/bin' to search for commands, not PATH.
	 */
	pexec(av[0], av);
	if (errno == ENOEXEC)
		error(125, FMT2S, av[0], dgn[ERR_NOSHELL]);
	if (errno != ENOENT && errno != ENOTDIR)
		error(126, FMT2S, av[0], dgn[ERR_EXEC]);
	error(127, FMT2S, av[0], dgn[ERR_NOTFOUND]);
}

/*
 * Redirect and/or strip away any redirection arguments according
 * to the value of idx.  If RED_OPEN(idx) is true, do a file open
 * and redirection strip.  Otherwise, only do a redirection strip.
 * Return 1 on success.
 * Return 0 on error.
 */
static int
redirect(int idx, char **av, int *redir)
{
	int fd;
	unsigned append, i, j;
#ifndef	SH6
	unsigned redfd0 = 0;
#endif	/* !SH6 */
	char *file, *p;

	append = i = 0;
	do {
		switch (*av[i]) {
		case '<':
			p = av[i] + 1;
			av[i] = (char *)-1;

			/* Get file name... */
			if (*p == '\0') {
				file = av[++i];
				av[i] = (char *)-1;
			} else
				file = p;
#ifndef	SH6
			/* Use original standard input instead of file? */
			if (*file == '-' && *(file + 1) == '\0')
				redfd0 = 1;
#endif	/* !SH6 */
			trim(file);

			/* Attempt to redirect input from file if possible. */
			if (RED_OPEN(idx) && redir[0] == -1) {
#ifndef	SH6
				if (redfd0)
					fd = dup(dupfd0);
				else
#endif	/* !SH6 */
					fd = open(file, O_RDONLY, 0);
				if (fd == -1) {
					error(-1, FMT2S, file, dgn[ERR_OPEN]);
					return 0;
				} else
					redir[0] = fd;
			}
			break;
		case '>':
			if (*(av[i] + 1) == '>') {
				append = 1;
				p = av[i] + 2;
			} else
				p = av[i] + 1;
			av[i] = (char *)-1;

			/* Get file name... */
			if (*p == '\0') {
				file = av[++i];
				av[i] = (char *)-1;
			} else
				file = p;
			trim(file);

			/* Attempt to redirect output to file if possible. */
			if (RED_OPEN(idx) && redir[1] == -1) {
				if (append)
					fd = open(file,
						O_WRONLY | O_APPEND | O_CREAT,
						0666);
				else
					fd = open(file,
						O_WRONLY | O_TRUNC | O_CREAT,
						0666);
				if (fd == -1) {
					error(-1, FMT2S, file, dgn[ERR_CREATE]);
					return 0;
				} else
					redir[1] = fd;
			}
			break;
		}
	} while (av[++i] != NULL);

	/* Strip away any redirection arguments. */
	i = j = 0;
	do {
		while (av[i] == (char *)-1)
			i++;
		if (av[i] != NULL)
			av[j++] = av[i++];
	} while (av[i] != NULL);
	av[j] = NULL;
	return 1;
}

/*
 * Expand arguments containing unquoted occurrences of any of the
 * glob characters `*', `?', or `['.  Arguments containing none of
 * the glob characters are used as is.  Never returns on error.
 *
 * NOTE: This function should only be called from the child process
 *	 after fork(), not from the parent process.
 */
static void
glob_av(char **av)
{
	glob_t gl;
	unsigned i, j, k, patc, patm;

	i = patc = patm = 0;
	do {
		if (!gchar(av[i])) {
			trim(av[i]);
			continue;
		}
		av[i] = gtrim(av[i]);

#ifdef	DEBUG
		fdprint(FD2, "glob_av: Pattern == %s", av[i]);
#endif

		patc++;
		if (glob(av[i], 0, NULL, &gl) == GLOB_NOSPACE)
			error(SH_ERR, FMT1S, dgn[ERR_NOMEM]);
		switch (gl.gl_pathc) {
		case 0:
			/*
			 * Discard this unmatched pattern.
			 */
			free(av[i]);
			globfree(&gl);
			for (j = i, k = i + 1; av[k] != NULL; j++, k++)
				av[j] = av[k];
			av[j] = NULL;
			i--;
			continue;
		case 1:
			/*
			 * There is a single match for this pattern.
			 * Try to replace it w/ the resulting file name.
			 */
			free(av[i]);
			av[i] = xstrdup(gl.gl_pathv[0]);
			break;
		default:
			/*
			 * There are multiple matches for this pattern.
			 * Try to replace it w/ the resulting file names.
			 */
			for (j = i; av[j] != NULL; j++)
				;	/* nothing */
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				globfree(&gl);
				error(SH_ERR, FMT1S, dgn[ERR_TMARGS]);
			}
			for ( ; j > i; j--)
				av[j + gl.gl_pathc - 1] = av[j];
			free(av[i]);
			for (j = i + gl.gl_pathc, k = 0; i < j; i++, k++)
				av[i] = xstrdup(gl.gl_pathv[k]);
			i--;
		}
		patm++;
		globfree(&gl);
	} while (av[++i] != NULL);

#ifdef	DEBUG
	if (patc > 0)
		fdprint(FD2, "glob_av: Matched == %u, Unmatched == %u",
		    patm, (patc - patm));
#endif

	/* Print the `No match' diagnostic if needed. */
	if (patc > 0 && patm == 0)
		error(SH_ERR, FMT1S, dgn[ERR_NOMATCH]);
}

/*
 * Check if the argument arg contains unquoted glob character(s);
 * return 1 if true or 0 if false.
 */
static int
gchar(const char *arg)
{
	const char *a;

	for (a = arg; *(a = quote(a)) != '\0'; a++)
		switch (*a) {
		case '*':
		case '?':
		case '[':
			return 1;
		}
	return 0;
}

/*
 * Prepare the glob pattern arg to be used by glob(3).
 * Remove any outer "double" or 'single' quotes from arg;
 * escape any glob or quote characters contained within.
 * Allocate memory for, make a copy of, and return a
 * pointer to the new arg.  Never returns on error.
 */
static char *
gtrim(char *arg)
{
	size_t siz;
	char buf[PATHMAX], c;
	char *a, *b;

	for (a = arg, b = buf; b < &buf[PATHMAX]; a++, b++) {
		switch (*a) {
		case '\0':
			*b = '\0';
			siz = (b - buf) + 1;
			arg = xmalloc(siz);
			memcpy(arg, buf, siz);
			return arg;
		case '\"':
		case '\'':
			c = *a++;
			while (*a != c && b < &buf[PATHMAX]) {
				switch (*a) {
				case '\0':
					goto gtrimerr;
				case '*': case '?': case '[': case ']':
				case '-': case '\"': case '\'': case '\\':
					*b = '\\';
					if (++b >= &buf[PATHMAX])
						goto gtrimerr;
					break;
				}
				*b++ = *a++;
			}
			b--;
			continue;
		case '\\':
			switch (*++a) {
			case '\0':
				a--, b--;
				continue;
			case '*': case '?': case '[': case ']':
			case '-': case '\"': case '\'': case '\\':
				*b = '\\';
				if (++b >= &buf[PATHMAX])
					goto gtrimerr;
				break;
			}
			break;
		}
		*b = *a;
	}

gtrimerr:
	error(SH_ERR, "%s %s", dgn[ERR_TRIM], arg);
	/* NOTREACHED */
	return NULL;
}

/*
 * Remove any unquoted quote characters from the argument arg.
 * Never returns on error.
 *
 * NOTE: The resulting argument should always be the same length
 *	 as or shorter than the original argument and is limited
 *	 to a maximum length of LINEMAX - 1 characters.  Also, it
 *	 is expected that the original argument is either quoted
 *	 correctly (see quote()) or is not quoted at all.
 */
static void
trim(char *arg)
{
	size_t siz;
	char buf[LINEMAX], c;
	char *a, *b;

	for (a = arg, b = buf; b < &buf[LINEMAX]; a++, b++) {
		switch (*a) {
		case '\0':
			*b = '\0';
			siz = (b - buf) + 1;
			memcpy(arg, buf, siz);
			return;
		case '\"':
		case '\'':
			c = *a++;
			while (*a != c && b < &buf[LINEMAX]) {
				if (*a == '\0')
					goto trimerr;
				*b++ = *a++;
			}
			b--;
			continue;
		case '\\':
			if (*++a == '\0') {
				a--, b--;
				continue;
			}
			break;
		}
		*b = *a;
	}

trimerr:
	error(SH_ERR, "%s %s", dgn[ERR_TRIM], arg);
}

#ifndef	SH6
/*
 * Change the shell's working directory.
 * Return  0 on success.
 * Return -1 on error.
 *
 *	`chdir'		changes to user's home directory
 *	`chdir -'	changes to previous working directory
 *	`chdir dir'	changes to `dir'
 *
 * NOTE: The user must have both read and search permission on
 *	 a directory in order for `chdir -' to be effective.
 */
static int
do_chdir(char **av)
{
	int eid, nwd;
	static int owd = -1;
	const char *home;

	eid = ERR_BADDIR;

	nwd = open(".", O_RDONLY | O_NONBLOCK, 0);

	if (av[1] == NULL) {
		/*
		 * Change to the user's home directory.
		 */
		home = getenv("HOME");
		if (home == NULL || *home == '\0') {
			eid = ERR_BADHOME;
			goto chdirerr;
		}
		if (chdir(home) == -1)
			goto chdirerr;
	} else if (EQUAL(av[1], "-")) {
		/*
		 * Change to the previous working directory.
		 */
		if (owd == -1) {
			eid = ERR_BADOLD;
			goto chdirerr;
		}
		if (!fdtype(owd, FD_DIROK) || fchdir(owd) == -1) {
			if (close(owd) != -1)
				owd = -1;
			goto chdirerr;
		}
	} else {
		/*
		 * Change to any other directory.
		 */
		trim(av[1]);
		if (chdir(av[1]) == -1)
			goto chdirerr;
	}

	/* success - clean up if needed and return */
	if (nwd != -1) {
		if ((owd = dup2(nwd, OWD)) == OWD)
			fcntl(owd, F_SETFD, FD_CLOEXEC);
		close(nwd);
	}
	status = 0;
	return 0;

chdirerr:
	if (nwd != -1)
		close(nwd);
	return error(-1, FMT2S, av[0], dgn[eid]);
}

/*
 * Ignore (or unignore) the specified signals, or print a list
 * of the signals currently ignored because of `sigign'.
 * Return  0 on success.
 * Return -1 on error.
 */
static int
do_sig(char **av)
{
	struct sigaction act, oact;
	sigset_t new_mask, old_mask;
	long lsigno;
	static int ignlst[NSIG], gotlst;
	int err, i, signo;
	char *sigbad;

	/* Temporarily block all signals in this function. */
	sigfillset(&new_mask);
	sigprocmask(SIG_SETMASK, &new_mask, &old_mask);

	if (!gotlst) {
		/* Initialize the list of already ignored signals. */
		for (i = 1; i < NSIG; i++) {
			ignlst[i - 1] = 0;
			if (sigaction(i, NULL, &oact) < 0)
				continue;
			if (oact.sa_handler == SIG_IGN)
				ignlst[i - 1] = 1;
		}
		gotlst = 1;
	}

	err = 0;
	if (av[1] != NULL) {
		if (av[2] == NULL) {
			err = 1;
			goto sig_done;
		}

		memset(&act, 0, sizeof(act));
		if (EQUAL(av[1], "+"))
			act.sa_handler = SIG_IGN;
		else if (EQUAL(av[1], "-"))
			act.sa_handler = SIG_DFL;
		else {
			err = 1;
			goto sig_done;
		}
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;

		for (i = 2; av[i] != NULL; i++) {
			errno = 0;
			lsigno = strtol(av[i], &sigbad, 10);
			if (errno != 0 || *av[i] == '\0' || *sigbad != '\0' ||
			    lsigno <= 0 || lsigno >= NSIG) {
				err = i;
				goto sig_done;
			}
			signo = lsigno;

			/* Does anything need to be done? */
			if (sigaction(signo, NULL, &oact) < 0 ||
			    (act.sa_handler == SIG_DFL &&
			    oact.sa_handler == SIG_DFL))
				continue;

			if (ignlst[signo - 1]) {
				sigflags(act.sa_handler, signo);
				continue;
			}

			/*
			 * Trying to ignore SIGKILL, SIGSTOP, and/or SIGCHLD
			 * has no effect.
			 */
			if (signo == SIGKILL ||
			    signo == SIGSTOP || signo == SIGCHLD ||
			    sigaction(signo, &act, NULL) < 0)
				continue;

			sigflags(act.sa_handler, signo);
		}
	} else {
		/* Print signals currently ignored because of `sigign'. */
		for (i = 1; i < NSIG; i++) {
			if (sigaction(i, NULL, &oact) < 0 ||
			    oact.sa_handler != SIG_IGN)
				continue;
			if (!ignlst[i - 1] ||
			    (i == SIGINT && sigs & SS_SIGINT) ||
			    (i == SIGQUIT && sigs & SS_SIGQUIT) ||
			    (i == SIGTERM && sigs & SS_SIGTERM))
				fdprint(FD1, "%s + %u", av[0], i);
		}
	}

sig_done:
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	if (err == 0) {
		status = 0;
		return 0;
	}
	if (err > 1)
		return error(-1, FMT3S, av[0], av[err], dgn[ERR_BADSIGNAL]);
	return error(-1, FMT2S, av[0], dgn[ERR_SIGIGN]);
}

/*
 * Read and execute commands from the file av[1] and return.
 * Calls to this function can be nested to the point imposed by
 * any limits in the shell's environment, such as running out of
 * file descriptors or hitting a limit on the size of the stack.
 * Return  0 on success.
 * Return -1 on error.
 */
static int
do_source(char **av)
{
	char *const *sppav;
	int nfd, sfd, sppac;
	static int cnt;

	if ((nfd = open(av[1], O_RDONLY | O_NONBLOCK, 0)) == -1)
		return error(-1, FMT3S, av[0], av[1], dgn[ERR_OPEN]);
	if (!fdtype(nfd, S_IFREG)) {
		close(nfd);
		return error(-1, FMT3S, av[0], av[1], dgn[ERR_EXEC]);
	}
	sfd = dup2(FD0, SAVFD0 + cnt);
	if (sfd == -1 || dup2(nfd, FD0) == -1 ||
	    fcntl(FD0, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1) {
		close(sfd);
		close(nfd);
		return error(-1, FMT3S, av[0], av[1], strerror(EMFILE));
	}
	fcntl(sfd, F_SETFD, FD_CLOEXEC);
	close(nfd);

	if (!(shtype & SOURCE))
		shtype |= SOURCE;

	sppav = ppav;
	sppac = ppac;
	if (av[2] != NULL) {
		ppav = &av[1];
		for (ppac = 3; av[ppac] != NULL; ppac++)
			;	/* nothing */
		ppac -= 1;
	}

	cnt++;
	cmd_loop(1);
	cnt--;

	if (av[2] != NULL) {
		ppav = sppav;
		ppac = sppac;
	}

	if (errflg) {
		/*
		 * The shell has detected an error (e.g., syntax error).
		 * Terminate any and all source commands (nested or not).
		 * Restore original standard input before returning for
		 * the final time, and call error() if needed.
		 */
		if (cnt == 0) {
			/* Restore original standard input or die trying. */
			if (dup2(SAVFD0, FD0) == -1)
				error(SH_ERR,FMT3S,av[0],av[1],strerror(errno));
			close(SAVFD0);
			shtype &= ~SOURCE;
			if (!(shtype & RCFILE))
				error(0, NULL);
			return -1;
		}
		close(sfd);
		return -1;
	}

	/* Restore previous standard input. */
	if (dup2(sfd, FD0) == -1) {
		close(sfd);
		return error(-1, FMT3S, av[0], av[1], strerror(errno));
	}
	close(sfd);

	if (cnt == 0)
		shtype &= ~SOURCE;
	return 0;
}

static void
sigflags(void (*act)(int), int s)
{

	if (act == SIG_IGN) {
		if (s == SIGINT)
			sigs |= SS_SIGINT;
		else if (s == SIGQUIT)
			sigs |= SS_SIGQUIT;
		else if (s == SIGTERM)
			sigs |= SS_SIGTERM;
	} else if (act == SIG_DFL) {
		if (s == SIGINT)
			sigs &= ~SS_SIGINT;
		else if (s == SIGQUIT)
			sigs &= ~SS_SIGQUIT;
		else if (s == SIGTERM)
			sigs &= ~SS_SIGTERM;
	}
}
#endif	/* !SH6 */
