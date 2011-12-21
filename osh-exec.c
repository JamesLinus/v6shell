/*
 * osh-exec.c - command execution routines for osh and sh6
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
 *	@(#)osh-exec.c	1.3 (jneitzel) 2004/12/28
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
 = "\100(#)osh-exec.c	1.3 (jneitzel) 2004/12/28";
#endif	/* !lint */

#include "osh.h"

#ifndef	SH6
/*
 * (NSIG - 1) is simply a default for the maximum number of signals
 * supported by sigign if it is not defined elsewhere.
 */
# ifndef	NSIG
#  define	NSIG		16
# endif

/* signal state flags */
# define	SI_SIGINT	01
# define	SI_SIGQUIT	02
# define	SI_SIGTERM	04

static int	sigs;		/* signal state for SIGINT, SIGQUIT, SIGTERM */
#endif	/* !SH6 */

/* special built-in commands */
static const char *const sbi[] = {
#define	SBINULL		0
	":",
#define	SBICHDIR	1
	"chdir",
#define	SBIEXIT		2
	"exit",
#define	SBILOGIN	3
	"login",
#define	SBISET		4
	"set",
#define	SBISHIFT	5
	"shift",
#define	SBIWAIT		6
	"wait",
	NULL
};
#ifndef	SH6
static const char *const sbiext[] = {
#define	SBISIGIGN	7
	"sigign",
#define	SBISETENV	8
	"setenv",
#define	SBIUNSETENV	9
	"unsetenv",
#define	SBIUMASK	10
	"umask",
#define	SBISOURCE	11
	"source",
#define	SBIEXEC		12
	"exec",
	NULL
};
#endif	/* !SH6 */

#ifndef	SH6
static void	do_chdir(char **);
static void	do_sig(char **);
static void	do_source(char **);
static void	sigflags(void (*)(int), int);
#endif	/* !SH6 */
static void	close_fd(int *);
static void	forkexec(char **, int *, int);
static int	gchar(char *);
static void	glob_av(char **);
static char	*gtrim(char *);
static int	*redirect(char **, int *, int);
static void	trim(char *);
static int	which(const char *);

/*
 * Do all of the final command preparations.
 * Then, execute the special built-in command, or call forkexec()
 * if the command is external.
 */
void
execute(char **av, int *redir, int flags)
{
	int cmd, i;
#ifndef	SH6
	mode_t m;
	char *p;
#endif	/* !SH6 */

	/*
	 * Determine which argument is the command name.
	 */
	for (i = 0; av[i] != NULL; i++)
		if (*av[i] == '<') {
			if (*(av[i] + 1) == '\0')
				i++;
#ifdef	DEBUG
			if (av[i] == NULL)
				error(SH_DEBUG, "execute: Bad `<'");
#endif
		} else if (*av[i] == '>') {
			if (*(av[i] + 1) == '\0' ||
			    (*(av[i] + 1) == '>' && *(av[i] + 2) == '\0'))
				i++;
#ifdef	DEBUG
			if (av[i] == NULL)
				error(SH_DEBUG, "execute: Bad `>' or `>>'");
#endif
		} else
			break;
	cmd = which(av[i]);

	/*
	 * Try to setup I/O redirection.
	 */
#ifndef	SH6
	if (redirect(av, redir, (cmd == -1 || cmd == SBIEXEC)?1:0) == NULL) {
#else
	if (redirect(av, redir, (cmd == -1)?1:0) == NULL) {
#endif	/* SH6 */
		close_fd(redir);
		apstat = 1;
		return;
	}

	/*
	 * Do preparations required for most special built-in commands.
	 */
#ifndef	SH6
	if (cmd != -1 && cmd != SBIEXEC) {
#else
	if (cmd != -1) {
#endif	/* SH6 */
		if (redir[0] != -1 || redir[1] != -1)
			apstat = 1;
		close_fd(redir);
		flags &= ~(FL_ASYNC | FL_PIPE);
#ifndef	SH6
		if (cmd != SBICHDIR)
#endif	/* !SH6 */
			for (i = 1; av[i] != NULL; i++)
				trim(av[i]);
	}

	/*
	 * Execute the requested command.
	 */
	switch (cmd) {
	case SBINULL:
		/*
		 * This is the "do-nothing" command.
		 */
		status = 0;
		return;

	case SBICHDIR:
		/*
		 * Change the shell's working directory.
		 */
#ifndef	SH6
		if (!clone) {
			do_chdir(av);
			return;
		}
#endif	/* !SH6 */
		if (av[1] == NULL) {
			error(0, "%s: arg count", av[0]);
			return;
		}
		if (chdir(av[1]) == -1)
			error(0, "%s: bad directory", av[0]);
		else
			status = 0;
		return;

	case SBIEXIT:
		/*
		 * Cause the shell to cease execution of a file of commands.
		 */
		if (!PROMPT(shtype)) {
			seek_eof();
			if (shtype & (COMMANDFILE|RCFILE))
				DOEXIT(status);
		}
		return;

	case SBILOGIN:
		if (PROMPT(shtype))
			execv(_PATH_LOGIN, av);
		error(0, "%s: cannot execute", av[0]);
		return;

#ifndef	SH6
	case SBISET:
		/*
		 * Set (or print) the current compatibility mode of the shell.
		 *
		 * usage: set [clone | noclone]
		 */
		if (av[1] != NULL) {
			if (EQUAL(av[1], "clone"))
				clone = 1;
			else if (EQUAL(av[1], "noclone"))
				clone = 0;
			else {
				error(0, "%s: bad mode", av[0]);
				return;
			}
		} else
			fdprint(FD1, clone ? "clone" : "noclone");
		status = 0;
		return;
#endif	/* !SH6 */

	case SBISHIFT:
		/*
		 * Shift all positional parameters to the left by 1
		 * with the exception of $0 which remains constant.
		 */
		if (posac <= 1) {
			error(0, "%s: no args", av[0]);
			return;
		}
		posav = &posav[1];
		posac--;
		status = 0;
		return;

	case SBIWAIT:
		apwait(0);
		return;

#ifndef	SH6
	case SBISIGIGN:
		/*
		 * Ignore (or unignore) the specified signals or
		 * print a list of the currently ignored signals.
		 *
		 * usage: sigign [[+ | -] signal_number ...]
		 */
		do_sig(av);
		return;

	case SBISETENV:
		if (av[1] != NULL) {
			if (*av[1] == '\0') {
				error(0, NULL);
				return;
			}
			p = NULL;
			if (av[2] != NULL)
				p = av[2];
			if (setenv(av[1], (p != NULL) ? p : "", 1) == -1)
				error(SH_FATAL, "Out of memory");

			/* Adjust $h or $p if needed. */
			if (EQUAL(av[1], "HOME")) {
				if ((p = getenv("HOME")) != NULL &&
				    *p != '\0' && strlen(p) < PATHMAX)
					si.home = p;
				else
					si.home = NULL;
			} else if (EQUAL(av[1], "PATH"))
				si.path = getenv("PATH");
			status = 0;
			return;
		}
		error(0, "%s: arg count", av[0]);
		return;

	case SBIUNSETENV:
		if (av[1] != NULL && *av[1] != '\0') {
			unsetenv(av[1]);

			/* Unset $h or $p if needed. */
			if (EQUAL(av[1], "HOME"))
				si.home = NULL;
			else if (EQUAL(av[1], "PATH"))
				si.path = NULL;
			status = 0;
			return;
		}
		error(0, "%s: arg count", av[0]);
		return;

	case SBIUMASK:
		if (av[1] != NULL) {
			for (m = 0, p = av[1]; *p >= '0' && *p <= '7'; p++)
				m = m * 8 + (*p - '0');
			if (*p != '\0' || m > 0777) {
				error(0, "%s: bad mask", av[0]);
				return;
			}
			umask(m);
		} else {
			umask(m = umask(0));
			fdprint(FD1, "%04o", m);
		}
		status = 0;
		return;

	case SBISOURCE:
		/*
		 * Read and execute commands from file and return.
		 *
		 * usage: source file
		 */
		if (av[1] == NULL || av[2] != NULL) {
			error(0, "%s: arg count", av[0]);
			return;
		}
		do_source(av);
		return;

	case SBIEXEC:
		if (av[1] == NULL) {
			error(0, "%s: arg count", av[0]);
			close_fd(redir);
			return;
		}
		flags |= FL_EXEC;
		av = &av[1];
		break;
#endif	/* !SH6 */

	default:
		/*
		 * The command is external.
		 */
		break;
	}
	forkexec(av, redir, flags);
}

/*
 * Fork and execute external command, manipulate file descriptors
 * for pipes and I/O redirection, and keep track of each process
 * to allow for proper termination reporting.
 */
static void
forkexec(char **av, int *redir, int flags)
{
	pid_t cpid;
	int cstat, i;
	static unsigned as_sub;
	char *p;

#ifndef	SH6
	cpid = (flags & FL_EXEC) ? 0 : fork();
#else
	cpid = fork();
#endif	/* SH6 */
	switch (cpid) {
	case -1:
		/* User has too many concurrent processes. */

		/* Flag end of the list. */
		splist[spc] = -1;

		/* Stop further command line execution; see pxpipe(). */
		spc = -1;

		/* Exorcise the zombies now. */
		for (i = 0; splist[i] != -1; i++)
			waitpid(splist[i], NULL, 0);
		error(0, "Cannot fork; try again");
		close_fd(redir);
		return;
	case 0:
		break;
	default:
		/**** Parent! ****/
		close_fd(redir);

		/* Track processes for proper termination reporting. */
		if (flags & FL_ASYNC) {
			aplist[apc] = cpid;
			if (++apc > aplim - 1) {
				aplist = xrealloc(aplist,
					    (aplim + PLSIZE) * sizeof(pid_t));
				aplim += PLSIZE;
			}
			aplist[apc] = 0;
		} else {
			splist[spc] = cpid;
			if (++spc > splim - 1) {
				splist = xrealloc(splist,
					    (splim + PLSIZE) * sizeof(pid_t));
				splim += PLSIZE;
			}
			splist[spc] = 0;
		}

		if (flags & FL_PIPE)
			return;

		if (flags & FL_ASYNC) {
			fdprint(FD2, "%u", cpid);
			return;
		}

		/* Wait for and deal w/ terminated child processes. */
		for (i = 0; i < spc; i++) {
			waitpid(splist[i], &cstat, 0);
			if (cstat) {
				if (WIFSIGNALED(cstat))
					sigmsg(cstat, (i==spc-1)?-1:splist[i]);
				else if (WIFEXITED(cstat))
					status = WEXITSTATUS(cstat);

				/* special cases, special attention */
				if (status == 0215)
					continue;
				if (status == 0202 && (shtype & INTERACTIVE))
					write(FD2, "\n", (size_t)1);
				if (status >= 0176 && !PROMPT(shtype)) {
#ifndef	SH6
					if ((shtype & RCFILE) &&
					    !(shtype & DOSOURCE) &&
					    status != 0202 && status != 0203)
						continue;
#endif	/* !SH6 */
					errflg = 1;
					seek_eof();
					if (shtype & COMMANDFILE)
						DOEXIT(status);
				}
			} else
				status = 0;
		}
		spc = 0; /* reset process count */
		return;
	}

	/**** Child! ****/
	/* pipes and I/O redirection */
	for (i = 0; i < 2; i++)
		if (redir[i] != -1) {
			if (dup2(redir[i], i) == -1)
				error(SH_FATAL,
				    "Cannot redirect descriptor %d", i);
			close(redir[i]);
		}

	/*
	 * Close the unused pipe descriptor (if any) as it is only
	 * needed in the parent.
	 */
	if (redir[2] != -1)
		close(redir[2]);

	/*
	 * Set the action for the SIGINT, SIGQUIT, and SIGTERM signals.
	 * Also, redirect input for `&' commands from `/dev/null' if needed.
	 */
	if (flags & FL_ASYNC) {
		if (shtype & COMMANDFILE) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}
		if (redir[0] == -1) {
			close(FD0);
			if (open("/dev/null", O_RDWR, 0) != FD0)
				error(SH_ERR, "%s: cannot open", "/dev/null");
		}
	} else if (as_sub == 0) {
#ifndef	SH6
		if (!(sigs & SI_SIGINT) && (dointr & DO_SIGINT))
			signal(SIGINT, SIG_DFL);
		if (!(sigs & SI_SIGQUIT) && (dointr & DO_SIGQUIT))
			signal(SIGQUIT, SIG_DFL);
	}
	if (!(sigs & SI_SIGTERM))
		signal(SIGTERM, SIG_DFL);
#else
		if (dointr & DO_SIGINT)
			signal(SIGINT, SIG_DFL);
		if (dointr & DO_SIGQUIT)
			signal(SIGQUIT, SIG_DFL);
	}
	signal(SIGTERM, SIG_DFL);
#endif	/* SH6 */

	if (*av[0] == '(') {
		if (flags & FL_ASYNC)
			as_sub = 1;
		*av[0] = ' ';
		p = av[0] + strlen(av[0]);
		while (*--p != ')')
			;	/* nothing */
		*p = '\0';
		pxline(av[0], PX_EXEC);
		_exit(status);
	}
	glob_av(av);

	/*
	 * The Sixth Edition Unix shell always used the equivalent
	 * of `.:/bin:/usr/bin' to search for commands, not PATH.
	 */
	execvp(av[0], av);
	if (errno == ENOENT || errno == ENOTDIR)
		error(127, "%s: not found", av[0]);
	error(126, "%s: cannot execute", av[0]);
}

/*
 * Determine whether or not cmd is a special built-in.
 * If so, return its index value.  Otherwise, return -1
 * as cmd is either external or not a command at all.
 */
static int
which(const char *cmd)
{
	int i;
#ifndef	SH6
	int j;
#endif	/* !SH6 */

#ifdef	DEBUG
	if (cmd == NULL)
		error(SH_DEBUG, "which: Internal error, cmd == NULL");
#endif

	for (i = 0; sbi[i] != NULL; i++)
		if (EQUAL(cmd, sbi[i]))
			return(i);
#ifndef	SH6
	if (!clone)
		for (j = 0; sbiext[j] != NULL; j++)
			if (EQUAL(cmd, sbiext[j]))
				return(i + j);
#endif	/* !SH6 */
	return(-1);
}

/*
 * Close file descriptors associated w/ pipes and/or redirects.
 */
static void
close_fd(int *redir)
{

	if (redir[0] != -1)
		close(redir[0]);
	if (redir[1] != -1)
		close(redir[1]);
}

/*
 * Redirect and/or strip away redirection arguments (if any).
 * If act == 1, do a normal file open and argument strip.
 * If act == 0, only do an argument strip.
 * Return file descriptors on success.
 * Return NULL on error.
 */
static int *
redirect(char **av, int *redir, int act)
{
	int fd;
	unsigned append, i, j;
	char *file, *p;

	for (append = i = 0; av[i] != NULL; i++) {
		switch (*av[i]) {
		case '<':
			p = av[i] + 1;
			av[i] = (char *)-1;

			/* Get file name... */
			if (*p == '\0') {
				file = av[++i];
				av[i] = (char *)-1;
			} else {
				file = p;
#ifndef	SH6
				/* Redirect from the original standard input. */
				if (*file == '-' && *(file + 1) == '\0' &&
				    !clone && act == 1 && redir[0] == -1) {
					if ((fd = dup(dupfd0)) == -1) {
						error(0, "<-: cannot redirect");
						return(NULL);
					} else
						redir[0] = fd;
				}
#endif	/* !SH6 */
			}
			trim(file);

			/* Attempt to open file; ignore if piped or `<-'. */
			if (act == 1 && redir[0] == -1) {
				if ((fd = open(file, O_RDONLY, 0)) == -1) {
					error(0, "%s: cannot open", file);
					return(NULL);
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

			/* Attempt to open file; ignore if piped. */
			if (act == 1 && redir[1] == -1) {
				if (append)
					fd = open(file,
						O_WRONLY | O_APPEND | O_CREAT,
						0666);
				else
					fd = open(file,
						O_WRONLY | O_TRUNC | O_CREAT,
						0666);
				if (fd == -1) {
					error(0, "%s: cannot create", file);
					return(NULL);
				} else
					redir[1] = fd;
			}
			break;
		}
	}

	/* Strip away the redirection arguments. */
	for (i = j = 0; av[i] != NULL; ) {
		while (av[i] == (char *)-1)
			i++;
		if (av[i] != NULL)
			av[j++] = av[i++];
	}
	av[j] = NULL;
	return(redir);
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
	unsigned expand, i, j, k, match;

	for (expand = i = match = 0; av[i] != NULL; i++) {

		if (gchar(av[i]) == 0) {
			trim(av[i]);
			continue;
		}
		av[i] = gtrim(av[i]);

		expand++;
		if (glob(av[i], 0, NULL, &gl) == GLOB_NOSPACE)
			error(SH_FATAL, "Out of memory");
		switch (gl.gl_pathc) {
		case 0:
			globfree(&gl);
#ifndef	SH6
			if (!clone) {
				/* Trim and use the argument as is. */
				trim(av[i]);
				continue;
			}
#endif	/* !SH6 */
			/* Discard the unmatched argument. */
			for (j = i, k = i + 1; av[k] != NULL; )
				av[j++] = av[k++];
			av[j] = NULL;
			i--;
			continue;
		case 1:
			av[i] = xrealloc(av[i], strlen(gl.gl_pathv[0]) + 1);
			strcpy(av[i], gl.gl_pathv[0]);
			break;
		default:
			for (j = i; av[j] != NULL; j++)
				;	/* nothing */
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				globfree(&gl);
				error(SH_ERR, "Arg list too long");
			}
			for ( ; j > i; j--)
				av[j + gl.gl_pathc - 1] = av[j];
			free(av[i]);
			for (j = i + gl.gl_pathc, k = 0; i < j; i++, k++) {
				av[i] = xmalloc(strlen(gl.gl_pathv[k]) + 1);
				strcpy(av[i], gl.gl_pathv[k]);
			}
			i--;
		}
		match++;
		globfree(&gl);
	}

	/* Print the `No match' diagnostic if needed. */
#ifndef	SH6
	if (clone && expand > 0 && match == 0)
#else
	if (expand > 0 && match == 0)
#endif	/* SH6 */
		error(SH_ERR, "No match");
}

/*
 * Check if the argument arg contains unquoted glob character(s);
 * return 1 if true or 0 if false.
 */
static int
gchar(char *arg)
{
	char *a;

	for (a = arg; ; a++) {
		a = quote(a);
		switch (*a) {
		case '\0':
			return(0);
		case '*':
		case '?':
		case '[':
			return(1);
		}
	}
	/* NOTREACHED */
}

/*
 * Prepare the glob pattern arg to be used by glob(3).
 * Remove any outer "double" or 'single' quotes from arg;
 * escape any glob or quote characters contained within.
 * Allocate memory for, make a copy of, and return a
 * pointer to the new arg.  Never returns on error.
 *
 * NOTE: The resulting pattern is always the same length as,
 *	 or longer than, the original.
 */
static char *
gtrim(char *arg)
{
	char buf[LINESIZE], ch;
	char *a, *b, *bufmax;

	bufmax = buf + LINESIZE;
	for (a = arg, b = buf; b < bufmax; a++, b++) {
		switch (*a) {
		case '\0':
			*b = '\0';
			arg = xmalloc((size_t)(b - buf) + 1);
			strcpy(arg, buf);
#ifdef	DEBUG
			fprintf(stderr, "Pattern: %s\n", arg);
#endif
			return(arg);
		case '\"':
		case '\'':
			ch = *a++;
			while (*a != ch && b < bufmax) {
				switch (*a) {
				case '\0':
					goto gtrimerr;
				case '*':
				case '?':
				case '[':
				case ']':
				case '-':
				case '\"':
				case '\'':
				case '\\':
					*b = '\\';
					if (++b >= bufmax)
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
			case '*':
			case '?':
			case '[':
			case ']':
			case '-':
			case '\"':
			case '\'':
			case '\\':
				*b = '\\';
				if (++b >= bufmax)
					goto gtrimerr;
				break;
			}
			/* FALLTHROUGH */
		default:
			*b = *a;
		}
	}

gtrimerr:
	error(SH_FATAL, "Pattern too long");
	/* NOTREACHED */
	return(NULL);
}

/*
 * Remove any unquoted quote characters from the argument arg.
 * Never returns on error.
 *
 * NOTE: The resulting argument is always the same length as,
 *	 or shorter than, the original.
 */
static void
trim(char *arg)
{
	char buf[LINESIZE], ch;
	char *a, *b, *bufmax;

#ifdef	DEBUG
	if (arg == NULL)
		error(SH_DEBUG, "trim: Internal error, arg == NULL");
#endif

	bufmax = buf + LINESIZE;
	for (a = arg, b = buf; b < bufmax; a++, b++) {
		switch (*a) {
		case '\0':
			*b = '\0';
			strcpy(arg, buf);
			return;
		case '\"':
		case '\'':
			ch = *a++;
			while (*a != ch && b < bufmax) {
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
			/* FALLTHROUGH */
		default:
			*b = *a;
		}
	}

trimerr:
	/*
	 * Generally, this code is (or should be) unreachable.
	 */
	error(SH_FATAL, "Arg too long");
}

#ifndef	SH6
/*
 * Change the shell's working directory.
 *
 *	`chdir'		changes to user's home directory
 *	`chdir -'	changes to previous working directory
 *	`chdir dir'	changes to `dir'
 *
 * NOTE: The user must have both read and search permission on
 *	 a directory in order for `chdir -' to be effective.
 */
static void
do_chdir(char **av)
{
	struct stat sb;
	int nwd;
	static int owd = -1;
	const char *emsg;

	emsg = "%s: bad directory"; /* default error message */

	nwd = open(".", O_RDONLY | O_NONBLOCK, 0);

	if (av[1] == NULL) {
		/*
		 * Change to the user's home directory.
		 */
		if (si.home == NULL) {
			emsg = "%s: no home directory";
			goto chdirerr;
		}
		if (chdir(si.home) == -1)
			goto chdirerr;
	} else if (EQUAL(av[1], "-")) {
		/*
		 * Change to the previous working directory.
		 */
		if (owd == -1) {
			emsg = "%s: no old directory";
			goto chdirerr;
		}
		if (fstat(owd, &sb) == 0 && sb.st_nlink > 0) {
#ifdef	DEBUG
			fprintf(stderr, "OWD OK: "
			    "Device == %u, Inode == %u, Link count == %u\n",
			    sb.st_dev, sb.st_ino, sb.st_nlink);
#endif
			if (fchdir(owd) == -1) {
				if (close(owd) != -1)
					owd = -1;
				goto chdirerr;
			}
		} else {
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
	return;

chdirerr:
	error(0, emsg, av[0]);
	if (nwd != -1)
		close(nwd);
}

/*
 * Ignore (or unignore) the specified signals.
 * Otherwise, print a list of the currently ignored signals.
 */
static void
do_sig(char **av)
{
	void (*dosigact)(int);
	struct sigaction act, oact;
	sigset_t new_mask, old_mask;
	static int ign_list[NSIG], list;
	int err, i, signo;
	char *sigbad;

	/* Temporarily block all signals in this function. */
	sigfillset(&new_mask);
	sigprocmask(SIG_SETMASK, &new_mask, &old_mask);

	if (!list) {
		/* Initialize the list of already ignored signals. */
		for (i = 1; i < NSIG; i++) {
			if (sigaction(i, NULL, &oact) < 0)
				continue;
			if (oact.sa_handler == SIG_IGN)
				ign_list[i - 1] = i;
		}
		list = 1;
	}

	if (av[1] != NULL) {
		err = 0;
		if (av[2] == NULL) {
			err = 1;
			goto sigerr;
		}

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (EQUAL(av[1], "+"))
			dosigact = SIG_IGN;
		else if (EQUAL(av[1], "-"))
			dosigact = SIG_DFL;
		else {
			err = 1;
			goto sigerr;
		}

		for (i = 2; av[i] != NULL; i++) {
			errno = 0;
			signo = strtol(av[i], &sigbad, 10);
			if (errno != 0 || !*av[i] || *sigbad ||
			    signo <= 0 || signo >= NSIG) {
				err = i;
				goto sigerr;
			}

			/* Does anything need to be done? */
			if (sigaction(signo, NULL, &oact) < 0 ||
			    (dosigact == SIG_DFL && oact.sa_handler == SIG_DFL))
				continue;

			if (ign_list[signo - 1] == signo) {
				sigflags(dosigact, signo);
				continue;
			}

			/*
			 * Trying to ignore SIGKILL, SIGSTOP, and/or SIGCHLD
			 * has no effect.
			 */
			act.sa_handler = dosigact;
			if (signo == SIGKILL ||
			    signo == SIGSTOP || signo == SIGCHLD ||
			    sigaction(signo, &act, &oact) < 0)
				continue;

			sigflags(dosigact, signo);
		}
	} else {
		/* Print signals currently ignored by the shell. */
		for (i = 1; i < NSIG; i++) {
			if (sigaction(i, NULL, &oact) < 0 ||
			    oact.sa_handler != SIG_IGN)
				continue;
			if (ign_list[i - 1] == 0 ||
			    (i == SIGINT && (sigs & SI_SIGINT)) ||
			    (i == SIGQUIT && (sigs & SI_SIGQUIT)) ||
			    (i == SIGTERM && (sigs & SI_SIGTERM)))
				fdprint(FD1, "sigign + %d", i);
		}
	}
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	status = 0;
	return;

sigerr:
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	if (err > 1)
		error(0, "%s: bad signal %s", av[0], av[err]);
	else
		error(0, "%s: error", av[0]);
}

/*
 * Read and execute commands from file and return.
 * Calls to this function can be nested to the point imposed by
 * any limits in the shell's environment, such as running out of
 * file descriptors or hitting a limit on the size of the stack.
 */
static void
do_source(char **av)
{
	int nfd, sfd;
	static int count;

	if ((nfd = open(av[1], O_RDONLY, 0)) == -1) {
		error(0, "%s: %s: cannot open", av[0], av[1]);
		return;
	}
	sfd = dup2(FD0, SAVFD0 + count);
	if (sfd == -1 || dup2(nfd, FD0) == -1) {
		error(0, "%s: %s: %s", av[0], av[1], strerror(EMFILE));
		if (sfd != -1)
			close(sfd);
		close(nfd);
		return;
	}
	fcntl(sfd, F_SETFD, FD_CLOEXEC);
	close(nfd);

	if (!(shtype & DOSOURCE))
		shtype |= DOSOURCE;

	count++;
	cmd_loop(1);
	count--;

	if (errflg) {
		/*
		 * The shell has detected an error (e.g., syntax error).
		 * Terminate any and all source commands (nested or not).
		 * Restore original standard input, etc...
		 */
		if (count == 0) {
			/* Restore original standard input or die trying. */
			if (dup2(SAVFD0, FD0) != FD0)
				error(SH_FATAL, "%s: %s: %s", av[0], av[1],
				    strerror(errno));
			if (!(shtype & RCFILE))
				error(0, NULL);
			close(SAVFD0);
			shtype &= ~DOSOURCE;
			return;
		}
		close(sfd);
		return;
	}

	/* Restore previous standard input. */
	if (dup2(sfd, FD0) != FD0) {
		error(0, "%s: %s: %s", av[0], av[1], strerror(errno));
		close(sfd);
		return;
	}
	close(sfd);

	if (count == 0)
		shtype &= ~DOSOURCE;
}

static void
sigflags(void (*act)(int), int s)
{
	if (act == SIG_IGN) {
		if (s == SIGINT)
			sigs |= SI_SIGINT;
		else if (s == SIGQUIT)
			sigs |= SI_SIGQUIT;
		else if (s == SIGTERM)
			sigs |= SI_SIGTERM;
		return;
	}
	if (s == SIGINT)
		sigs &= ~SI_SIGINT;
	else if (s == SIGQUIT)
		sigs &= ~SI_SIGQUIT;
	else if (s == SIGTERM)
		sigs &= ~SI_SIGTERM;
}
#endif	/* !SH6 */
