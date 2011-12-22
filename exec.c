/*
 * osh:
 *	an enhanced, backward-compatible reimplementation of the
 *	Sixth Edition (V6) Unix shell.
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
 = "@(#)osh-041018: exec.c	1.1 (jneitzel) 2004/10/16";
#endif	/* !lint */

#include "osh.h"

#ifndef	SH6
/*
 * (NSIG - 1) is simply a default for the maximum number of signals
 * supported by trap if it is not defined elsewhere.
 */
# ifndef	NSIG
#  define	NSIG		16
# endif

/* trap flags */
# define	TR_SIGINT	01
# define	TR_SIGQUIT	02
# define	TR_SIGTERM	04
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
#define	SBITRAP		7
	"trap",
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
static int	traps;		/* trap state for SIGINT, SIGQUIT, SIGTERM */
#endif	/* !SH6 */

#ifndef	SH6
static void	do_chdir(char *);
static void	do_source(char *);
static void	do_trap(char **);
static void	trapflags(void (*)(int), int);
#endif	/* !SH6 */
static void	close_fd(int *);
static void	forkexec(char **, int *, int);
static void	glob_args(char **);
static int	glob_char(char *);
static int	*redirect(char **, int *, int);
static void	sigmsg(int, pid_t);
static void	strip_arg(char *);
static int	which(const char *);

/*
 * Wait, fetch, and report termination status for asynchronous
 * child processes and exorcise any stray zombies.
 */
void
apwait(int opt)
{
	pid_t apid;
	int i, cstat;

	while ((apid = waitpid(-1, &cstat, opt)) > 0) {
		/* Check if apid is a known asynchronous process. */
		for (i = 0; i < apc; ++i) {
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
						lseek(STDIN_FILENO, (off_t)0,
						    SEEK_END);
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
 * Execute special built-in command, or call forkexec()
 * if command is external.
 */
void
execute(char **args, int *redir, int flags)
{
	int cmd, i;
#ifndef	SH6
	int t;
	mode_t m;
	char *p;
#endif	/* !SH6 */

	/* Determine which argument is the command name. */
	for (i = 0; args[i] != NULL; ++i)
		if (*args[i] == '<') {
			if (*(args[i] + 1) == '\0')
				++i;
		} else if (*args[i] == '>') {
			if (*(args[i] + 1) == '\0' ||
			    (*(args[i] + 1) == '>' && *(args[i] + 2) == '\0'))
				++i;
		} else
			break;
	cmd = which(args[i]);

#ifndef	SH6
	if (redirect(args, redir, (cmd == -1 || cmd == SBIEXEC)?1:0) == NULL) {
#else
	if (redirect(args, redir, (cmd == -1)?1:0) == NULL) {
#endif	/* !SH6 */
		close_fd(redir);
		apstat = 1;
		return;
	}

	/* Do preparations required for most special built-in commands. */
#ifndef	SH6
	if (cmd != -1 && cmd != SBIEXEC) {
#else
	if (cmd != -1) {
#endif	/* !SH6 */
		if (redir[0] != -1 || redir[1] != -1)
			apstat = 1;
		close_fd(redir);
		flags &= ~(FL_ASYNC | FL_PIPE);
		if (cmd != SBICHDIR)
			for (i = 1; args[i] != NULL; ++i)
				strip_arg(args[i]);
	}

	/* Execute the requested command. */
	switch (cmd) {
	case SBINULL:
		/* the "do-nothing" command */
		status = 0;
		return;
	case SBICHDIR:
#ifndef	SH6
		if (!clone) {
			do_chdir(args[1]);
			return;
		}
#endif	/* !SH6 */
		if (args[1] == NULL) {
			error(0, "%s: arg count", args[0]);
			return;
		}
		strip_arg(args[i]);
		if (chdir(args[1]) == -1)
			error(0, "%s: bad directory", args[0]);
		else
			status = 0;
		return;
	case SBIEXIT:
		/* Terminate a shell which is reading commands from a file. */
		if (!PROMPT(shtype)) {
			if (lseek(STDIN_FILENO, (off_t)0, SEEK_END) == -1 &&
			    errno == ESPIPE)
				while (getchar() != EOF)
					;	/* nothing */
			if (shtype & (COMMANDFILE|RCFILE))
				DOEXIT(status);
		}
		return;
	case SBILOGIN:
		if (PROMPT(shtype))
			execv(_PATH_LOGIN, args);
		error(0, "%s: cannot execute", args[0]);
		return;
#ifndef	SH6
	case SBISET:
		/* Set (or print) the current compatibility mode of the shell.
		 * usage: set [clone | noclone]
		 */
		if (args[1] != NULL) {
			if (EQUAL(args[1], "clone"))
				clone = 1;
			else if (EQUAL(args[1], "noclone"))
				clone = 0;
			else {
				error(0, "%s: bad mode", args[0]);
				return;
			}
		} else
			fputs(clone ? "clone\n" : "noclone\n", stdout);
		status = 0;
		return;
#endif	/* !SH6 */
	case SBISHIFT:
		/* Shift all positional parameters to the left by 1
		 * with the exception of $0 which remains constant.
		 */
		if (posac <= 1) {
			error(0, "%s: no args", args[0]);
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
	/* The remaining commands correspond to those in sbiext[]. */
	case SBITRAP:
		/* Trap (i.e., ignore) or untrap the specified signals
		 * or print the signals currently trapped by the shell.
		 * usage: trap [[+ | -] signal_number ...]
		 */
		do_trap(args);
		return;
	case SBISETENV:
		if (args[1] != NULL) {
			if (*args[1] == '\0') {
				error(0, NULL);
				return;
			}
			p = NULL;
			if (args[2] != NULL)
				p = args[2];
			if (setenv(args[1], (p != NULL) ? p : "", 1) == -1) {
				error(SH_FATAL, "Out of memory");
				return;
			}

			/* Adjust $h or $p if needed. */
			if (EQUAL(args[1], "HOME")) {
				if ((p = getenv("HOME")) != NULL &&
				    *p != '\0' && strlen(p) < PATHMAX)
					si.home = p;
				else
					si.home = NULL;
			} else if (EQUAL(args[1], "PATH"))
				si.path = getenv("PATH");
			status = 0;
			return;
		}
		error(0, "%s: arg count", args[0]);
		return;
	case SBIUNSETENV:
		if (args[1] != NULL && *args[1] != '\0') {
			unsetenv(args[1]);

			/* Unset $h or $p if needed. */
			if (EQUAL(args[1], "HOME"))
				si.home = NULL;
			else if (EQUAL(args[1], "PATH"))
				si.path = NULL;
			status = 0;
			return;
		}
		error(0, "%s: arg count", args[0]);
		return;
	case SBIUMASK:
		if (args[1] != NULL) {
			for (m = 0, p = args[1]; *p >= '0' && *p <= '7'; ++p)
				m = m * 8 + (*p - '0');
			if (*p != '\0' || m > 0777) {
				error(0, "%s: bad mask", args[0]);
				return;
			}
			umask(m);
		} else {
			umask(m = umask(0));
			printf("%04o\n", m);
		}
		status = 0;
		return;
	case SBISOURCE:
		/* Read and execute commands from file and return.
		 * usage: source file
		 */
		if (args[1] == NULL || args[2] != NULL) {
			error(0, "%s: arg count", args[0]);
			return;
		}
		t = 0;
		if (!(shtype & DOSOURCE)) {
			t = 1;
			shtype |= DOSOURCE;
		}
		do_source(args[1]);
		if (t)
			shtype &= ~DOSOURCE;
		return;
	case SBIEXEC:
		if (args[1] == NULL) {
			error(0, "%s: arg count", args[0]);
			close_fd(redir);
			return;
		}
		flags |= FL_EXEC;
		args = &args[1];
		break;
#endif	/* !SH6 */

	default:
		/* Command is external. */
		break;
	}
	forkexec(args, redir, flags);
}

/*
 * Fork and execute external command, manipulate file descriptors
 * for pipes and I/O redirection, and keep track of each process
 * to allow for proper termination reporting.
 */
static void
forkexec(char **args, int *redir, int flags)
{
	pid_t cpid;
	int cstat, i;
	static unsigned as_sub;
	char as_pid[32];
	char *p;

#ifndef	SH6
	switch (cpid = (flags & FL_EXEC) ? 0 : fork()) {
#else
	switch (cpid = fork()) {
#endif	/* !SH6 */
	case -1:
		/* User has too many concurrent processes. */

		/* Flag end of the list. */
		splist[spc] = -1;

		/* Stop further command line execution; see pxpipe(). */
		spc = -1;

		/* Exorcise the zombies now. */
		for (i = 0; splist[i] != -1; ++i)
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
			snprintf(as_pid, sizeof(as_pid), "%u\n", cpid);
			write(STDERR_FILENO, as_pid, strlen(as_pid));
			return;
		}

		/* Wait for and deal w/ terminated child processes. */
		for (i = 0; i < spc; ++i) {
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
					write(STDERR_FILENO, "\n", (size_t)1);
				if (status >= 0176 && !PROMPT(shtype)) {
#ifndef	SH6
					if ((shtype & RCFILE) &&
					    !(shtype & DOSOURCE) &&
					    status != 0202 && status != 0203)
						continue;
#endif	/* !SH6 */
					errflg = 1;
					lseek(STDIN_FILENO, (off_t)0, SEEK_END);
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

	/* Close the unused pipe descriptor (if any) as it is only
	 * needed in the parent.
	 */
	if (redir[2] != -1)
		close(redir[2]);

	if (flags & FL_ASYNC) {
		if (shtype & COMMANDFILE) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}
		if (redir[0] == -1) {
			close(STDIN_FILENO);
			if (open("/dev/null", O_RDWR, 0) != STDIN_FILENO)
				error(SH_ERR, "/dev/null: cannot open");
		}
	} else if (as_sub == 0) {
#ifndef	SH6
		if (!(traps & TR_SIGINT) && (dointr & DO_SIGINT))
			signal(SIGINT, SIG_DFL);
		if (!(traps & TR_SIGQUIT) && (dointr & DO_SIGQUIT))
			signal(SIGQUIT, SIG_DFL);
#else
		if (dointr & DO_SIGINT)
			signal(SIGINT, SIG_DFL);
		if (dointr & DO_SIGQUIT)
			signal(SIGQUIT, SIG_DFL);
#endif	/* !SH6 */
	}
#ifndef	SH6
	if (!(traps & TR_SIGTERM))
#endif	/* !SH6 */
		signal(SIGTERM, SIG_DFL);

	if (*args[0] == '(') {
		if ((flags & FL_ASYNC) && as_sub == 0)
			as_sub = 1;
		*args[0] = ' ';
		p = args[0] + strlen(args[0]);
		while (*--p != ')')
			;	/* nothing */
		*p = '\0';
		pxline(args[0], PX_EXEC);
		if (as_sub)
			as_sub = 0;
		_exit(status);
	}
	glob_args(args);

	/* The Sixth Edition Unix shell always used the equivalent
	 * of `.:/bin:/usr/bin' to search for commands, not PATH.
	 */
	execvp(args[0], args);
	if (errno == ENOENT || errno == ENOTDIR)
		error(127, "%s: not found", args[0]);
	error(126, "%s: cannot execute", args[0]);
}

#ifndef	SH6
/*
 * Change shell's working directory.
 *
 *	`chdir'		changes to user's home directory
 *	`chdir -'	changes to previous working directory
 *	`chdir dir'	changes to `dir'
 *
 * XXX: The user must have both read and search permission
 *	on a directory in order for `chdir -' to be effective.
 */
static void
do_chdir(char *dir)
{
	int nwd, tfd;
	static int owd = -1;
	const char *emsg;

	emsg = "chdir: bad directory"; /* default error message */

	nwd = open(".", O_RDONLY | O_NONBLOCK, 0);

	if (dir == NULL) {
		/* Change to the user's home directory. */
		if (si.home == NULL) {
			emsg = "chdir: no home directory";
			goto chdirerr;
		}
		if (chdir(si.home) == -1)
			goto chdirerr;
	} else if (EQUAL(dir, "-")) {
		/* Change to the previous working directory. */
		if (owd == -1) {
			emsg = "chdir: no old directory";
			goto chdirerr;
		}
		if (fchdir(owd) == -1) {
			if (close(owd) != -1)
				owd = -1;
			goto chdirerr;
		}
		if ((tfd = open(".", O_RDONLY | O_NONBLOCK, 0)) == -1) {
			if (close(owd) != -1)
				owd = -1;
			fchdir(nwd);
			goto chdirerr;
		} else {
			if (fchdir(tfd) == -1) {
				close(tfd);
				fchdir(nwd);
				goto chdirerr;
			}
			close(tfd);
		}
	} else {
		/* Change to any other directory. */
		strip_arg(dir);
		if (chdir(dir) == -1)
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
	error(0, emsg);
	if (nwd != -1)
		close(nwd);
}

/*
 * Read and execute commands from file and return.
 * Calls to this function can be nested to the point imposed by
 * any limits in the shell's environment, such as running out of
 * file descriptors or hitting a limit on the size of the stack.
 */
static void
do_source(char *file)
{
	int nfd, sfd;
	static int count;
	char line[LINESIZE];

	if ((nfd = open(file, O_RDONLY, 0)) == -1) {
		error(0, "source: %s: cannot open", file);
		return;
	}
	sfd = dup2(STDIN_FILENO, SAVFD0 + count);
	if (sfd == -1 || dup2(nfd, STDIN_FILENO) == -1) {
		error(0, "source: %s: %s", file, strerror(EMFILE));
		if (sfd != -1)
			close(sfd);
		close(nfd);
		return;
	}
	fcntl(sfd, F_SETFD, FD_CLOEXEC);
	close(nfd);

	count++;
	cmd_loop(line, 1);
	count--;

	if (errflg) {
		/* The shell has detected an error (e.g., syntax error).
		 * Terminate any and all source commands (nested or not).
		 * Restore original standard input, etc...
		 */
		if (count == 0) {
			/* Restore original standard input or die trying. */
			if (dup2(SAVFD0, STDIN_FILENO) != STDIN_FILENO)
				error(SH_FATAL, "source: %s: %s", file,
				    strerror(errno));
			if (!(shtype & RCFILE))
				error(0, NULL);
			close(SAVFD0);
			return;
		}
		close(sfd);
		return;
	}

	/* Restore previous standard input. */
	if (dup2(sfd, STDIN_FILENO) != STDIN_FILENO) {
		error(0, "source: %s: %s", file, strerror(errno));
		close(sfd);
		return;
	}
	close(sfd);
}

/*
 * Trap (i.e., ignore) or untrap the requested signals.
 * Otherwise, print the signals currently trapped by the shell.
 */
static void
do_trap(char **args)
{
	void (*dosigact)(int);
	struct sigaction act, oact;
	sigset_t new_mask, old_mask;
	static int list[NSIG], fl_list;
	int err, i, signo;
	char *sigbad;

	/* Temporarily block all signals in this function. */
	sigfillset(&new_mask);
	sigprocmask(SIG_SETMASK, &new_mask, &old_mask);

	/* Initialize list of already ignored, but not trapped, signals. */
	if (!fl_list) {
		for (i = 1; i < NSIG; ++i) {
			if (sigaction(i, NULL, &oact) < 0)
				continue;
			if (oact.sa_handler == SIG_IGN)
				list[i - 1] = i;
		}
		fl_list = 1;
	}

	if (args[1] != NULL) {
		err = 0;
		if (args[2] == NULL) {
			err = 1;
			goto traperr;
		}

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (EQUAL(args[1], "+"))
			dosigact = SIG_IGN;
		else if (EQUAL(args[1], "-"))
			dosigact = SIG_DFL;
		else {
			err = 1;
			goto traperr;
		}

		for (i = 2; args[i] != NULL; ++i) {
			errno = 0;
			signo = strtol(args[i], &sigbad, 10);
			if (errno != 0 || !*args[i] || *sigbad ||
			    signo <= 0 || signo >= NSIG) {
				err = i;
				goto traperr;
			}

			/* Does anything need to be done? */
			if (sigaction(signo, NULL, &oact) < 0 ||
			    (dosigact == SIG_DFL && oact.sa_handler == SIG_DFL))
				continue;

			if (list[signo - 1] == signo) {
				trapflags(dosigact, signo);
				continue;
			}

			/* Trapping SIGKILL, SIGSTOP, and/or SIGCHLD
			 * has no effect.
			 */
			act.sa_handler = dosigact;
			if (signo == SIGKILL ||
			    signo == SIGSTOP || signo == SIGCHLD ||
			    sigaction(signo, &act, &oact) < 0)
				continue;

			trapflags(dosigact, signo);
		}
	} else {
		/* Print signals currently trapped by the shell. */
		for (i = 1; i < NSIG; ++i) {
			if (sigaction(i, NULL, &oact) < 0 ||
			    oact.sa_handler != SIG_IGN)
				continue;
			if (list[i - 1] == 0 ||
			    (i == SIGINT && (traps & TR_SIGINT)) ||
			    (i == SIGQUIT && (traps & TR_SIGQUIT)) ||
			    (i == SIGTERM && (traps & TR_SIGTERM)))
				printf("trap + %d\n", i);
		}
	}
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	status = 0;
	return;

traperr:
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	if (err > 1)
		error(0, "%s: bad signal %s", args[0], args[err]);
	else
		error(0, "%s: error", args[0]);
}

static void
trapflags(void (*act)(int), int s)
{
	if (act == SIG_IGN) {
		if (s == SIGINT)
			traps |= TR_SIGINT;
		else if (s == SIGQUIT)
			traps |= TR_SIGQUIT;
		else if (s == SIGTERM)
			traps |= TR_SIGTERM;
		return;
	}
	if (s == SIGINT)
		traps &= ~TR_SIGINT;
	else if (s == SIGQUIT)
		traps &= ~TR_SIGQUIT;
	else if (s == SIGTERM)
		traps &= ~TR_SIGTERM;
}
#endif	/* !SH6 */

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

	for (i = 0; sbi[i] != NULL; ++i)
		if (EQUAL(cmd, sbi[i]))
			return(i);
#ifndef	SH6
	if (!clone) {
		for (j = i, i = 0; sbiext[i] != NULL; ++i)
			if (EQUAL(cmd, sbiext[i]))
				return(i + j);
	}
#endif	/* !SH6 */
	return(-1);
}

/*
 * Redirect and/or strip away redirection arguments (if any).
 * If act == 1, do a normal file open and argument strip.
 * If act == 0, only do an argument strip.
 * Return file descriptors on success.
 * Return NULL on failure.
 */
static int *
redirect(char **args, int *redir, int act)
{
	int fd;
	unsigned append, i, j;
	char *file, *p;

	for (append = i = 0; args[i] != NULL; ++i) {
		switch (*args[i]) {
		case '<':
			p = args[i] + 1;
			args[i] = (char *)-1;

			/* Get file name... */
			if (*p == '\0') {
				file = args[++i];
				args[i] = (char *)-1;
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
			strip_arg(file);

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
			if (*(args[i] + 1) == '>') {
				append = 1;
				p = args[i] + 2;
			} else
				p = args[i] + 1;
			args[i] = (char *)-1;

			/* Get file name... */
			if (*p == '\0') {
				file = args[++i];
				args[i] = (char *)-1;
			} else
				file = p;
			strip_arg(file);

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
		default:
			break;
		}
	}

	/* Strip away the redirection arguments. */
	for (i = j = 0; args[i] != NULL; ) {
		while (args[i] == (char *)-1)
			i++;
		if (args[i] != NULL)
			args[j++] = args[i++];
	}
	args[j] = NULL;
	return(redir);
}

/*
 * Expand arguments containing unquoted occurrences of any of the
 * glob characters `*', `?', or `['.  Arguments containing none of
 * the glob characters are used as is.
 * Never returns on failure.
 *
 * XXX: This function should never be called from the parent shell,
 *	only from the child process after a successful fork().
 */
static void
glob_args(char **args)
{
	glob_t gl;
	unsigned exp, i, j, k, match;
	int gcr;

	for (exp = i = match = 0; args[i] != NULL; i++) {

		if ((gcr = glob_char(args[i])) == 0) {
			strip_arg(args[i]);
			continue;
		}
		strip_arg(args[i]);
		if (gcr == 2 && strlen(args[i]) == 2)
			continue;

		exp++;
		if (glob(args[i], GLOB_NOESCAPE, NULL, &gl) == GLOB_NOSPACE)
			error(SH_FATAL, "Out of memory");
		if (gl.gl_pathc == 0) {
			globfree(&gl);
#ifndef	SH6
			if (!clone) /* Leave the argument unchanged. */
				continue;
#endif	/* !SH6 */
			/* Discard the unmatched argument. */
			for (j = i, k = i + 1; args[k] != NULL; )
				args[j++] = args[k++];
			args[j] = NULL;
			i--;
			continue;
		}
		if (gl.gl_pathc > 1) {
			for (j = i; args[j] != NULL; j++)
				;	/* nothing */
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				globfree(&gl);
				error(SH_ERR, "Too many args");
			}
			for ( ; j > i; j--)
				args[j + gl.gl_pathc - 1] = args[j];
			for (j = i + gl.gl_pathc, k = 0; i < j; i++, k++) {
				args[i] = xmalloc(strlen(gl.gl_pathv[k]) + 1);
				strcpy(args[i], gl.gl_pathv[k]);
			}
			i--;
		} else {
			args[i] = xmalloc(strlen(gl.gl_pathv[0]) + 1);
			strcpy(args[i], gl.gl_pathv[0]);
		}
		match++;
		globfree(&gl);
	}

	/* Print the `No match' diagnostic if needed. */
#ifndef	SH6
	if (clone && exp > 0 && match == 0)
#else
	if (exp > 0 && match == 0)
#endif	/* !SH6 */
		error(SH_ERR, "No match");
}

/*
 * Check if the argument contains glob character(s).
 * Return 0 if no glob characters are found.
 * Return 1 for a `*' or `?' pattern.
 * Return 2 for a `[...]' pattern.
 */
static int
glob_char(char *s)
{
	char *p, *q;

	for (p = s, q = NULL; ; p++) {
		if (*(p = quote(p)) == '\0')
			break;
		if (*p == ']' && q != NULL && p > q)
			return(2);
		if ((*p == '*' || *p == '?') && q == NULL)
			return(1);
		if (*p == '[' && q == NULL)
			q = p + 1;
	}
	return(0);
}

/*
 * Close file descriptors associated w/ pipes and/or redirects.
 */
static void
close_fd(int *redir)
{
	unsigned i;

	for (i = 0; i < 2; ++i)
		if (redir[i] != -1)
			close(redir[i]);
}

/*
 * Remove all unquoted occurrences of ", ', and \ .
 * Never returns on failure.
 *
 * XXX: Using strcpy() here should be safe as the resulting argument
 *      is always the same length as, or shorter than, the original.
 */
static void
strip_arg(char *arg)
{
	char buf[LINESIZE], ch;
	char *a, *b, *end;

	end = buf + LINESIZE - 1;
	for (a = arg, b = buf; *a != '\0'; a++, b++) {
		if (b >= end)
			error(SH_FATAL, "Internal error");
		switch (*a) {
		case '\"':
		case '\'':
			ch = *a++;
			while (*a != ch) {
				if (*a == '\0' || b >= end)
					error(SH_FATAL, "Internal error");
				*b++ = *a++;
			}
			b--;
			continue;
		case '\\':
			if (*++a == '\0')
				goto stripend;
			/* FALLTHROUGH */
		default:
			*b = *a;
		}
	}

stripend:
	*b = '\0';
	strcpy(arg, buf);
}

/*
 * Print termination message according to signal s.
 * If p != -1, precede message w/ process ID (e.g., `&' commands).
 */
static void
sigmsg(int s, pid_t p)
{
	int len, signo;
	char msgbuf[64];
	const char *msg1, *msg2;

	signo  = WTERMSIG(s);
	status = signo + 0200;

	/* First, determine which message to print if any. */
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
#ifdef	SIGIOT /* old name for the POSIX SIGABRT signal */
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
	default:	msg1 = NULL;	/* Report by signal number. */
	}

	/* Create and print the message now. */
	msg2 = (s & 0200) ? " -- Core dumped" : "";
	len = 0;
	if (p != -1)
		len = snprintf(msgbuf, sizeof(msgbuf), "%d: ", p);
	if (msg1 == NULL)
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "Sig %d%s\n", signo, msg2);
	else
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "%s%s\n", msg1, msg2);
	write(STDERR_FILENO, msgbuf, strlen(msgbuf));
}
