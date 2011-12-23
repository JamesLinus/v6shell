/*
 * osh - clone of the Sixth Edition (V6) Unix shell
 */
/*
 * Copyright (c) 2003, 2004
 *	Jeffrey Allen Neitzel.  All rights reserved.
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
const char revision[] = "@(#)osh.c	1.94 (jneitzel) 2004/04/16";
#endif	/* !lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define	ARGMAX		256
#define	LINELEN		1024
#define	PLSIZE		16	/* # of processes for *[as]plist allocations */

#define	EQUAL(a, b)	(strcmp(a, b) == 0)
#define	INTERACTIVE	(shtype == 2)

/* command flags */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

/* action values for command parsing/execution */
#define	PX_SYN		0	/* check syntax only */
#define	PX_SYNEXEC	1	/* check syntax and execute if syntax is OK */
#define	PX_EXEC		2	/* execute only */

#ifndef	WIFCORED
#define	WIFCORED(s)	((s) & 0200)
#endif

/* special built-in commands */
#define	SBINULL		0
#define	SBICHDIR	1
#define	SBIEXIT		2
#define	SBILOGIN	3
#define	SBISHIFT	4
#define	SBIWAIT		5
const char *const sbi[] = {
	":",		/* SBINULL */
	"chdir",	/* SBICHDIR */
	"exit",		/* SBIEXIT */
	"login",	/* SBILOGIN */
	"shift",	/* SBISHIFT */
	"wait",		/* SBIWAIT */
	NULL
};

int		apc;		/* # of known asynchronous processes */
int		aplim;		/* current limit on # of processes in aplist */
pid_t		*aplist;	/* array of known asynchronous process IDs */
signed char	apstat = -1;	/* whether or not to call apwait() */
char		*cmdnam;	/* command name - used for $0 */
pid_t		oshpid;		/* process ID of shell */
uid_t		oshuid;		/* real user ID of shell/user */
unsigned	posac;		/* count of positional parameters */
char		**posav;	/* positional parameters */
int		retval;		/* return value of shell */
unsigned char	rsig;		/* if command execution received INTR or QUIT */
unsigned char	shtype;		/* type of shell */
int		spc;		/* # of processes in current pipeline */
int		splim;		/* current limit on # of processes in splist */
pid_t		*splist;	/* array of process IDs in current pipeline */
#ifndef	CLONE
char		*home;		/* user's home directory */
#endif

void	apwait(int);
void	bargs(char *, int *, int);
char	*de_blank(char *);
void	err(const char *);
void	execute(char **, int *, int);
char	**globargs(char **);
int	lookup(const char *);
void	oshinit(void);
char	*psynsub(char *);
int	psyntax(char *);
int	pxline(char *, int);
int	pxpipe(char *, int, int);
char	*quote(char *);
char	*readline(char *, size_t, int);
int	*redirect(char **, int *, int);
void	sigmsg(int, pid_t);
char	*striparg(char *);
int	stripsub(char *);
char	*substparm(char *);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);
#ifndef	CLONE
void	xchdir(const char *);
#endif

int
main(int argc, char **argv)
{
	off_t ioff;
	unsigned bsc;
	int ch, input;
	unsigned char tflag;
	char line[LINELEN];
	char *pr;

	oshinit();
	input = tflag = 0;

	/* Setup the positional parameters. */
	cmdnam = argv[1];
	posav = &argv[1];
	posac = argc - 1;

	/* Act accordingly, based on how the shell was invoked. */
	if (argc == 1) {
		if (isatty(0) && isatty(2)) { /* interactive shell */
			shtype = 2;
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
		}
	} else if (argc > 1 && *argv[1] == '-') { /* non-interactive shell */
		/*
		 * This implementation behaves exactly like the V6 shell
		 * WRT options.  For compatibility, any option is accepted
		 * w/o producing an error.
		 */
		shtype = 1;
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		switch (*(argv[1] + 1)) {
		case 'c':
			/* osh -c [string]
			 * XXX: `-canything' works too (compatible).
			 */
			if (argv[2] == NULL) /* read from input w/o prompting */
				break;

			/* Deal w/ '\' characters at end of string. */
			pr = argv[2] + strlen(argv[2]);
			for (bsc = 1;
			    (pr - bsc) >= argv[2] && *(pr - bsc) == '\\'; ++bsc)
				;	/* nothing */
			if ((bsc - 1) % 2 == 1)
				return(0);

			/* Parse and execute string as a command line. */
			pr = argv[2];
			if (pxline(pr, PX_SYNEXEC) == 0)
				err("syntax error");
			return(retval);
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
		/* osh name [arg1 ... [arg9]] */
		close(input);
		if ((input = open(argv[1], O_RDONLY, 0)) == -1) {
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
				return(0);
		}
		/* XXX: Of course, lseek() fails when command file is a FIFO. */
		if (lseek(input, ioff, SEEK_SET) == -1) {
			fputs("Cannot seek\n", stderr);
			return(1);
		}
	}

	/* main command loop */
	for (;;) {
again:
		if (INTERACTIVE)
			write(2, oshuid ? "% " : "# ", (size_t)2);

		pr = readline(line, (size_t)LINELEN, input);
		if (pr == NULL)
			break;
		if (pr == (char *)-1)
			continue;

		/* While a line of input ends w/ a `\' character, read input
		 * from the next line if the number of `\' characters is odd.
		 * Each time, the last `\' is translated into a blank.
		 */
		while ((pr - 1) >= line && *(pr - 1) == '\\') {
			for (bsc = 2;
			    (pr - bsc) >= line && *(pr - bsc) == '\\'; ++bsc)
				;	/* nothing */
			if ((bsc - 1) % 2 == 0)
				break;
			if (tflag)
				return(0);
			*(pr - 1) = ' ';
			pr = readline(pr, (size_t)LINELEN - (pr - line), input);
			if (pr == NULL)
				return(retval);
			if (pr == (char *)-1)
				goto again;
		}

		/* Parse and execute command line. */
		if (pxline(line, PX_SYNEXEC) == 0)
			err("syntax error");
		if (tflag)
			break;
	}
	return(retval);
}

/*
 * osh initialization
 */
void
oshinit()
{
	struct stat fdsb;
	int fd;
#ifndef	CLONE
	size_t len;
	char *str;
#endif

	/* Revoke set-id privileges; get shell/user ID and process ID. */
	setgid(getgid());
	setuid(oshuid = getuid());
	oshpid = getpid();

	/* Ensure that stdin, stdout, and stderr are not closed. */
	for (fd = 0; fd < 3; ++fd)
		if (fstat(fd, &fdsb) == -1) {
			close(fd);
			if (open("/dev/null", O_RDWR, 0) != fd) {
				fputs("/dev/null: cannot open\n", stderr);
				exit(1);
			}
		}

	/* Allocate memory for aplist and splist; reallocate in execute(). */
	aplim = splim = PLSIZE;
	aplist = xmalloc(aplim * sizeof(pid_t));
	splist = xmalloc(splim * sizeof(pid_t));

#ifndef	CLONE
	/* Get user's home directory for use in xchdir(). */
	if ((str = getenv("HOME")) != NULL && *str != '\0' &&
	    (len = strlen(str)) < PATH_MAX) {
		home = xmalloc(len + 1);
		strcpy(home, str);
	} else
		home = NULL;
#endif	/* !CLONE */
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
readline(char *b, size_t len, int fd)
{
	unsigned char eot;
	char *end, *p;

	end = b + len;
	for (p = b, eot = 0; p < end; p++) {
		switch (read(fd, p, (size_t)1)) {
		case -1:
			exit(1);
		case 0:
			/* If EOF occurs at the beginning of line, return NULL
			 * immediately.  Otherwise, ignore EOF until it occurs
			 * 3 times in sequence and then return NULL.
			 */
			if (p > b && ++eot < 3) {
				*p-- = '\0';
				continue;
			}
			if (INTERACTIVE)
				write(2, "\n", (size_t)1);
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
	int flag, res;
	unsigned char doex;
	char *p, *lpip, *cl_sav = NULL;

	if (*(cl = de_blank(cl)) == '\0')
		return(1);

	if (act == PX_SYNEXEC) { /* save a copy first */
		cl_sav = xmalloc(strlen(cl) + 1);
		strcpy(cl_sav, cl);
	}

	for (doex = flag = res = rsig = spc = 0, p = cl, lpip = cl; ; ++p) {
		if (rsig || (p = quote(p)) == NULL)
			goto px_end;
		switch (*p) {
		case ';':
		case '&':
			flag = (*p == '&') ? FL_ASYNC : 0;
			*p = '\0';
			lpip = de_blank(lpip);
			if (act != PX_EXEC && *lpip != '\0') {
				if (pxpipe(lpip, flag, PX_SYN) == 0)
					goto px_end;
				doex = 1;
			} else if (*lpip != '\0')
				if (pxpipe(lpip, flag, PX_EXEC) == 0)
					goto px_end;
			lpip = p + 1;
			break;
		case '\0':
			lpip = de_blank(lpip);
			if (act != PX_EXEC && *lpip != '\0') {
				if (pxpipe(lpip, 0, PX_SYN) == 0)
					goto px_end;
				doex = 1;
			} else if (*lpip != '\0')
				if (pxpipe(lpip, 0, PX_EXEC) == 0)
					goto px_end;
			if (act == PX_SYNEXEC && doex)
				pxline(cl_sav, PX_EXEC);
			res = 1;
			goto px_end;
		}
	}

px_end:
	if (cl_sav) {
		free(cl_sav);
		cl_sav = NULL;
	}
	return(res);
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
		p = quote(p);
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
					apstat = 1; /* force zombie check */
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
		if (*a != '(') a = quote(a);
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
			if ((p = quote(p)) == NULL)
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
			if ((p = quote(p)) == NULL)
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
	char *args[ARGMAX];
	char *p, *larg;

	for (i = 0, p = cmd, larg = cmd; ; p++) {
		p = quote(p);
		switch (*p) {
		case ' ':
		case '\t':
			*p = '\0';
			if (*(p - 1) != '\0') {
				/* error if too many args */
				if (i + 1 >= ARGMAX)
					goto bargserr;
				args[i] = xmalloc(strlen(larg) + 1);
				strcpy(args[i++], larg);
			}
			larg = p + 1;
			break;
		case '\0':
			if (*(p - 1) != '\0') {
				/* error if too many args */
				if (i + 1 >= ARGMAX)
					goto bargserr;
				args[i] = xmalloc(strlen(larg) + 1);
				strcpy(args[i++], larg);
			}
			goto bargsend;
		}
	}

bargsend:
	args[i] = NULL;
	if (args[0] == NULL) /* Should never happen... */
		return;
	/* Report termination of asynchronous commands. */
	if ((apc > 0 || apstat >= 0) && spc == 0)
		apwait(WNOHANG);
	execute(args, redir, flags);
	for (i = 0; args[i]; i++) {
		free(args[i]);
		args[i] = NULL;
	}
	return;

bargserr:
	err("Too many args");
	while (i != 0) {
		free(args[--i]);
		args[i] = NULL;
	}
}

/*
 * Determine whether or not command name is a special built-in.
 * If it is, return the index value, 0 - 5, to indicate which one.
 * Otherwise, return -1 to indicate an external command.
 */
int
lookup(const char *cmd)
{
	int i;

	for (i = 0; sbi[i]; ++i)
		if (EQUAL(cmd, sbi[i]))
			return(i);
	return(-1);
}

/*
 * Execute command as appropriate after manipulating file descriptors
 * for pipes and I/O redirection.  Keep track of each created process
 * to allow for proper termination reporting.
 */
void
execute(char **args, int *redir, int flags)
{
	pid_t cpid;
	int cmd, i, ract, status;
	static unsigned char as_sub;
	char as_pid[32];
	char *p;

	/* Allow redirection arguments to appear before command name. */
	for (i = 0; args[i]; ++i)
		if (*args[i] == '<') {
			if (*(args[i] + 1) == '\0')
				++i;
		} else if (*args[i] == '>') {
			if (*(args[i] + 1) == '\0' ||
			    (*(args[i] + 1) == '>' && *(args[i] + 2) == '\0'))
				++i;
		} else
			break;
	ract = ((cmd = lookup(args[i])) == -1) ? 1 : 0;

	/* Redirect now that we know which argument is command name.
	 * Special built-in commands cannot be redirected (ract == 0).
	 */
	if (redirect(args, redir, ract) == NULL) {
		err(NULL);
		apstat = 1; /* force zombie check */
		goto exec_err;
	}
	if (cmd == -1)
		goto external;

	/* Special built-in commands cannot be piped.
	 * Close the appropriate file descriptor(s) and
	 * continue as normal.
	 */
	for (i = 0; i < 2; i++)
		if (redir[i] != -1) {
			close(redir[i]);
			redir[i] = -1;
			apstat = 1; /* force zombie check */
		}

	/* Execute the requested built-in command. */
	switch (cmd) {
	case SBINULL:
		/* the "do-nothing" command */
		retval = 0;
		return;
	case SBICHDIR:
		if (args[1] != NULL) {
			if ((p = substparm(args[1])) == NULL)
				return;
			args[1] = p;
			striparg(args[1]);
		}
#ifdef	CLONE
		else {
			err("chdir: arg count");
			return;
		}
		if (chdir(args[1]) == -1)
			err("chdir: bad directory");
		else
			retval = 0;
#else
		xchdir(args[1]);
#endif
		return;
	case SBIEXIT:
		/* Terminate command file. */
		if (shtype == 0)
			exit(retval);
		return;
	case SBILOGIN:
		if (!INTERACTIVE) {
			fputs("login: cannot execute\n", stderr);
			retval = 1;
			return;
		}
		/* login cannot be backgrounded, piped, or redirected. */
		flags &= ~(FL_ASYNC | FL_PIPE);
		flags |= FL_EXEC;
		break;
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
		retval = 0;
		return;
	case SBIWAIT:
		/* wait, like other special commands, is non-interruptible. */
		apwait(0);
		return;
	}

external:
	/*
	 * Fork and execute external command.
	 */
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
		goto exec_err;
	case 0:
		break;
	default:
		/**** Parent! ****/
		for (i = 0; i < 2; i++)
			if (redir[i] != -1)
				close(redir[i]);

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
			write(2, as_pid, strlen(as_pid));
			return;
		}

		/* Wait and fetch termination status. */
		for (i = 0; i < spc; ++i) {
			waitpid(splist[i], &status, 0);
			if (status) {
				if (WIFSIGNALED(status)) {
					sigmsg(status, (i==spc-1)?-1:splist[i]);
					if (retval == 0215)
						continue;
				} else if (WIFEXITED(status))
					retval = WEXITSTATUS(status);
				if (shtype == 0 && retval >= 0176)
					exit(retval);
			} else
				retval = 0;
		}

		/* reset process count */
		spc = 0;
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
		if (redir[0] == -1) {
			close(0);
			if (open("/dev/null", O_RDWR, 0) == -1) {
				fputs("/dev/null: cannot open\n", stderr);
				_exit(01);
			}
		}
	} else if (!as_sub) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
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
		exit(retval);
	} else {
		if (globargs(args) == NULL)
			_exit(01);
		for (i = 0; args[i]; i++) {
			if ((p = substparm(args[i])) == NULL)
				_exit(01);
			args[i] = p;
			striparg(args[i]);
		}
	}

	/* Don't bother calling execvp(3) when command name is too long
	 * and is not prefixed w/ path name.
	 */
	if (strchr(args[0], '/') == NULL && strlen(args[0]) > NAME_MAX) {
		fprintf(stderr, "%s: cannot execute\n", args[0]);
		_exit(0176);
	}
	/* The V6 shell always used the equivalent of .:/bin:/usr/bin
	 * to search for commands, not PATH.
	 */
	execvp(args[0], args);
	if (errno == ENOENT || errno == ENOTDIR) {
		fprintf(stderr, "%s: not found\n", args[0]);
		_exit(0177);
	} else {
		fprintf(stderr, "%s: cannot execute\n", args[0]);
		_exit(0176);
	}

exec_err:
	/**** Parent again! ****/
	for (i = 0; i < 2; i++)
		if (redir[i] != -1)
			close(redir[i]);
}

#ifndef	CLONE
/*
 * Change shell's working directory.
 *
 *	`chdir'		changes to HOME directory
 *	`chdir -'	changes to previous directory
 *	`chdir dir'	changes to `dir'
 */
void
xchdir(const char *dir)
{
	static int owd1 = -1, owd2 = -1;
	const char *emsg;

	/* XXX: The read bit must be set on directory in order to open() it.
	 *	`chdir -' has no effect when dup() and/or open() fail(s).
	 */
	/* Duplicate fd of old working dir if previous chdir succeeded. */
	if (owd1 != -1) {
		owd2 = dup(owd1);
		close(owd1);
	}
	/* Open current dir; it becomes old working dir if chdir succeeds. */
	owd1 = open(".", O_RDONLY, 0);

	emsg = "chdir: bad directory"; /* default error message */

	if (dir == NULL) { /* chdir to home directory */
		if (home == NULL) {
			emsg = "chdir: no home directory";
			goto chdirerr;
		}
		if (chdir(home) == -1)
			goto chdirerr;
	} else if (EQUAL(dir, "-")) { /* chdir to old working directory */
		if (owd2 == -1) {
			emsg = "chdir: no old directory";
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
	retval = 0;
	return;

chdirerr:
	err(emsg);
	if (owd1 != -1 && close(owd1) != -1)
		owd1 = -1;
	/* set close-on-exec flag */
	if (owd2 != -1)
		fcntl(owd2, F_SETFD, FD_CLOEXEC);
}
#endif	/* !CLONE */

/*
 * Get redirection info from args and strip it away.
 * If act == 1 do normal file open, strip, and free.
 * If act == 0 do only strip and free.
 * Return file descriptors.
 * Return NULL at error.
 */
int *
redirect(char **args, int *redir, int act)
{
	int fd;
	unsigned i, j;
	unsigned char append, error;
	unsigned char strip[ARGMAX];
	char *p, *file;

	for (append = error = i = 0; args[i]; ++i) {
		switch (*args[i]) {
		case '<':
			strip[i] = 1;
			p = args[i] + 1;

			/* Get file name; do parameter substitution if $... */
			if (*p == '\0') {
				++i;
				if ((p = substparm(args[i])) == NULL)
					return(NULL);
				file = args[i] = p;
				strip[i] = 1;
			} else {
				file = xmalloc(strlen(p) + 1);
				strcpy(file, p);
				if ((p = substparm(file)) == NULL) {
					free(file);
					file = NULL;
					return(NULL);
				}
				file = p;
				strip[i] += 1;
			}
			striparg(file);

			/* Attempt to open file, or ignore if piped. */
			if (act == 1 && redir[0] == -1) {
				if ((fd = open(file, O_RDONLY, 0)) == -1) {
					fprintf(stderr, "%s: cannot open\n",
						file);
					error = 1;
				} else
					redir[0] = fd;
			}

			if (strip[i] == 2) {
				free(file);
				file = NULL;
			}
			if (error)
				return(NULL);
			break;
		case '>':
			strip[i] = 1;
			if (*(args[i] + 1) == '>') {
				append = 1;
				p = args[i] + 2;
			} else
				p = args[i] + 1;

			/* Get file name; do parameter substitution if $... */
			if (*p == '\0') {
				++i;
				if ((p = substparm(args[i])) == NULL)
					return(NULL);
				file = args[i] = p;
				strip[i] = 1;
			} else {
				file = xmalloc(strlen(p) + 1);
				strcpy(file, p);
				if ((p = substparm(file)) == NULL) {
					free(file);
					file = NULL;
					return(NULL);
				}
				file = p;
				strip[i] += 1;
			}
			striparg(file);

			/* Attempt to open file, or ignore if piped. */
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

			if (strip[i] == 2) {
				free(file);
				file = NULL;
			}
			if (error)
				return(NULL);
			break;
		default:
			strip[i] = 0;
			break;
		}
	}

	/* Free memory used by, and strip away, redirection arguments. */
	for (i = 0, j = 0; args[i]; ) {
		while (args[i] && strip[i] > 0) {
			free(args[i]);
			args[i++] = NULL;
		}
		if (args[i])
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
 */
char **
globargs(char **args)
{
	glob_t gl;
	unsigned exp, i, j, k;
	char *p;

	for (i = 0; args[i]; i++) {
		/* First, check for glob character(s). */
		for (exp = 0, p = args[i]; !exp; p++) {
			if (*(p = quote(p)) == '\0')
				break;
			if (*p == '*' || *p == '?' || *p == '[')
				exp = 1;
		}
		if (!exp)
			continue;

		/* Found glob character(s); expand argument now. */
		switch (glob(args[i], 0, NULL, &gl)) {
		case GLOB_NOSPACE:
			fputs("Out of memory\n", stderr);
			exit(1);
		case GLOB_NOMATCH:
			globfree(&gl);
#ifdef	CLONE
			/* In the V6 shell, /etc/glob prints 'No match'
			 * and does not execute the requested command.
			 */
			err("No match");
			return(NULL);
#else
			/* Leave the argument unchanged. */
			continue;
#endif
		default:
			break;
		}
		if (gl.gl_pathc == 1) {
			args[i] = xrealloc(args[i], strlen(gl.gl_pathv[0]) + 1);
			strcpy(args[i], gl.gl_pathv[0]);
		} else {
			for (j = i; args[j]; j++)
				;	/* nothing */
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				globfree(&gl);
				err("Arg list too long");
				return(NULL);
			}
			for ( ; j > i; j--)
				args[j + gl.gl_pathc - 1] = args[j];
			free(args[i]);
			for (j = i + gl.gl_pathc, k = 0; i < j; i++, k++) {
				args[i] = xmalloc(strlen(gl.gl_pathv[k]) + 1);
				strcpy(args[i], gl.gl_pathv[k]);
			}
			i--;
		}
		globfree(&gl);
	}
	return(args);
}

/*
 * Substitute unquoted occurrences of $...
 * Return NULL on failure (too many characters).
 * Otherwise, return s.
 */
char *
substparm(char *s)
{
	size_t i;
	unsigned argno;
	unsigned char changed;
	char b[LINELEN], num[32];
	char *p, *q;

	for (p = s, changed = i = 0; *p != '\0'; p++, i++) {
		/* skip quoted characters */
		q = quote(p);
		while (p < q) {
			if (i + 1 >= LINELEN)
				goto substerr;
			b[i++] = *p++;
		}
		if (*p == '\0')
			break;

		if (i + 1 >= LINELEN)
			goto substerr;
		if (*p == '$') {
			changed = 1;
			if (isdigit(*++p)) {
				num[0] = *p;
				num[1] = '\0';
				argno = atoi(num);
				if (argno < posac) {
					if (argno == 0)
						q = cmdnam;
					else
						q = posav[argno];
					while (*q != '\0') {
						if (i + 1 >= LINELEN)
							goto substerr;
						b[i++] = *q++;
					}
				}
				i--;
			} else if (*p == '$') {
				snprintf(num, sizeof(num), "%05u", oshpid);
				q = num;
				while (*q != '\0') {
					if (i + 1 >= LINELEN)
						goto substerr;
					b[i++] = *q++;
				}
				i--;
			} else {
				i--;
				p--;
			}
		} else
			b[i] = *p;
	}
	b[i] = '\0';
	if (changed) {
		s = xrealloc(s, i + 1);
		strcpy(s, b);
	}
	return(s);

substerr:
	err("Too many characters");
	return(NULL);
}

/*----------------------------------------------------------------------*\
 | Support - Various functions used everywhere...			|
\*----------------------------------------------------------------------*/

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
	retval = 1;
}

void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL) {
		fputs("Cannot malloc\n", stderr);
		exit(1);
	}
	return(mp);
}

void *
xrealloc(void *rp, size_t s)
{
	if ((rp = realloc(rp, s)) == NULL) {
		fputs("Cannot realloc\n", stderr);
		exit(1);
	}
	return(rp);
}

/*
 * Return pointer to the next unquoted character.
 * Return NULL if syntax error.
 */
char *
quote(char *s)
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
			if (s < p || (p = psynsub(p)) == NULL)
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
 * Return a
 */
char *
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
	/* reassign to argument */
	strcpy(a, b);
	return(a);
}

/*
 * Print termination message according to signal s.
 * If p != -1, precede message w/ process ID (e.g., `&' commands).
 * The V6 sh(1) manual page says that 'Broken Pipe' is reported;
 * however, it actually is not.
 */
void
sigmsg(int s, pid_t p)
{
	int len, signo;
	char msgbuf[64];
	const char *msg1, *msg2;

	signo  = WTERMSIG(s);
	retval = signo + 0200;

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
	msg2 = (WIFCORED(s)) ? " -- Core dumped" : "";
	len = 0;
	if ((msg1 == NULL || *msg1 != '\0') && p != -1)
		len = snprintf(msgbuf, sizeof(msgbuf), "%d: ", p);
	if (msg1 == NULL)
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "Sig %d%s\n", signo, msg2);
	else
		snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "%s%s\n", msg1, msg2);
	write(2, msgbuf, strlen(msgbuf));
}

/*
 * Report termination status of asynchronous commands and exorcise
 * any stray zombies that might exist.
 */
void
apwait(int opt)
{
	pid_t apid;
	int i, status;

	while ((apid = waitpid(-1, &status, opt)) > 0) {
		/* Check if apid is a known asynchronous process. */
		for (i = 0; i < apc; ++i) {
			if (aplist[i] == 0) /* already reported */
				continue;
			if (aplist[i] == apid) { /* report termination */
				if (status) {
					if (WIFSIGNALED(status)) {
						sigmsg(status, apid);
						if (retval == 0215)
							continue;
					} else if (WIFEXITED(status))
						retval = WEXITSTATUS(status);
					if (shtype == 0 && retval >= 0176)
						exit(retval);
				} else
					retval = 0;
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
