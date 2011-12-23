/*
 * osh - enhanced reimplementation of the Sixth Edition (V6) Unix shell
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
/*	Derived from: osh-020214/osh.c	*/
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
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
#  define	ID_ATTR	__attribute__((used))
# elif defined(__GNUC__)
#  define	ID_ATTR	__attribute__((unused))
# else
#  define	ID_ATTR
# endif
const char revision[] ID_ATTR = "@(#)osh.c	1.99 (jneitzel) 2004/07/23";
#endif	/* !lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define	_PATH_SYSTEM_RC	"/etc/osh.login"
#define	_PATH_DOT_RC	".osh.login"

#ifndef	NSIG
#define	NSIG		16	/* (NSIG - 1) is simply a default maximum... */
#endif

#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		256	/* includes space for the final `\0' */
#endif

#define	ARGMAX		256
#define	LINELEN		1024
#define	PLSIZE		16	/* # of processes for *[as]plist allocations */

#define	EQUAL(a, b)	(strcmp(a, b) == 0)	/* compare strings */

/* action values for command parsing/execution */
#define	PX_SYN		0	/* check syntax only */
#define	PX_SYNEXEC	1	/* check syntax and execute if syntax is OK */
#define	PX_EXEC		2	/* execute only */

/* command flags */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

/* trap flags */
#define	TR_SIGINT	01
#define	TR_SIGQUIT	02
#define	TR_SIGTERM	04

/* type of shell */
#define	INTERACTIVE	(shtype == 0)
#define	RCFILE		01
#define	COMMANDFILE	02
#define	OTHER		04

/* shell/user identifiers */
struct {
	pid_t	pid;		/* process ID of shell		*/
	uid_t	uid;		/* real user ID of shell/user	*/
	char	*user;		/* login (or user) name		*/
	char	*tty;		/* terminal name		*/
	char	*home;		/* home (login) directory	*/
	char	*path;		/* command search path		*/
} si;

/* special built-in commands */
#define	SBINULL		0
#define	SBICHDIR	1
#define	SBIEXIT		2
#define	SBILOGIN	3
#define	SBISET		4
#define	SBISHIFT	5
#define	SBIWAIT		6
#define	SBITRAP		7
#define	SBISETENV	8
#define	SBIUNSETENV	9
#define	SBIUMASK	10
#define	SBIEXEC		11
const char *const sbi[] = {
	":",		/* SBINULL */
	"chdir",	/* SBICHDIR */
	"exit",		/* SBIEXIT */
	"login",	/* SBILOGIN */
	"set",		/* SBISET */
	"shift",	/* SBISHIFT */
	"wait",		/* SBIWAIT */
	NULL
};
const char *const sbiext[] = {
	"trap",		/* SBITRAP */
	"setenv",	/* SBISETENV */
	"unsetenv",	/* SBIUNSETENV */
	"umask",	/* SBIUMASK */
	"exec",		/* SBIEXEC */
	NULL
};

int		apc;		/* # of known asynchronous processes */
int		aplim;		/* current limit on # of processes in aplist */
pid_t		*aplist;	/* array of known asynchronous process IDs */
signed char	apstat = -1;	/* whether or not to call apwait() */
unsigned char	clone;		/* current compatibility mode of the shell */
char		*cmdnam;	/* command name - used for $0 */
unsigned char	dointr;		/* if child processes can be interrupted */
int		oldfd0;		/* original standard input of the shell */
unsigned	posac;		/* count of positional parameters */
char		**posav;	/* positional parameters */
unsigned char	rsig;		/* if command execution received INTR or QUIT */
int		shtype;		/* type of shell */
int		spc;		/* # of processes in current pipeline */
int		splim;		/* current limit on # of processes in splist */
pid_t		*splist;	/* array of process IDs in current pipeline */
int		status;		/* exit status of the shell */
int		traps;		/* trap state for SIGINT, SIGQUIT, SIGTERM */

void	apwait(int);
void	bargs(char *, int *, int);
void	close_fd(int *);
char	*de_blank(char *);
void	dotrap(char **);
void	err(const char *);
void	execute(char **, int *, int);
void	forkexec(char **, int *, int);
char	**globargs(char **);
int	globchar(char *);
int	lookup(const char *);
char	*psynsub(char *);
int	psyntax(char *);
int	pxline(char *, int);
int	pxpipe(char *, int, int);
char	*quote(char *, int);
void	rcfile(int *);
char	*readline(char *, int, size_t);
int	*redirect(char **, int *, int);
void	shell_init(void);
void	sigmsg(int, pid_t);
void	striparg(char *);
int	stripsub(char *);
int	substparm(char *);
void	trapflags(void (*)(int), int);
void	xchdir(const char *);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);

int
main(int argc, char **argv)
{
	off_t ioff;
	int ch, input, rcflag;
	unsigned char tflag;
	char line[LINELEN];
	char *bs, *pr;

	shell_init();
	input = rcflag = tflag = 0;

	/* Setup the positional parameters. */
	cmdnam = argv[1];
	posav = &argv[1];
	posac = argc - 1;

	/* Determine how the shell was invoked and act accordingly. */
	if (argc == 1) {
		if (isatty(STDIN_FILENO) && isatty(STDERR_FILENO)) {
			if (signal(SIGINT, SIG_IGN) == SIG_DFL)
				dointr = 1;
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
			if (*argv[0] == '-')
				rcfile(&rcflag);
		} else
			shtype = OTHER;
	} else if (*argv[1] == '-') { /* non-interactive shell */
		/*
		 * This implementation behaves exactly like the Sixth Edition
		 * Unix shell WRT options.  For compatibility, any option is
		 * accepted w/o producing an error.
		 */
		shtype = OTHER;
		if (signal(SIGINT, SIG_IGN) == SIG_DFL)
			dointr = 1;
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		switch (*(argv[1] + 1)) {
		case 'c':
			/* osh -c [string]
			 * XXX: `-canything' works too (compatible).
			 */
			if (argv[2] == NULL) /* read from input w/o prompting */
				break;

			/* Resetting posav and posac here is not compatible. */
			posav += 1;
			posac -= 1;

			strncpy(line, argv[2], (size_t)LINELEN);
			if (line[LINELEN - 1] != '\0')
				err("Command line overflow");

			/* Deal w/ '\' characters at end of string. */
			pr = line + strlen(line) - 1;
			for (bs = pr; bs >= line && *bs == '\\'; --bs)
				;	/* nothing */
			if ((pr - bs) % 2 == 1)
				return(0);

			/* Parse and execute string as a command line. */
			if (substparm(line) && !pxline(line, PX_SYNEXEC))
				err("syntax error");
			return(status);
		case 't':
			/* osh -t
			 * XXX: `-tanything' works too (compatible).
			 */
			tflag = 1;
			break;
		default:
			/* XXX (undocumented):
			 * All other options have the same effect as
			 * `osh -c' (w/o command).
			 */
			break;
		}
	} else {
		/* osh file [arg1 ...] */
		shtype = COMMANDFILE;
		close(input);
		if ((input = open(argv[1], O_RDONLY, 0)) != STDIN_FILENO) {
			fprintf(stderr, "%s: cannot open\n", argv[1]);
			return(1);
		}
		/* Ignore first line in command file if it begins w/ `#!' . */
		ioff = 0;
		if (getchar() == '#' && getchar() == '!') {
			ioff += 3;
			while ((ch = getchar()) != '\n' && ch != EOF)
				++ioff;
			if (ch == EOF)
				return(1);
		}
		/* Command file must be seekable. */
		if (lseek(input, ioff, SEEK_SET) == -1) {
			fprintf(stderr, "%s: cannot seek\n", argv[1]);
			return(1);
		}
	}

	/* main command loop */
again:
	for (;;) {
		if (INTERACTIVE)
			write(STDERR_FILENO, si.uid ? "% " : "# ", (size_t)2);

		pr = readline(line, input, (size_t)LINELEN);
		if (pr == NULL)
			break;
		if (pr == (char *)-1)
			continue;

		/* While a line of input ends w/ a `\' character, read input
		 * from the next line if the number of `\' characters is odd.
		 * Each time, the last `\' is translated into a blank.
		 */
		while ((pr - 1) >= line && *(pr - 1) == '\\') {
			for (bs = pr - 2; bs >= line && *bs == '\\'; --bs)
				;	/* nothing */
			if ((pr - bs - 1) % 2 == 0)
				break;
			if (tflag)
				return(0);
			*(pr - 1) = ' ';
			pr = readline(pr, input, (size_t)LINELEN - (pr - line));
			if (pr == NULL)
				goto done;
			if (pr == (char *)-1)
				goto again;
		}

		/* Parse and execute command line. */
		if (substparm(line) && !pxline(line, PX_SYNEXEC))
			err("syntax error");
		if (tflag)
			break;
	}

done:
	if (rcflag) {
		rcfile(&rcflag);
		goto again;
	}
	return(status);
}

/*
 * shell initialization
 */
void
shell_init(void)
{
	struct stat fdsb;
	struct passwd *pw;
	int fd;
	char *str;

	/* Revoke set-id privileges; get shell/user ID and process ID. */
	setgid(getgid());
	setuid(si.uid = getuid());
	si.pid = getpid();

	/* Ensure that stdin, stdout, and stderr are not closed. */
	for (fd = 0; fd < 3; ++fd)
		if (fstat(fd, &fdsb) == -1) {
			close(fd);
			if (open("/dev/null", O_RDWR, 0) != fd) {
				fputs("/dev/null: cannot open\n", stderr);
				exit(1);
			}
		}

	if ((oldfd0 = dup(STDIN_FILENO)) == -1) {
		fputs("Cannot dup\n", stderr);
		exit(1);
	}
	fcntl(oldfd0, F_SETFD, FD_CLOEXEC);

	/* Allocate memory for aplist and splist; reallocate in forkexec(). */
	aplim = splim = PLSIZE;
	aplist = xmalloc(aplim * sizeof(pid_t));
	splist = xmalloc(splim * sizeof(pid_t));

	/* Check if `OSH_COMPAT' exists in the shell's environment.
	 * If so and value is legal, set compatibility mode accordingly.
	 * Otherwise, remove it from the environment.
	 */
	if ((str = getenv("OSH_COMPAT")) != NULL) {
		if (EQUAL(str, "clone"))
			clone = 1;
		else if (EQUAL(str, "noclone"))
			clone = 0;
		else
			unsetenv("OSH_COMPAT");
	}

	/* Get login (or user) name for $u. */
	pw = NULL;
	if ((str = getlogin()) != NULL)
		if ((pw = getpwnam(str)) != NULL)
			if (pw->pw_uid != si.uid)
				pw = NULL;
	if (pw == NULL)
		pw = getpwuid(si.uid);
	if (pw != NULL) {
		si.user = xmalloc(strlen(pw->pw_name) + 1);
		strcpy(si.user, pw->pw_name);
	} else
		si.user = NULL;

	/* Get terminal name for $t. */
	if ((str = ttyname(STDIN_FILENO)) != NULL) {
		si.tty = xmalloc(strlen(str) + 1);
		strcpy(si.tty, str);
	} else
		si.tty = NULL;

	/* Get home (login) directory for `chdir' and $h. */
	if ((str = getenv("HOME")) != NULL && *str != '\0' &&
	    strlen(str) < PATHMAX)
		si.home = str;
	else
		si.home = NULL;

	si.path = getenv("PATH");
}

/*
 * Try to open each rc file (in the specified order).
 * If successful, temporarily assign the shell's standard input
 * to come from said file.  Restore the original standard input
 * when finished.
 *
 * XXX: Notice that absolutely no restrictions are placed on the
 *	ownership/permissions of the rc files.  This is for the
 *	sake of simplicity, but it would be trivial to add some
 *	restrictions...  However, I prefer to assume users are
 *	sane enough to protect themselves from malicious users.
 */
void
rcfile(int *flag)
{
	int nfd;
	char dot_rc[PATHMAX];
	const char *file;

	/* Try each rc file. */
	while (*flag < 2) {
		if (!*flag)
			file = _PATH_SYSTEM_RC;
		else {
			dot_rc[0] = '\0';
			if (si.home != NULL)
				if (snprintf(dot_rc, sizeof(dot_rc), "%s/%s",
				    si.home, _PATH_DOT_RC) >= PATHMAX)
					dot_rc[0] = '\0';
			if (dot_rc[0] == '\0')
				break;
			file = dot_rc;
		}

		*flag += 1;
		if ((nfd = open(file, O_RDONLY, 0)) == -1)
			continue;
		/* The rc file must be seekable. */
		if (lseek(nfd, (off_t)0, SEEK_CUR) == -1 ||
		    dup2(nfd, STDIN_FILENO) != STDIN_FILENO) {
			close(nfd);
			continue;
		}
		close(nfd);
		shtype |= RCFILE;
		return;
	}
	shtype &= ~RCFILE;
	*flag = 0;

	/* Restore the original standard input. */
	if (dup2(oldfd0, STDIN_FILENO) != STDIN_FILENO) {
		fputs("Cannot dup2\n", stderr);
		exit(1);
	}
}

/*
 * Read a line from fd, with error checking.
 * If read(2) returns -1, terminate the shell w/ non-zero status.
 * If command line overflow occurs in a non-interactive shell,
 * terminate w/ non-zero status.  Otherwise, return -1.
 * Return end-of-string on success.
 * Return NULL at EOF.
 */
char *
readline(char *b, int fd, size_t len)
{
	unsigned char eot;
	char *end, *p;

	end = b + len;
	for (eot = 0, p = b; p < end; p++) {
		switch (read(fd, p, (size_t)1)) {
		case -1:
			exit(1);
		case 0:
			/* If EOF occurs at the beginning of line, return NULL
			 * immediately.  Otherwise, ignore EOF until it occurs
			 * 3 times in sequence and then return NULL.
			 */
			if (p > b && ++eot < 3) {
				p--;
				continue;
			}
			if (INTERACTIVE)
				write(STDERR_FILENO, "\n", (size_t)1);
			return(NULL);
		}
		eot = 0;
		if (*p == '\n') {
			*p = '\0';
			return(p);
		}
	}

	/* Flush all unread characters from the input queue if
	 * command line overflow occurs and input is from a tty.
	 */
	if (isatty(fd))
		tcflush(fd, TCIFLUSH);
	err("Command line overflow");
	return((char *)-1);
}

/*
 * Substitute unquoted occurrences of $...
 * Return 0 on failure (e.g., syntax error or too many characters).
 * Return 1 on success.
 */
int
substparm(char *s)
{
	unsigned argno;
	unsigned char copy;
	char dbuf[LINELEN], tbuf[32];
	char *d, *eos, *p, *t;

	if (*(s = de_blank(s)) == '\0')
		return(1);

	eos = dbuf + LINELEN - 1;
	for (copy = 0, d = dbuf, p = s; *p != '\0'; d++, p++) {

		/* Skip quoted characters while ignoring `( ... )'. */
		if ((t = quote(p, 0)) == NULL) {
			err("syntax error");
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
			if (isdigit(*++p)) {
				tbuf[0] = *p;
				tbuf[1] = '\0';
				argno = atoi(tbuf);
				if (argno < posac)
					t = argno ? posav[argno] : cmdnam;
			} else if (*p == '$') {
				snprintf(tbuf, sizeof(tbuf), "%05u", si.pid);
				t = tbuf;
			} else {
				if (!clone && *p == 'u')
					t = si.user;
				else if (!clone && *p == 't')
					t = si.tty;
				else if (!clone && *p == 'h')
					t = si.home;
				else if (!clone && *p == 'p')
					t = si.path;
				else if (!clone && *p == 'n') {
					snprintf(tbuf, sizeof(tbuf), "%u",
					    posac ? posac - 1 : 0);
					t = tbuf;
				} else if (!clone && *p == 's') {
					snprintf(tbuf, sizeof(tbuf), "%d",
					    status);
					t = tbuf;
				} else
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
		strncpy(s, dbuf, (size_t)LINELEN - 1);
		s[LINELEN - 1] = '\0';
	}
	return(1);

substerr:
	err("Too many characters");
	return(0);
}

/*----------------------------------------------------------------------*\
 | Parse - Command parsing stage					|
 | The pxline() and pxpipe() functions do double-duty in both the	|
 | parsing and execution stages of osh.					|
\*----------------------------------------------------------------------*/

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
	unsigned char doex, done;
	char cl_sav[LINELEN];
	char *p, *lpip;

	if (*(cl = de_blank(cl)) == '\0')
		return(1);

	if (act == PX_SYNEXEC) {
		/* First, save a copy of the command line. */
		strncpy(cl_sav, cl, (size_t)LINELEN - 1);
		cl_sav[LINELEN - 1] = '\0';
	}

	for (doex = done = rsig = spc = 0, p = cl, lpip = cl; ; ++p) {
		if (rsig || (p = quote(p, 1)) == NULL)
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
}

/*
 * Parse pipeline for proper syntax if act == PX_SYN; otherwise,
 * break it down into individual commands, connected by pipes if
 * needed, and pass each command to bargs() for execution.
 * Return 1 on success.
 * Return 0 on failure.
 */
int
pxpipe(char *pl, int flags, int act)
{
	int pfd[2] = { -1, -1 };
	int redir[2] = { -1, -1 };
	unsigned char piped;
	char *lcmd, *p;
 
	for (piped = 0, p = pl, lcmd = pl; ; ++p) {
		if (act == PX_EXEC && spc == -1) {
			/* too many concurrent processes */
			if (pfd[0] != -1)
				close(pfd[0]);
			return(0);
		}
		p = quote(p, 1);
		switch (*p) {
		case '|':
		case '^':
			*p = '\0';
			lcmd = de_blank(lcmd);
			if (act == PX_SYN) {
				if (*lcmd == '\0')
					return(0); /* unexpected `|' or `^' */
				if (psyntax(lcmd) == 0)
					return(0);
				if (stripsub(lcmd) == 0)
					return(0);
				piped = 1;
			} else {
				redir[0] = pfd[0];
				if (pipe(pfd) == -1) {
					err("Cannot pipe");
					apstat = 1;	/* force zombie check */
					if (pfd[0] != -1)
						close(pfd[0]);
					return(0);
				}
				fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
				redir[1] = pfd[1];
				bargs(lcmd, redir, flags | FL_PIPE);
			}
			lcmd = p + 1;
			break;
		case '\0':
			lcmd = de_blank(lcmd);
			if (act == PX_SYN) {
				if (piped && *lcmd == '\0')
					return(0); /* unexpected `\0' */
				if (psyntax(lcmd) == 0)
					return(0);
				if (stripsub(lcmd) == 0)
					return(0);
			} else {
				redir[0] = pfd[0];
				redir[1] = -1;
				bargs(lcmd, redir, flags);
			}
			return(1);
		}
	}
}

/*
 * Strip away the outermost `... (' and `) ...' from command and
 * call pxline() to parse the command line contained within.
 * This function has no effect on simple commands.
 * Return 1 on success.
 * Return 0 on failure.
 */
int
stripsub(char *cmd)
{
	char *a, *b;

	/* Locate the first unquoted `(' if any. */
	for (a = cmd; ; ++a) {
		a = quote(a, 0);
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
int
psyntax(char *s)
{
	unsigned char r_in, r_out, sub, word, wtmp;
	char *p;

	for (r_in = r_out = sub = word = wtmp = 0, p = s; ; ++p) {
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			if ((p = quote(p, 0)) == NULL)
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

			if ((p = psynsub(p)) == NULL)
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
}

/*
 * Parse subshell command for proper syntax.
 * Return pointer to the character following `( ... )' or NULL
 * if a syntax error has been detected.
 */
char *
psynsub(char *s)
{
	unsigned count;
	unsigned char empty;
	char *p, *q;

	for (count = empty = 1, p = s, ++p; ; ++p) {
		p = de_blank(p);
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			if ((p = quote(p, 0)) == NULL)
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

/*----------------------------------------------------------------------*\
 | Execute - Command execution stage					|
\*----------------------------------------------------------------------*/

/*
 * Build argument vector for command.
 * Call execute() if successful; otherwise,
 * fail with a 'Too many args' error.
 */
void
bargs(char *cmd, int *redir, int flags)
{
	unsigned i;
	unsigned char done;
	char *args[ARGMAX];
	char *p, *larg;

	for (done = i = 0, p = cmd, larg = cmd; ; p++) {
		p = quote(p, 1);
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
				err("Too many args");
				return;
			}
			args[i++] = larg;
		}
		if (done) {
			if (i == 0) return; /* This should never happen. */
			args[i] = NULL;
			execute(args, redir, flags);
			return;
		}
		larg = p + 1;
	}
}

/*
 * Determine whether or not command name is a special built-in.
 * If so, return its index value.  Return -1 for external commands.
 */
int
lookup(const char *cmd)
{
	int i, j;

	for (i = 0; sbi[i] != NULL; ++i)
		if (EQUAL(cmd, sbi[i]))
			return(i);
	if (!clone) {
		for (j = i, i = 0; sbiext[i] != NULL; ++i)
			if (EQUAL(cmd, sbiext[i]))
				return(i + j);
	}
	return(-1);
}

/*
 * Execute special built-in command, or call forkexec()
 * if command is external.
 */
void
execute(char **args, int *redir, int flags)
{
	mode_t m;
	int cmd, i;
	char *p;

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
	cmd = lookup(args[i]);

	if (redirect(args, redir, (cmd == -1 || cmd == SBIEXEC)?1:0) == NULL) {
		err(NULL);
		apstat = 1;	/* force zombie check */
		close_fd(redir);
		return;
	}

	/* Special built-in commands cannot be backgrounded or piped.
	 * Close the appropriate file descriptor(s), unset the appropriate
	 * flags, and continue as normal.
	 */
	if (cmd != -1 && cmd != SBIEXEC) {
		if (redir[0] != -1 || redir[1] != -1)
			apstat = 1;	/* force zombie check */
		close_fd(redir);
		flags &= ~(FL_ASYNC | FL_PIPE);
		for (i = 1; args[i] != NULL; ++i)
			striparg(args[i]);
	}

	/* Execute the requested command. */
	switch (cmd) {
	case SBINULL:
		/* the "do-nothing" command */
		status = 0;
		return;
	case SBICHDIR:
		if (!clone) {
			xchdir(args[1]);
			return;
		}
		if (args[1] == NULL) {
			err("chdir: arg count");
			return;
		}
		if (chdir(args[1]) == -1)
			err("chdir: bad directory");
		else
			status = 0;
		return;
	case SBIEXIT:
		/* Terminate a shell which is reading commands from a file. */
		if (!INTERACTIVE) {
			lseek(STDIN_FILENO, (off_t)0, SEEK_END);
			if (shtype & (RCFILE | COMMANDFILE))
				exit(status);
		}
		return;
	case SBILOGIN:
		if (INTERACTIVE)
			execvp(args[0], args);
		err("login: cannot execute");
		return;
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
				err("set: bad mode");
				return;
			}
		} else
			fputs(clone ? "clone\n" : "noclone\n", stdout);
		status = 0;
		return;
	case SBISHIFT:
		/* Shift all positional parameters to the left by 1
		 * with the exception of $0 which remains constant.
		 */
		if (posac <= 1) {
			err("shift: no args");
			return;
		}
		posav = &posav[1];
		posac--;
		status = 0;
		return;
	case SBIWAIT:
		apwait(0);
		return;

	/* The remaining commands correspond to those in sbiext[]. */
	case SBITRAP:
		/* Trap (i.e., ignore) or untrap the specified signals
		 * or print the signals currently trapped by the shell.
		 * usage: trap [[+ | -] signal_number ...]
		 */
		dotrap(args);
		return;
	case SBISETENV:
		if (args[1] != NULL) {
			if (*args[1] == '\0') {
				err(NULL);
				return;
			}
			p = NULL;
			if (args[2] != NULL)
				p = args[2];
			if (setenv(args[1], (p != NULL) ? p : "", 1) == -1) {
				err("Out of memory");
				return;
			}

			/* Repoint HOME/PATH to the new values if needed. */
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
		err("setenv: arg count");
		return;
	case SBIUNSETENV:
		if (args[1] != NULL && *args[1] != '\0') {
			unsetenv(args[1]);

			/* Unset the internal representation of HOME/PATH. */
			if (EQUAL(args[1], "HOME"))
				si.home = NULL;
			else if (EQUAL(args[1], "PATH"))
				si.path = NULL;
			status = 0;
			return;
		}
		err("unsetenv: arg count");
		return;
	case SBIUMASK:
		if (args[1] != NULL) {
			for (m = 0, p = args[1]; *p >= '0' && *p <= '7'; ++p)
				m = m * 8 + (*p - '0');
			if (*p != '\0' || m > 0777) {
				err("umask: bad mask");
				return;
			}
			umask(m);
		} else {
			umask(m = umask(0));
			printf("%04o\n", m);
		}
		status = 0;
		return;
	case SBIEXEC:
		if (args[1] == NULL) {
			err("exec: arg count");
			close_fd(redir);
			return;
		}
		flags |= FL_EXEC;
		args = &args[1];
		break;
	default:
		/* Command is external. */
		break;
	}
	forkexec(args, redir, flags);
}

/*
 * Fork and execute external commands, manipulate file descriptors
 * for pipes and I/O redirection, and keep track of each process
 * to allow for proper termination reporting.
 */
void
forkexec(char **args, int *redir, int flags)
{
	pid_t cpid;
	int i, cstat;
	static unsigned char as_sub;
	char as_pid[32];
	char *p;

	if (flags & FL_EXEC)
		cpid = 0;
	else
		cpid = fork();
	switch (cpid) {
	case -1:
		/* User has too many concurrent processes. */

		/* Flag end of the list. */
		splist[spc] = -1;

		/* Stop further command line execution; see pxpipe(). */
		spc = -1;

		/* Exorcise the zombies now. */
		for (i = 0; splist[i] != -1; ++i)
			waitpid(splist[i], NULL, 0);
		err("Cannot fork");
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

		/* Wait and fetch termination status. */
		for (i = 0; i < spc; ++i) {
			waitpid(splist[i], &cstat, 0);
			if (cstat) {
				if (WIFSIGNALED(cstat)) {
					sigmsg(cstat, (i==spc-1)?-1:splist[i]);
					if (status == 0215)
						continue;
				} else if (WIFEXITED(cstat))
					status = WEXITSTATUS(cstat);

				/* Special-case shell terminations. */
				if (status >= 0176 && !INTERACTIVE) {
					if ((shtype & RCFILE) && !rsig)
						continue;
					lseek(STDIN_FILENO, (off_t)0, SEEK_END);
					if (shtype & COMMANDFILE)
						exit(status);
				}

			} else
				status = 0;
		}

		spc = 0; /* reset process count */
		return;
	}

	/**** Child! ****/
	/* redirections */
	for (i = 0; i < 2; i++)
		if (redir[i] != -1) {
			if (dup2(redir[i], i) == -1) {
				fprintf(stderr, "Cannot redirect %d\n", i);
				_exit(01);
			}
			close(redir[i]);
		}

	if (flags & FL_ASYNC) {
		if (shtype & COMMANDFILE) {
			if (!(traps & TR_SIGINT))
				signal(SIGINT, SIG_IGN);
			if (!(traps & TR_SIGQUIT))
				signal(SIGQUIT, SIG_IGN);
		}
		if (redir[0] == -1) {
			close(0);
			if (open("/dev/null", O_RDWR, 0) == -1) {
				fputs("/dev/null: cannot open\n", stderr);
				_exit(01);
			}
		}
	} else if (!as_sub && dointr) {
		if (!(traps & TR_SIGINT))
			signal(SIGINT, SIG_DFL);
		if (!(traps & TR_SIGQUIT))
			signal(SIGQUIT, SIG_DFL);
	}
	if (!(traps & TR_SIGTERM))
		signal(SIGTERM, SIG_DFL);

	if (*args[0] == '(') {
		if ((flags & FL_ASYNC) && !as_sub)
			as_sub = 1;
		*args[0] = ' ';
		p = args[0] + strlen(args[0]);
		while (*--p != ')')
			;	/* nothing */
		*p = '\0';
		pxline(args[0], PX_EXEC);
		if (as_sub)
			as_sub = 0;
		exit(status);
	} else {
		if (globargs(args) == NULL)
			_exit(01);
		for (i = 0; args[i] != NULL; i++)
			striparg(args[i]);
	}

	/* The Sixth Edition Unix shell always used the equivalent
	 * of `.:/bin:/usr/bin' to search for commands, not PATH.
	 */
	execvp(args[0], args);
	if (errno == ENOENT || errno == ENOTDIR) {
		fprintf(stderr, "%s: not found\n", args[0]);
		_exit(0177);
	}
	fprintf(stderr, "%s: cannot execute\n", args[0]);
	_exit(0176);
}

/*
 * Change shell's working directory.
 *
 *	`chdir'		changes to HOME directory
 *	`chdir -'	changes to previous working directory
 *	`chdir dir'	changes to `dir'
 */
void
xchdir(const char *dir)
{
	int tfd;
	static int owd1 = -1, owd2 = -1;
	const char *emsg;

	/* XXX: The read bit must be set on directory in order to open() it.
	 *	`chdir -' has no effect when dup() and/or open() fail(s).
	 */
	if (owd1 != -1) {
		owd2 = dup(owd1);
		close(owd1);
	}
	owd1 = open(".", O_RDONLY | O_NONBLOCK, 0);

	emsg = "chdir: bad directory"; /* default error message */

	if (dir == NULL) { /* chdir to home directory */
		if (si.home == NULL) {
			emsg = "chdir: no home directory";
			goto chdirerr;
		}
		if (chdir(si.home) == -1)
			goto chdirerr;
	} else if (EQUAL(dir, "-")) { /* chdir to old working directory */
		if (owd2 == -1) {
			emsg = "chdir: no old directory";
			goto chdirerr;
		}
		if (owd1 != -1)
			if (fchdir(owd2) == 0) {
				/* Does owd2 still refer to a directory? */
				if ((tfd = open(".",
				    O_RDONLY | O_NONBLOCK, 0)) == -1) {
					if (close(owd2) != -1)
						owd2 = -1;
				} else
					close(tfd);
				if (fchdir(owd1) == -1 || owd2 == -1)
					goto chdirerr;
			}
		if (fchdir(owd2) == -1)
			goto chdirerr;
	} else /* chdir to any other directory */
		if (chdir(dir) == -1)
			goto chdirerr;

	/* success - clean up and return */
	if (owd2 != -1 && close(owd2) != -1)
		owd2 = -1;
	/* set close-on-exec flag */
	if (owd1 != -1)
		fcntl(owd1, F_SETFD, FD_CLOEXEC);
	status = 0;
	return;

chdirerr:
	err(emsg);
	if (owd1 != -1 && close(owd1) != -1)
		owd1 = -1;
	/* set close-on-exec flag */
	if (owd2 != -1)
		fcntl(owd2, F_SETFD, FD_CLOEXEC);
}

/*
 * Trap (i.e., ignore) or untrap the requested signals.
 * Otherwise, print the signals currently trapped by the shell.
 */
void
dotrap(char **args)
{
	void (*dosigact)(int);
	struct sigaction act, oact;
	sigset_t new_mask, old_mask;
	static int list[NSIG], fl_list;
	int error, i, signo;
	const char *emsg;
	char *sigbad;

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
		sigfillset(&new_mask);
		sigprocmask(SIG_SETMASK, &new_mask, &old_mask);

		emsg = "trap: error";
		error = 0;
		if (args[2] == NULL) {
			error = 1;
			goto unblock;
		}

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (EQUAL(args[1], "+"))
			dosigact = SIG_IGN;
		else if (EQUAL(args[1], "-"))
			dosigact = SIG_DFL;
		else {
			error = 1;
			goto unblock;
		}

		for (i = 2; args[i] != NULL; ++i) {
			errno = 0;
			signo = strtol(args[i], &sigbad, 10);
			if (errno != 0 || !*args[i] || *sigbad ||
			    signo <= 0 || signo >= NSIG) {
				fprintf(stderr,"trap: bad signal %s\n",args[i]);
				error = 1;
				emsg = NULL;
				goto unblock;
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
unblock:
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		if (error) {
			err(emsg);
			return;
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
	status = 0;
	return;
}

void
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

/*
 * Redirect and/or strip away redirection arguments (if any).
 * If act == 1, do a normal file open and argument strip.
 * If act == 0, only do an argument strip.
 * Return file descriptors on success.
 * Return NULL on failure.
 */
int *
redirect(char **args, int *redir, int act)
{
	int fd;
	unsigned i, j;
	unsigned char append, error;
	char *p, *file;

	for (append = error = i = 0; args[i] != NULL; ++i) {
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
				/* Redirect from the original standard input. */
				if (!clone && act == 1 && EQUAL(file, "-")) {
					if ((fd = dup(oldfd0)) == -1) {
						fputs("<-: cannot redirect\n",
						    stderr);
						return(NULL);
					} else
						redir[0] = fd;
				}
			}
			striparg(file);

			/* Attempt to open file; ignore if piped or `<-'. */
			if (act == 1 && redir[0] == -1) {
				if ((fd = open(file, O_RDONLY, 0)) == -1) {
					fprintf(stderr, "%s: cannot open\n",
						file);
					error = 1;
				} else
					redir[0] = fd;
			}

			if (error)
				return(NULL);
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
			striparg(file);

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
					fprintf(stderr, "%s: cannot create\n",
						file);
					error = 1;
				} else
					redir[1] = fd;
			}

			if (error)
				return(NULL);
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
 * Return args if successful.
 * Return NULL if error.
 *
 * XXX: This function should only be called from the child process
 *	after a successful call to fork().
 */
char **
globargs(char **args)
{
	glob_t gl;
	unsigned exp, i, j, k, match;
	char *dir;

	for (exp = i = match = 0; args[i] != NULL; i++) {
		if (!globchar(args[i]))
			continue;
		exp++;
		if (glob(args[i], 0, NULL, &gl) == GLOB_NOSPACE) {
			fputs("Out of memory\n", stderr);
			exit(1);
		}
		if (gl.gl_pathc == 0) {
			globfree(&gl);
			if (!clone) /* Leave the argument unchanged. */
				continue;
			if ((dir = dirname(args[i])) != NULL &&
			    close(open(dir, O_RDONLY | O_NONBLOCK, 0)) != 0) {
				err("No directory");
				return(NULL);
			}
			/* Throw away the unmatched argument. */
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
				err("Arg list too long");
				return(NULL);
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

	if (clone && exp > 0 && match == 0) {
		/* In Sixth Edition Unix, /etc/glob prints the following
		 * diagnostic in cases such as this.
		 */
		err("No match");
		return(NULL);
	}
	return(args);
}

/*
 * Check if the argument contains glob character(s).
 * Return 1 if it does or 0 if it doesn't.
 */
int
globchar(char *s)
{
	char *p;

	for (p = s; ; p++) {
		if (*(p = quote(p, 0)) == '\0')
			break;
		if (*p == '*' || *p == '?' || *p == '[')
			return(1);
	}
	return(0);
}

/*----------------------------------------------------------------------*\
 | Various support functions...						|
\*----------------------------------------------------------------------*/

/*
 * Close file descriptors associated w/ pipes and/or redirects.
 */
void
close_fd(int *redir)
{
	unsigned i;

	for (i = 0; i < 2; ++i)
		if (redir[i] != -1)
			close(redir[i]);
}

/*
 * Skip blanks at beginning of string.
 */
char *
de_blank(char *s)
{
	for ( ; *s == ' ' || *s == '\t'; ++s)
		;	/* nothing */
	return(s);
}

/*
 * Handle errors detected by the shell.
 */
void
err(const char *msg)
{
	if (msg != NULL)
		fprintf(stderr, "%s\n", msg);
	if (!INTERACTIVE)
		exit(1);
	status = 1;
}

/*
 * Allocate memory and check for error.
 */
void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL) {
		fputs("Out of memory\n", stderr);
		exit(1);
	}
	return(mp);
}

/*
 * Reallocate memory and check for error.
 */
void *
xrealloc(void *rp, size_t s)
{
	if ((rp = realloc(rp, s)) == NULL) {
		fputs("Out of memory\n", stderr);
		exit(1);
	}
	return(rp);
}

/*
 * Return pointer to the next unquoted character.
 * Return NULL if syntax error.
 */
char *
quote(char *s, int dosub)
{
	char ch, *p;

	for (p = s; *p != '\0'; p++) {
		switch (*p) {
		case '\"':
		case '\'':
			/* Everything within "double" or 'single' quotes
			 * is treated literally.
			 */
			for (ch = *p++; *p != ch; p++)
				if (*p == '\0')
					return(NULL); /* unmatched " or ' */
			break;
		case '\\':
			/* The next character is quoted (escaped). */
			if (*++p == '\0')
				return(p);
			break;
		case '(':
			if (dosub && (p > s || (p = psynsub(p)) == NULL))
					return(NULL); /* bad subshell */
			/* FALLTHROUGH */
		default:
			return(p);
		}
	}
	return(p);
}

/*
 * Remove all unquoted occurrences of ", ', and \ .
 *
 * XXX: Using strcpy() here should be safe as the resulting argument
 *	is always the same length as, or shorter than, the original.
 */
void
striparg(char *a)
{
	unsigned i;
	char b[LINELEN], ch, *p;

	for (p = a, i = 0; *p != '\0'; p++, i++) {
		switch (*p) {
		case '\"':
		case '\'':
			ch = *p++;
			while (*p != ch)
				b[i++] = *p++;
			i--;
			continue;
		case '\\':
			if (*++p == '\0')
				goto stripend;
			/* FALLTHROUGH */
		default:
			b[i] = *p;
		}
	}
stripend:
	b[i] = '\0';
	strcpy(a, b);
}

/*
 * Print termination message according to signal s.
 * If p != -1, precede message w/ process ID (e.g., `&' commands).
 * The sh(1) manual page from Sixth Edition Unix says `Broken Pipe'
 * is reported; however, it actually is not.
 */
void
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
	case SIGPIPE:	return;		/* Broken pipe is not reported. */
	case SIGINT:	rsig = 1; msg1 = "";		break;
	case SIGQUIT:	rsig = 1; msg1 = "Quit";	break;
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
	if ((msg1 == NULL || *msg1 != '\0') && p != -1)
		len = snprintf(msgbuf, sizeof(msgbuf), "%d: ", p);
	if (msg1 == NULL)
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "Sig %d%s\n", signo, msg2);
	else
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "%s%s\n", msg1, msg2);
	write(STDERR_FILENO, msgbuf, strlen(msgbuf));
}

/*
 * Report termination status of asynchronous commands and exorcise
 * any stray zombies that might exist.
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
				if (cstat) {
					if (WIFSIGNALED(cstat)) {
						sigmsg(cstat, apid);
						if (status == 0215)
							continue;
					} else if (WIFEXITED(cstat))
						status = WEXITSTATUS(cstat);

					/* Special-case shell terminations. */
					if (status >= 0176 && !INTERACTIVE) {
						lseek(STDIN_FILENO, (off_t)0,
						    SEEK_END);
						if (shtype & COMMANDFILE)
							exit(status);
					}

				} else
					status = 0;
				aplist[i] = 0;
				break;
			}
		}
	}
	/* Reset apc if there are no more asynchronous processes. */
	if (apid == -1 && apc > 0)
		apc = 0;
	apstat = apid;
}
