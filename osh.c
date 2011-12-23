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
const char revision[] = "@(#)osh.c	1.93 (jneitzel) 2004/02/16";
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

#define	equal(a, b)	(strcmp(a, b) == 0)	/* compare strings */

/* command flags */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

/* action flags for parsing/execution */
#define	PX_SYN		(-1)	/* check syntax only */
#define	PX_SYNEXEC	0	/* check syntax and execute if syntax OK */
#define	PX_EXEC		1	/* execute only */

#ifndef	GLFLAGS
#define	GLFLAGS		(GLOB_ERR | GLOB_NOCHECK | GLOB_NOESCAPE)
#endif

#ifndef	WIFCORED
#define	WIFCORED(s)	((s) & 0200)
#endif

/* special built-in commands */
#define	SBINULL		0
#define	SBICHDIR	1
#define	SBISHIFT	2
#define	SBIWAIT		3
#define	SBIEXIT		4
#define	SBILOGIN	5
const char *const sbi[] = {
	":",		/* SBINULL */
	"chdir",	/* SBICHDIR */
	"shift",	/* SBISHIFT */
	"wait",		/* SBIWAIT */
	"exit",		/* SBIEXIT */
	"login",	/* SBILOGIN */
	NULL
};

int		apc;		/* # of known asynchronous processes */
int		aplim;		/* current limit on # of processes in aplist */
pid_t		*aplist;	/* array of known asynchronous process IDs */
pid_t		apstat;		/* whether or not to call apwait() */
char		*cmdfil;	/* command file name - used for $0 */
int		input;		/* input file descriptor */
unsigned char	intty;		/* if stdin and stderr are associated w/ tty */
pid_t		oshpid;		/* pid of shell */
uid_t		oshuid;		/* uid of shell/user */
unsigned	posac;		/* count of positional parameters */
char		**posav;	/* positional parameters */
int		retval;		/* return value of shell */
unsigned char	r_intr;		/* if command execution received SIGINT */
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
void	sigmsg(pid_t, int);
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
	int bsc, ch;
	char line[LINELEN];
	char *pr;

	oshinit();
	if (argc >= 2 && *argv[1] == '-') {
		switch (*(argv[1] + 1)) {
		case 't':
			input = 0;
			pr = readline(line, (size_t)LINELEN, input);
			if (pr == NULL)
				exit(0);
			if (pr == (char *)-1)
				exit(1);
			if (pxline(line, PX_SYNEXEC) == 0)
				err("syntax error");
			return(retval);
		case 'c':
			if (*(argv[1] + 2) != '\0') {
				pr = argv[1] + 2;
			} else {
				if (argv[2] != NULL) {
					pr = argv[2];
				} else {
					/* normal read, without prompt */
					argc = 1;
					input = 0;
					goto mainloop;
				}
			}
			input = 0;	/* to avoid closing in child */
			if (pxline(pr, PX_SYNEXEC) == 0)
				err("syntax error");
			return(retval);
		default: /* silly, but compatible */
			argc = 1;
			input = 0;
			goto mainloop;
		}
	}
	if (argc == 1) { /* interactive, or input from pipe or '<file' */
		input = 0;
		if (isatty(0) && isatty(2)) {
			/* Standard input and standard error output must be
			 * associated w/ a tty for shell to be interactive.
			 */
			intty = 1;
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
		}
	} else {
		close(0);
		input = open(argv[1], O_RDONLY);
		if (input == -1) {
			fprintf(stderr, "%s: cannot open\n", argv[1]);
			exit(1);
		}
		/* Ignore first line in command file if it begins w/ '#!' . */
		ioff = 0;
		if (getchar() == '#' && getchar() == '!') {
			ioff += 2;
			do {
				ch = getchar();
				++ioff;
			} while (ch != '\n' && ch != EOF);
			if (ch == EOF)
				exit(0);
		}
		if (lseek(0, ioff, SEEK_SET) == -1) {
			fputs("Cannot seek\n", stderr);
			exit(1);
		}
		cmdfil = argv[1];
		posav = &argv[1];
		posac = argc - 1;
	}
mainloop:
	for (;;) {
mainext:
		if (intty)
			write(2, oshuid ? "% " : "# ", (size_t)2);

		pr = readline(line, (size_t)LINELEN, input);
		if (pr == NULL)
			break;
		if (pr == (char *)-1)
			continue;

		/* While line of input ends w/ a '\' character... */
		while (*(pr - 1) == '\\') {
			/*
			 * Continue reading input from next line if number
			 * of '\' characters is odd; break if even.
			 */
			for (bsc = 2;
			    (pr - bsc) >= line && *(pr - bsc) == '\\'; ++bsc)
				;	/* nothing */
			if ((bsc - 1) % 2 == 0)
				break;
			*(pr - 1) = ' ';
			pr = readline(pr, (size_t)LINELEN - (pr - line), input);
			if (pr == NULL)
				goto osh_exit;
			if (pr == (char *)-1)
				goto mainext;
		}

		/* Parse and execute command line now. */
		if (r_intr) r_intr = 0;
		if (pxline(line, PX_SYNEXEC) == 0)
			err("syntax error");
	}
osh_exit:
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
	setuid(getuid());
	oshuid = getuid();
	oshpid = getpid();

	/* Ensure that stdin, stdout, and stderr are not closed. */
	for (fd = 0; fd < 3; ++fd)
		if (fstat(fd, &fdsb) == -1) {
			close(fd);
			if (open("/dev/null", O_RDWR) != fd) {
				fputs("/dev/null: cannot open\n", stderr);
				exit(1);
			}
		}

	/* Initialize apstat. */
	apstat = -1;

	/* Allocate memory for aplist and splist; reallocate in execute(). */
	aplim = splim = PLSIZE;
	aplist = xmalloc(aplim * sizeof(pid_t));
	splist = xmalloc(splim * sizeof(pid_t));

#ifndef	CLONE
	/* Get user's home directory for use in xchdir().
	 * We don't check if it's a symlink to a too long directory.
	 * Just let chdir() fail w/ ENAMETOOLONG in that case.
	 */
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
 * Terminate w/ error if EBADF; input has been lost.
 * Terminate w/ error if command line overflow and not interactive,
 * or flush received overflow data and return error if interactive.
 * Return the end of string if successful.
 * Return NULL if EOF.
 * Return -1 if error.
 */
char *
readline(char *b, size_t len, int fd)
{
	unsigned eot;
	char *p, *end;

	end = b + len;
	for (p = b, eot = 0; p < end; p++) {
		switch (read(fd, p, (size_t)1)) {
		case -1:
			/* Always terminate shell if input is ever lost. */
			if (errno == EBADF) {
				fputs("Cannot read input\n", stderr);
				exit(1);
			}
			return((char *)-1);
		case 0:
			/* If EOF occurs at the beginning of line, return NULL
			 * immediately.  Otherwise, ignore EOF until it occurs
			 * 3 times in sequence.  Return NULL at this point.
			 */
			if (p > b && ++eot < 3) {
				*p-- = '\0';
				continue;
			}
#ifndef	CLONE
			/* This is just a personal preference of mine... */
			if (intty)
				fputc('\n', stdout);
#endif
			return(NULL);
		}
		eot = 0;
		if (*p == '\n') {
			*p = '\0';
			return(p);
		}
	}
	err("Command line overflow");
	tcflush(fd, TCIFLUSH);
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
	int doex, flag, res;
	char *p, *lpip, *cl_sav = NULL;

	/* reset process count */
	spc = 0;

	if (*(cl = de_blank(cl)) == '\0')
		return(1);

	if (act == 0) { /* save a copy first */
		cl_sav = xmalloc(strlen(cl) + 1);
		strcpy(cl_sav, cl);
	}

	for (doex = flag = res = 0, p = cl, lpip = cl; ; ++p) {
		if (r_intr)
			goto px_end;
		if ((p = quote(p)) == NULL)
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
			if (act == 0 && doex == 1)
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
 * Parse pipeline for proper syntax if act == PX_SYN; otherwise, split
 * pipeline into individual commands connected by pipes and execute.
 * Return 1 on success.
 * Return 0 on failure.
 *
 * XXX: The following command line (and similar):
 *
 * % cat </dev/zero | sleep 1
 *
 * demonstrates the need to set pipe descriptors to close-on-exec in
 * this implementation.  The reason..?  If the pipe descriptors are not
 * closed after dup2() in the child, the pipe will never enter an EOF state.
 * The associated process will block on writing to the pipe.  Hence, the
 * process will hang around until killed manually.  Not very desirable.
 *
 * The cat command above should receive a SIGPIPE (i.e., Broken pipe).
 * Because of the way pipes are created and used in this implementation,
 * extra steps are needed to ensure that the pipe's descriptors get closed
 * after dup2() in the forked child; see execute().  Piped commands that
 * pass to execution from here do not know the descriptor for the other
 * end of the pipe.  TTBOMK, there is no way to close it in the child w/o
 * playing a little guessing game.  Just set the pipe descriptors to
 * close-on-exec since that is easier and probably more efficient than
 * playing guessing games.  dup2() clears this flag for the descriptor(s)
 * the child will use.  The other descriptor(s) will close-on-exec w/o
 * needing to guess which one(s) to close.
 */
int
pxpipe(char *pl, int flags, int act)
{
	unsigned piped;
	int pfd[2] = { -1, -1 };
	int redir[2] = { -1, -1 };
	char *lcmd, *p;
 
	for (piped = 0, p = pl, lcmd = pl; ; ++p) {
		if (act == PX_EXEC && spc == -1) {
			/* too many concurrent processes */
			if (pfd[0] != -1)
				close(pfd[0]);
			err(NULL);
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
					return(0); /* unexpected '|' or '^' */
				if (psyntax(lcmd) == 0)
					return(0);
				if (*lcmd == '(')
					if (stripsub(lcmd) == 0)
						return(0);
				piped = 1;
			} else {
				redir[0] = pfd[0];
				if (pipe(pfd) == -1) {
					err("Cannot pipe");
					apstat = 1; /* zombie check */
					return(0);
				}
				fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
				fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
				redir[1] = pfd[1];
				bargs(lcmd, redir, flags | FL_PIPE);
			}
			lcmd = p + 1;
			break;
		case '\0':
			lcmd = de_blank(lcmd);
			if (act == PX_SYN) {
				if (piped && *lcmd == '\0')
					return(0); /* unexpected '\0' */
				if (psyntax(lcmd) == 0)
					return(0);
				if (*lcmd == '(')
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
 * Only called by pxpipe().
 * Strip away the outermost '(   )' subshell and call pxline()
 * to parse the command line contained within.
 * Return 1 on success.
 * Return 0 on failure.
 */
int
stripsub(char *sub)
{
	char *p;

	/* First character must be '('.  pxpipe() ensures this is true. */
	*sub = ' ';
	p = sub + strlen(sub);
	while (*--p != ')')
		;	/* nothing */
	*p = '\0';

	/* Check syntax. */
	if (pxline(sub, PX_SYN) == 0)
		return(0);
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
				return(0); /* redirect: unexpected '<' */
			++r_in;
			continue;
		case '>':
			if (r_out || (r_in && r_in != 2))
				return(0); /* redirect: unexpected '>' */
			if (*(p + 1) == '>')
				++p;
			++r_out;
			continue;
		case ')':
			/* unexpected ')' */
			return(0);
		case '(':
			/* subshell command */
			if (wtmp || word)
				return(0); /* subshell: unexpected '(' */

			if ((p = psynsub(p)) == NULL)
				return(0);
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

			if (sub && word)
				return(0); /* subshell: unexpected word */
			if ((r_in && r_in != 2) || (r_out && r_out != 2))
				return(0); /* redirect: missing file name */
			if (!sub && !word)
				return(0); /* redirect: missing command */
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
 * Return pointer to the character following '( ... )' or NULL
 * if a syntax error has been detected.
 */
char *
psynsub(char *s)
{
	unsigned count, empty;
	char *p, *q;

	for (count = empty = 1, p = s, ++p; ; ++p) {
		p = de_blank(p);
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			if ((p = quote(p)) == NULL)
				return(NULL);
			if (empty)
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
			--count;
			if (count == 0) {
				++p;
				goto subend;
			}
			continue;
		case '\0':
			/* unbalanced '(' */
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
	case '>': case '<':
		if (q == p)
			return(NULL); /* blank-space(s) required here... */
		break;
	default:
		/* garbage or unbalanced ')' */
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
bargs(char *co, int *redir, int flags)
{
	unsigned i;
	char *args[ARGMAX];
	char *p, *larg;

	for (i = 0, p = co, larg = co; ; p++) {
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
			larg = p + 1;
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
 * Determine whether or not command name is a special built-in
 * and return its index value, 0 - 5, to indicate which one it is.
 * Otherwise, return -1 to indicate that it is external.
 */
int
lookup(const char *cmd)
{
	int i;

	for (i = 0; sbi[i]; ++i)
		if (equal(cmd, sbi[i]))
			return(i);
	return(-1);
}

/*
 * Determine whether command is internal or external and execute it
 * as appropriate after manipulating the file descriptors for pipes
 * and I/O redirection.  Keep track of created processes to allow
 * for proper termination reporting.
 */
void
execute(char **args, int *redir, int flags)
{
	pid_t cpid;
	int cmd, i, ract, status;
	char spid[20];
	char *p;

	/* Allow redirect arguments to appear before command name. */
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
		apstat = 1; /* zombie check */
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
			apstat = 1; /* zombie check */
		}

	/* Execute the requested built-in command. */
	switch (cmd) {
	case SBINULL:
		/* the "do-nothing" command */
		retval = 0;
		return;
	case SBICHDIR:
		if (args[1] != NULL) {
			args[1] = substparm(args[1]);
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
		apwait(0);
		return;
	case SBIEXIT:
		/* Terminate command file.
		 * Seek to end-of-file; termination status of shell is that
		 * of the last command executed.
		 */
		lseek(0, (off_t)0, SEEK_END);
		return;
	case SBILOGIN:
		if (!intty) {
			fputs("login: cannot execute\n", stderr);
			retval = 1;
			return;
		}
		/* login, like other built-in commands, cannot be piped. */
		flags &= ~FL_PIPE;
		flags |= FL_EXEC;
		break;
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
		if ((flags & 07) == FL_ASYNC) {
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
			sprintf(spid, "%u\n", cpid);
			write(2, spid, strlen(spid));
			return;
		}

		/* Wait and fetch termination status. */
		for (i = 0; i < spc; ++i)
			waitpid(splist[i], &status, 0);
		if (status) {
			if (WIFSIGNALED(status))
				sigmsg(-1, status);
			else if (WIFEXITED(status))
				retval = WEXITSTATUS(status);
			if (posac && retval >= 0176)
				exit(retval);
		} else
			retval = 0;

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

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	if (flags & FL_ASYNC) {
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if (redir[0] == -1) {
			close(0);
			if (open("/dev/null", O_RDWR) == -1) {
				fputs("/dev/null: cannot open\n", stderr);
				_exit(01);
			}
		}
	}

	if (*args[0] == '(') {
		*args[0] = ' ';
		p = args[0] + strlen(args[0]);
		while (*--p != ')')
			;	/* nothing */
		*p = '\0';
		pxline(args[0], PX_EXEC);
		exit(retval);
	} else {
		if (globargs(args) == NULL)
			_exit(01);
		for (i = 0; args[i]; i++) {
			args[i] = substparm(args[i]);
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
	/* The original osh always used the equivalent of .:/bin:/usr/bin
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
 *	'chdir'		change to HOME directory
 *	'chdir -'	change to previous directory
 *	'chdir dir'	change to 'dir'
 */
void
xchdir(const char *dir)
{
	static int owd1 = -1, owd2 = -1;
	const char *emsg;

	/* XXX: The read bit must be set on directory in order to open() it.
	 *	'chdir -' has no effect when dup() and/or open() fail(s).
	 */
	/* Duplicate fd of old working dir if previous chdir succeeded. */
	if (owd1 != -1) {
		owd2 = dup(owd1);
		close(owd1);
	}
	/* Open current dir; it becomes old working dir if chdir succeeds. */
	owd1 = open(".", O_RDONLY);

	emsg = "chdir: bad directory"; /* default error message */

	if (dir == NULL) { /* chdir to home directory */
		if (home == NULL) {
			emsg = "chdir: no home directory";
			goto chdirerr;
		}
		if (chdir(home) == -1)
			goto chdirerr;
	} else if (equal(dir, "-")) { /* chdir to old working directory */
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
	unsigned i, j, error = 0, append = 0;
	unsigned char strip[ARGMAX];
	char *p, *file;

	for (i = 0; args[i]; ++i) {
		switch (*args[i]) {
		case '<':
			strip[i] = 1;
			p = args[i] + 1;

			/* Get file name; do parameter substitution if $... */
			if (*p == '\0') {
				++i;
				file = args[i] = substparm(args[i]);
				strip[i] = 1;
			} else {
				file = xmalloc(strlen(p) + 1);
				strcpy(file, p);
				file = substparm(file);
				strip[i] += 1;
			}
			striparg(file);

			/* Open file if we've not already; ignore if piped. */
			if (error == 0 && act == 1 && redir[0] == -1) {
				if ((fd = open(file, O_RDONLY)) == -1) {
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
				file = args[i] = substparm(args[i]);
				strip[i] = 1;
			} else {
				file = xmalloc(strlen(p) + 1);
				strcpy(file, p);
				file = substparm(file);
				strip[i] += 1;
			}
			striparg(file);

			/* Open file if we've not already; ignore if piped. */
			if (error == 0 && act == 1 && redir[1] == -1) {
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
	if (error)
		return(NULL);
	return(redir);
}

/*
 * Expand * ? [
 * Return args if successful.
 * Return NULL if error.
 */
char **
globargs(char **args)
{
	unsigned i, j, k;
	glob_t gl;

	for (i = 0; args[i]; i++) {
		switch (glob(args[i], GLFLAGS, NULL, &gl)) {
		case GLOB_NOSPACE:
			fputs("Out of memory\n", stderr);
			exit(1);
		}
		if (gl.gl_pathc == 1) {
			if (!equal(gl.gl_pathv[0], args[i])) {
				args[i] = xrealloc(args[i],
					    strlen(gl.gl_pathv[0]) + 1);
				strcpy(args[i], gl.gl_pathv[0]);
			}
		} else {
			for (j = i; args[j]; j++)
				;	/* nothing */
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				err("Too many args");
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
 * Substitute $... if unquoted.
 * Return s
 */
char *
substparm(char *s)
{
	size_t i;
	unsigned argno, changed = 0;
	char b[LINELEN], num[20];
	char *p, *q;

	for (p = s, i = 0; *p; p++, i++) {
		/* skip quoted characters */
		q = quote(p);
		while (p < q)
			b[i++] = *p++;
		if (*p == '\0')
			break;
		if (*p == '$') {
			changed = 1;
			if (isdigit(*++p)) {
				num[0] = *p;
				num[1] = '\0';
				argno = atoi(num);
				if (argno < posac) {
					if (argno == 0)
						q = cmdfil;
					else
						q = posav[argno];
					while (*q != '\0')
						b[i++] = *q++;
				}
				i--;
			} else if (*p == '$') {
				sprintf(num, "%05u", oshpid);
				q = num;
				while (*q != '\0')
					b[i++] = *q++;
				i--;
			} else
				b[i--] = *p--;
		} else
			b[i] = *p;
	}
	b[i] = '\0';
	if (changed) {
		s = xrealloc(s, i + 1);
		strcpy(s, b);
	}
	return(s);
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
	if (!intty)
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
			p++;
			break;
		case '(':
			if (s < p || (p = psynsub(p)) == NULL)
				return(NULL); /* bad subshell */
			/* FALLTHROUGH */
		default:
			goto quoteend;
		}
	}
quoteend:
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

	for (p = a, i = 0; *p; p++, i++) {
		switch (*p) {
		case '\"':
		case '\'':
			ch = *p++;
			while (*p != ch)
				b[i++] = *p++;
			i--;
			continue;
		case '\\':
			p++;
			/* FALLTHROUGH */
		default:
			b[i] = *p;
		}
	}
	b[i] = '\0';
	/* reassign to argument */
	strcpy(a, b);
	return(a);
}

/*
 * Display messages according to signal s.
 * The V6 sh(1) manual page said that 'Broken Pipe' was reported; however,
 * it actually was not reported.  'Hangup' was not documented but was
 * present in the code.  Signals >= 14 were reported by signal number.
 */
void
sigmsg(pid_t p, int s)
{
	const char *n;
	int signo = WTERMSIG(s);

	retval = signo + 0200;
	switch (signo) {
	/* signals required by POSIX */
	case SIGINT:	r_intr = 1; return;	/* no termination message */
	case SIGHUP:	n = "Hangup";			break;
	case SIGQUIT:	n = "Quit";			break;
	case SIGILL:	n = "Illegal instruction";	break;
	case SIGFPE:	n = "Floating exception";	break;
	case SIGKILL:	n = "Killed";			break;
	case SIGSEGV:	n = "Memory fault";		break;
	case SIGPIPE:	n = "Broken pipe";		break;
#if 0 /* Osh on V7 printed these two messages. */
	case SIGALRM:	n = "Alarm clock";		break;
	case SIGTERM:	n = "Terminated";		break;
#endif
	/* signals not required by POSIX */
#ifdef	SIGTRAP
	case SIGTRAP:	n = "Trace/BPT trap";		break;
#endif
#ifdef	SIGIOT /* old name for the POSIX SIGABRT signal */
	case SIGIOT:	n = "IOT trap";			break;
#endif
#ifdef	SIGEMT
	case SIGEMT:	n = "EMT trap";			break;
#endif
#ifdef	SIGBUS
	case SIGBUS:	n = "Bus error";		break;
#endif
#ifdef	SIGSYS
	case SIGSYS:	n = "Bad system call";		break;
#endif
	default:	n = NULL;
	}
	if (p != -1)
		fprintf(stderr, "%d: ", p);
	if (n == NULL)
		fprintf(stderr, "Sig %d", signo);
	else
		fputs(n, stderr);
	if (WIFCORED(s))
		fputs(" -- Core dumped\n", stderr);
	else
		fputs("\n", stderr);
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

	while ((apid = waitpid(0, &status, opt)) > 0) {
		/* Check if apid is a known asynchronous process. */
		for (i = 0; i < apc; ++i) {
			if (aplist[i] == 0) /* already terminated */
				continue;
			if (aplist[i] == apid) { /* report termination */
				if (status) {
					if (WIFSIGNALED(status))
						sigmsg(apid, status);
					else if (WIFEXITED(status))
						retval = WEXITSTATUS(status);
					if (posac && retval >= 0176)
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
