/*
 * osh - clone of the Sixth Edition (V6) UNIX shell, derived from osh-020214
 *
 * Copyright (c) 2003 Jeffrey Allen Neitzel.  All rights reserved.
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
const char revision[] = "@(#)osh.c	1.91 (jneitzel) 2003/10/14";
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
#include <unistd.h>

#define	ARGMAX		256
#define	LINELEN		1024
#ifdef	PATH_MAX
#define	PATHMAX		PATH_MAX
#else
#define	PATHMAX		1024
#endif	/* PATH_MAX */
#define	PLMAX		64	/* initial maximum # of processes in plist */

/* flags */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

#ifndef	WIFCORED
#define	WIFCORED(s)	((s) & 0200)
#endif

#define	equal(a, b)	(strcmp(a, b) == 0)	/* compare strings */

#ifndef	CLONE
char		*home;		/* user's home directory */
#endif
int		input;		/* input file */
unsigned	interactive;	/* if shell reads from stdin */
uid_t		osheuid;	/* effective uid of shell */
pid_t		oshpid;		/* pid of shell */
int		pcount;		/* # of processes resulting from command line */
int		plim;		/* current maximum # of processes in plist */
pid_t		*plist;		/* array of process IDs to wait for */
int		posac;		/* count of positional arguments */
char		**posav;	/* positional arguments */
char		retval;		/* return value for shell */

void	aswait(int);
void	comtype(char **, unsigned, int *, int);
void	ecmd(char **, unsigned, int *, int);
int	*getredir(char **, unsigned, int *);
char	**globargs(char **);
void	oshinit(int);
void	pcmd(char *, int *, int);
void	pline(char *);
void	ppipe(char *, int);
void	quit(int);
char	*quote(char *);
char	*readline(char *, size_t, int);
void	sherror(void);
void	sigmsg(int);
char	*striparg(char *);
char	*substvars(char *);
void	syntax(void);
#ifndef	CLONE
void	xchdir(char **);
#endif
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);

int
main(int argc, char **argv)
{
	char line[LINELEN];
	char *pr;
	int ch;
	off_t ioff;

	oshinit(1);
	if (argc >= 2 && *argv[1] == '-') {
		switch (*(argv[1] + 1)) {
		case 't':
			input = 0;
			pr = readline(line, LINELEN, input);
			if (pr == NULL)
				quit(0);;
			if (pr == (char *)-1)
				quit(1);
			pline(line);
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
			pline(pr);
			return(retval);
		default: /* silly, but compatible */
			argc = 1;
			input = 0;
			goto mainloop;
		}
	}
	if (argc == 1) { /* interactive, or input from pipe or '<file' */
		input = 0;
		if (isatty(0) && isatty(1) && isatty(2)) {
			interactive = 1;
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
		}
	} else {
		close(0);
		input = open(argv[1], O_RDONLY);
		if (input == -1) {
			fprintf(stderr, "%s: cannot open\n", argv[1]);
			quit(1);
		}
		/* Ignore possible '#!' line at beginning of command file. */
		ioff = 0;
		if (getchar() == '#' && getchar() == '!') {
			ioff += 2;
			do {
				ch = getchar();
				++ioff;
			} while (ch != '\n' && ch != EOF);
			if (ch == EOF)
				quit(0);
		}
		if (lseek(0, ioff, SEEK_SET) == -1) {
			if (errno == ESPIPE)
				fputs("Illegal seek\n", stderr);
			quit(1);
		}
		posav = &argv[1];
		posac = argc - 1;
	}
mainloop:
	for (;;) {
mainext:
		if (interactive) {
			if (osheuid == 0)
				write(2, "# ", 2);
			else
				write(2, "% ", 2);
		}
		/* XXX: command line overflow handling is less than ideal */
		pr = readline(line, LINELEN, input);
		if (pr == NULL)
			break;
		if (pr == (char *)-1)
			continue;
		while (*(pr - 1) == '\\' && (pr - 2) >= line
			&& *(pr - 2) != '\\') {
			*(pr - 1) = ' ';
			pr = readline(pr, (size_t)LINELEN - (pr - line), input);
			if (pr == NULL)
				break;
			if (pr == (char *)-1)
				goto mainext;
		}
		pline(line);
	}
	quit(retval);
	/* NOTREACHED */
	return(0);
}

/*
 * osh initialization/termination
 * action 1 is for initialization
 * action 0 is for termination
 */
void
oshinit(int action)
{
#ifndef	CLONE
	char *str;
	size_t len;
#endif

	if (action) {
		osheuid = geteuid();
		oshpid = getpid();

#ifndef	CLONE
		/* Get user's home directory for use in xchdir().
		 * We don't check if it's a symlink to a too long directory.
		 * Just let chdir() fail w/ ENAMETOOLONG in that case.
		 */
		if ((str = getenv("HOME")) != NULL && *str != '\0' &&
		    (len = strlen(str)) < PATHMAX) {
			home = xmalloc(len + 1);
			strcpy(home, str);
		} else
			home = NULL;
#endif	/* !CLONE */

		/* Allocate memory for plist array; can reallocate in ecmd(). */
		plim = PLMAX;
		plist = xmalloc(plim * sizeof(pid_t));
	} else {
#ifndef	CLONE
		if (home)
			free(home);
#endif	/* !CLONE */
		if (plist)
			free(plist);
	}
}

void
quit(int code)
{
	if (getpid() == oshpid)
		oshinit(0);
	exit(code);
}

/* all errors detected by the shell */
void
sherror()
{
	if (!interactive)
		quit(1);
	retval = 1;
}

/* syntax errors */
void
syntax()
{
	fputs("syntax error\n", stderr);
	sherror();
}

void *
xmalloc(size_t s)
{
	void *mp;

	if ((mp = malloc(s)) == NULL) {
		fputs("Cannot malloc\n", stderr);
		quit(1);
	}
	return(mp);
}

void *
xrealloc(void *rp, size_t s)
{
	if ((rp = realloc(rp, s)) == NULL) {
		fputs("Cannot realloc\n", stderr);
		quit(1);
	}
	return(rp);
}

/*
 * return the next unquoted character
 * return NULL at syntax error
 */
char *
quote(char *s)
{
	char *p;
	unsigned count;

	for (p = s; *p != '\0'; p++) {
		switch (*p) {
		case '\"':
			for (p++;; p++) {
				if (*p == '\"')
					break;
				if (*p == '\\')
					p++;
				if (*p == '\0')
					return(NULL);
			}
			break;
		case '\'':
			for (p++;; p++) {
				if (*p == '\'')
					break;
				if (*p == '\\')
					p++;
				if (*p == '\0')
					return(NULL);
			}
			break;
		case '(':
			for (p++, count = 0;; p++) {
				if (*p == '(') {
					count++;
					continue;
				}
				if (*p == ')') {
					if (count == 0)
						break;
					count--;
				}
				if (*p == '\\')
					p++;
				if (*p == '\0')
					return(NULL);
			}
			break;
		case '\\':
			p++;
			break;
		default:
			goto quoteend;
		}
	}
quoteend:
	return(p);
}

/*
 * expand * ? [
 * return args if successful
 * return NULL if error
 */
char **
globargs(char **args)
{
	unsigned i, j, k;
	glob_t gl;

	for (i = 0; args[i]; i++) {
		switch (glob(args[i], GLOB_ERR | GLOB_NOCHECK, NULL, &gl)) {
		case GLOB_NOSPACE:
			fputs("Out of memory\n", stderr);
			quit(1);
		}
		if (gl.gl_pathc == 1) {
			if (!equal(gl.gl_pathv[0], args[i])) {
				free(args[i]);
				args[i] = xmalloc(strlen(gl.gl_pathv[0]) + 1);
				strcpy(args[i], gl.gl_pathv[0]);
			}
		} else {
			for (j = i; args[j]; j++);
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				fputs("Too many args\n", stderr);
				sherror();
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
 * substitute $...
 * return s if successful
 * return NULL at error
 */
char *
substvars(char *s)
{
	int argno;
	unsigned i, changed = 0;
	char b[LINELEN], num[20];
	char *p, *q;

	for (p = s, i = 0; *p; p++, i++) {
		q = quote(p);
		while (p < q)
			b[i++] = *p++;
		if (*p == '\0') {
			i--;
			break;
		}
		if (*p == '$') {
			changed = 1;
#if 0	/* this is what UNIX 6th Ed. manual page sh(1) writes */
			if (isdigit(*++p) && !interactive) {
#else
			if (isdigit(*++p)) {
#endif
				num[0] = *p;
				num[1] = '\0';
				argno = atoi(num);
				if (argno < posac) {
					q = posav[argno];
					while (*q != '\0')
						b[i++] = *q++;
				}
				i--;
			} else {
				if (*p == '$') {
					sprintf(num, "%05u", oshpid);
					q = num;
					while (*q != '\0')
						b[i++] = *q++;
					i--;
				} else
					b[i--] = *p--;
			}
		} else
			b[i] = *p;
	}
	b[i] = '\0';
	if (changed) {
		/* reallocate only when result of $... is longer than s */
		if (i > strlen(s))
			s = xrealloc(s, i + 1);
		strcpy(s, b);
	}
	return(s);
}

/*
 * remove " ' \
 * because a previous call of 'quote' has done this,
 * no syntax checking is done here.
 * returns a
 */
char *
striparg(char *a)
{
	register unsigned i;
	register int c;
	char b[LINELEN];
	register char *p;

	for (p = a, i = 0; *p; p++, i++) {
		c = *p & 0377;
		switch (c) {
		case '\"':
		case '\'':
			p++;
			while (*p != c) {
				if (*p == '\\' && *(p + 1) == c)
					p++;
				b[i++] = *p++;
			}
			i--;
			continue;
		case '\\':
			c = *++p;
			/* FALLTHROUGH */
		default:
			b[i] = c;
		}
	}
	b[i] = 0;
	/* reassign to argument */
	strcpy(a, b);
	return(a);
}

/*
 * Display messages according to signal s.
 * Some of these messages were not documented in the V6 sh(1) manual page
 * but were present in the code.  Moreover, some were added and/or changed
 * in the move from V6 to V7.  The messages here are basically the same as
 * those from osh in V7.
 */
void
sigmsg(int s)
{
	const char *n;
	int signo = WTERMSIG(s);
	
	switch (signo) {
	/* signals required by POSIX */
	case SIGINT:	return;	/* is not being reported */
	case SIGHUP:	n = "Hangup";			break;
	case SIGQUIT:	n = "Quit";			break;
	case SIGILL:	n = "Illegal instruction";	break;
	case SIGFPE:	n = "Floating exception";	break;
	case SIGKILL:	n = "Killed";			break;
	case SIGSEGV:	n = "Memory violation";		break;
	case SIGPIPE:	n = "Broken Pipe";		break;
	case SIGALRM:	n = "Alarm clock";		break;
	case SIGTERM:	n = "Terminated";		break;
	/* signals not required by POSIX */
#ifdef	SIGIOT
	/* SIGIOT is the old name for SIGABRT. */
	case SIGIOT:	n = "IOT trap";			break;
#endif
#ifdef	SIGTRAP
	case SIGTRAP:	n = "Trace/BPT trap";		break;
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
	if (n == NULL)
		fprintf(stderr, "Sig %d", signo);
	else
		fputs(n, stderr);
	if (WIFCORED(s))
		fputs(" -- Core dumped\n", stderr);
	else
		fputs("\n", stderr);
	retval = signo + 0200;
}

/*
 * Report termination status of async commands if available.
 * Match the behaviour documented in the V6 sh(1) manual page that says:
 * When commands followed by `&' terminate, a termination report is given
 * upon receipt of the first subsequent command or when a wait is executed.
 */
void
aswait(int opt)
{
	int status;

	while (waitpid(0, &status, opt) > 0) {
		if (status) {
			if (WIFSIGNALED(status))
				sigmsg(status);
			else if (WIFEXITED(status))
				retval = WEXITSTATUS(status);
		} else
			retval = 0;
	}
}

/*
 * get redirection info from args and strip it away.
 * return file descriptors
 * return NULL at error
 */
int *
getredir(char **args, unsigned ac, int *redir)
{
	int fd;
	unsigned i, j, error = 0, append = 0;
	unsigned *strip;
	char *p, *file;

	/* allocate memory for strip[] array */
	strip = xmalloc(ac * sizeof(unsigned));

	for (i = 0; args[i]; i++) {
		switch (*args[i]) {
		case '<':
			strip[i] = 1;
			p = args[i] + 1;
			if (*p == '\0') {
				file = args[++i];
				strip[i] = 1;
			} else
				file = p;
			/* if file is NULL give syntax error. */
			if (file == NULL) {
				fputs("syntax error\n", stderr);
				error = 1;
				goto redirend;
			}
			striparg(file);
			/* open file if we've not already; ignore if piped. */
			if (redir[0] == -1) {
				if ((fd = open(file, O_RDONLY)) == -1) {
					fprintf(stderr, "%s: cannot open\n",
						file);
					error = 1;
				} else
					redir[0] = fd;
			}
			break;
		case '>':
			strip[i] = 1;
			if (*(args[i] + 1) == '>') {
				append = 1;
				p = args[i] + 2;
			} else
				p = args[i] + 1;
			if (*p == '\0') {
				file = args[++i];
				strip[i] = 1;
			} else
				file = p;
			/* if file is NULL give syntax error. */
			if (file == NULL) {
				fputs("syntax error\n", stderr);
				error = 1;
				goto redirend;
			}
			striparg(file);
			/* open file if we've not already; ignore if piped. */
			if (redir[1] == -1) {
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
			break;
		default:
			strip[i] = 0;
		}
	}
redirend:
	/*
	 * Free memory used by, and strip away, redirection arguments.
	 * Values in strip[] array:
	 *	1 - flags redirections to free and strip away.
	 *	0 - flags regular (non-redirection) arguments.
	 */
	for (i = 0, j = 0; i < ac; i++) {
		while (args[i] && strip[i] == 1) {
			free(args[i]);
			args[i] = NULL;
			i++;
		}
		if (args[i] && i < ac)
			args[j++] = args[i];
	}
	args[j] = NULL;
	if (strip) {
		free(strip);
		strip = NULL;
	}
	if (error)
		return(NULL);
	return(redir);
}

/* fork and execute a command */
void
ecmd(char **args, unsigned ac, int *redir, int flags)
{
	int i, status;
	pid_t cpid;
	char spid[20];
	char *p;

	if (getredir(args, ac, redir) == NULL) {
		sherror();
		return;
	}
	if (*args[0] == '(' && args[1]) {
		syntax();
		goto commanderr;
	}
	if (flags & FL_EXEC)
		cpid = 0;
	else
		cpid = fork();
	switch (cpid) {
	case -1:
		/* User has too many processes. */
		fputs("Cannot fork\n", stderr);

		/* Flag end of the list. */
		plist[pcount] = -1;

		/* Stop all command line parsing; see ppipe(). */
		pcount = -1;

		/* Exorcise all the evil little zombies ;) */
		for (i = 0; plist[i] != -1; i++)
			if (plist[i] > 0)
				waitpid(plist[i], NULL, 0);
		sherror();
		goto commanderr;
	case 0:
		break;
	default:
		/* this is parent */
		for (i = 0; i < 2; i++) {
			if (redir[i] != -1)
				close(redir[i]);
		}

		/* Track processes so we know which ones we have to wait for. */
		if (flags & FL_ASYNC)
			plist[pcount] = 0;
		else
			plist[pcount] = cpid;
		if (++pcount > plim - 1) {
			plist = xrealloc(plist, (plim + PLMAX) * sizeof(pid_t));
			plim += PLMAX;
		}

		if (flags & FL_PIPE)
			return;

		if (flags & FL_ASYNC) {
			sprintf(spid, "%u\n", cpid);
			write(2, spid, strlen(spid));
			return;
		}

		for (i = 0; i < pcount; i++)
			if (plist[i] > 0)
				if (waitpid(plist[i], &status, 0) == plist[i])
					plist[i] = 0;
		if (status) {
			if (WIFSIGNALED(status))
				sigmsg(status);
			else if (WIFEXITED(status))
				retval = WEXITSTATUS(status);
		} else
			retval = 0;

		return;
	}
	/* this is child only */
	/* redirections */
	for (i = 0; i < 2; i++) {
		if (redir[i] != -1) {
			if (dup2(redir[i], i) == -1) {
				fprintf(stderr, "Cannot redirect %d\n", i);
				_exit(0100);
			}
			close(redir[i]);
		}
	}
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	if (flags & FL_ASYNC) {
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if (redir[0] == -1) {
			close(0);
			open("/dev/null", O_RDWR);
		}
	}
	if (*args[0] != '(') {
		if (globargs(args) == NULL)
			_exit(0176);
		for (i = 0; args[i]; i++) {
			args[i] = substvars(args[i]);
			striparg(args[i]);
		}
	} else {
		*args[0] = ' ';
		p = args[0] + strlen(args[0]);
		while (*p != ')')
			p--;
		*p = '\0';
		pline(args[0]);
		quit(retval);
	}
#ifdef	CLONE
	/* Try current directory first. */
	execv(args[0], args);
	if (errno == ENOENT)
		/* Try $PATH. In original osh, this was always /bin:/usr/bin */
		execvp(args[0], args);
#else
	/* Search $PATH only; try current directory only if it's in $PATH. */
	execvp(args[0], args);
#endif
	if (errno == ENOENT || errno == ENOTDIR) {
		fprintf(stderr, "%s: not found\n", args[0]);
		_exit(0177);
	} else {
		fprintf(stderr, "%s: cannot execute\n", args[0]);
		_exit(0176);
	}
	/* this is parent again */
commanderr:
	for (i = 0; i < 2; i++)
		if (redir[i] != -1)
			close(redir[i]);
}

#ifndef	CLONE
/*
 * Change shell's working directory.
 * Get fd for current directory before calling chdir() or fchdir().
 * Close unused fd afterwards.  Dup and close appropriate fd on next
 * call to this function and open new fd for current directory.
 *
 *	'chdir'		change to HOME directory
 *	'chdir -'	change to previous directory
 *	'chdir dir'	change to 'dir'
 */
void
xchdir(char **args)
{
	static int fail = 0, owd1 = -1, owd2 = -1;

	/* duplicate fd of old working dir if previous chdir succeeded */
	if (!fail && owd1 > 0) {
		if ((owd2 = dup(owd1)) == -1) {
			fprintf(stderr, "chdir: cannot duplicate %d\n", owd1);
			sherror();
			return;
		}
		close(owd1);
	}
	/* open current dir, becomes old working dir after successful chdir */
	if ((owd1 = open(".", O_RDONLY)) == -1) {
		fputs("chdir: cannot open current directory\n", stderr);
		retval = 1;
	}

	/* chdir to home directory */
	if (!args[1]) {
		if (home) {
			if (chdir(home) == -1) {
				fputs("chdir: bad home directory\n", stderr);
				goto chdirerr;
			} else
				goto chdirend;
		} else {
			fputs("chdir: no home directory\n", stderr);
			goto chdirerr;
		}
	} else if (equal(args[1], "-")) { /* chdir to old working directory */
		if (owd2 > 0) {
			if (fchdir(owd2) == -1) {
				fputs("chdir: bad old directory\n", stderr);
				goto chdirerr;
			} else
				goto chdirend;
		} else {
			fputs("chdir: no old directory\n", stderr);
			goto chdirerr;
		}
	} else { /* chdir to any other directory */
		globargs(args);
		args[1] = substvars(args[1]);
		striparg(args[1]);
		if (chdir(args[1]) == -1) {
			fputs("chdir: bad directory\n", stderr);
			goto chdirerr;
		} else
			goto chdirend;
	}
chdirend:
	fail = 0;
	if (owd2 > 0)
		owd2 = close(owd2);
	/* set close-on-exec flag */
	if (owd1 > 0)
		fcntl(owd1, F_SETFD, FD_CLOEXEC);
	retval = 0;
	return;
chdirerr:
	fail = 1;
	if (owd1 > 0)
		owd1 = close(owd1);
	/* set close-on-exec flag if it's not already */
	if (owd2 > 0 && (fcntl(owd2, F_GETFD) & FD_CLOEXEC) == 0)
		fcntl(owd2, F_SETFD, FD_CLOEXEC);
	sherror();
}
#endif	/* !CLONE */

/* distinguish between internal, external and special commands */
void
comtype(char **args, unsigned ac, int *redir, int flags)
{
	/* Check for and report on terminated asynchronous commands. */
	if (pcount == 0 && !equal(args[0], "wait"))
		aswait(WNOHANG);

	if (equal(args[0], ":")) {
		retval = 0;
		return;
	}
#ifdef	CLONE
	if (equal(args[0], "chdir")) {
		if (args[1] == NULL) {
			fputs("chdir: arg count\n", stderr);
			sherror();
			return;
		}
		globargs(args);
		args[1] = substvars(args[1]);
		striparg(args[1]);
		if (chdir(args[1]) == -1) {
			fputs("chdir: bad directory\n", stderr);
			sherror();
		} else
			retval = 0;
		return;
	}
#else
	if (equal(args[0], "chdir")) {
		xchdir(args);
		return;
	}
#endif
	if (equal(args[0], "shift")) {
		if (posac <= 1) {
			fputs("shift: no args\n", stderr);
			retval = 1;
			return;
		}
		posav = &posav[1];
		posac--;
		retval = 0;
		return;
	}
	if (equal(args[0], "wait")) {
		aswait(0);
		return;
	}
	if (equal(args[0], "login")) {
		if (!interactive) {
			fputs("login: cannot execute\n", stderr);
			retval = 1;
			return;
		}
		flags |= FL_EXEC;
	}
	/* V6 Unix had an external exit utility, /bin/exit, used to terminate
	 * command files.  Use a built-in exit since modern systems no longer
	 * have it as an external utility.
	 */
	if (equal(args[0], "exit")) {
		/* Only terminates command files, not interactive shells. */
		if (lseek(0, (off_t)0, SEEK_END) == -1) {
			if (errno == ESPIPE)
				fputs("Illegal seek\n", stderr);
			sherror();
			return;
		}
		return;
	}
	ecmd(args, ac, redir, flags);
}

/*
 * parse a command.
 * break it down into args
 */
void
pcmd(char *co, int *redir, int flags)
{
	unsigned i, maxerr = 0;
	char *args[ARGMAX];
	char *p, *lastarg;

	for (i = 0, p = co, lastarg = co; ; p++) {
		/* redirect w/o command is not supported */
		if (i == 0 && lastarg != p &&
		    (*lastarg == '<' || *lastarg == '>'))
			goto parserr;
		if ((i != 0 || lastarg != p) && *p == '(')
			goto parserr;
		if ((p = quote(p)) == NULL)
			goto parserr;
		switch (*p) {
		case ')':
			if (i != 0 || p == co || *(p - 1) == '\0' ||
			    *lastarg != '(')
				goto parserr;
			/* FALLTHROUGH */
		case ' ':
		case '\t':
			*p = '\0';
			/* ignore whitespace at beginning */
			if (i == 0 && (p == co || *(p - 1) == '\0')) {
				lastarg++;
				continue;
			}
			if (*(p - 1) != '\0') {
				/* error if too many args */
				if (i >= ARGMAX - 1) {
					maxerr = 1;
					goto parserr;
				}
				args[i] = xmalloc(strlen(lastarg) + 1);
				strcpy(args[i], lastarg);
				++i;
			}
			lastarg = p + 1;
			break;
		case '\0':
			if (*(p - 1) != '\0') {
				/* error if too many args */
				if (i >= ARGMAX - 1) {
					maxerr = 1;
					goto parserr;
				}
				args[i] = xmalloc(strlen(lastarg) + 1);
				strcpy(args[i], lastarg);
				++i;
			}
			lastarg = p + 1;
			goto parsend;
		}
	}
parsend:
	args[i] = NULL;
	/* Ignore empty command lines. */
	if (args[0] != NULL)
		comtype(args, i, redir, flags);
	for (i = 0; args[i]; i++) {
		free(args[i]);
		args[i] = NULL;
	}
	return;
parserr:
	while (i != 0) {
		--i;
		free(args[i]);
		args[i] = NULL;
	}
	if (maxerr) {
		fputs("Too many args\n", stderr);
		sherror();
	} else
		syntax();
}

/* split a pipe */
void
ppipe(char *r, int flags)
{
	char *p, *lastcmd;
	int redir[2] = { -1, -1 };
	int pfd[2] = { -1, -1 };

	for (p = r, lastcmd = r; ; p++) {
		if ((p = quote(p)) == NULL) {
			if (pfd[0] != -1)
				close(pfd[0]);
			syntax();
			return;
		}
		switch (*p) {
		case '|':
		case '^':
			redir[0] = pfd[0];
			if (p == r) {
				syntax();
				return;
			}
			*p = '\0';
			if (pcount == -1) {
				/* too many processes */
				sherror();
				return;
			}
			if (pipe(pfd) == -1) {
				fputs("Cannot pipe", stderr);
				sherror();
				return;
			}
			redir[1] = pfd[1];
#if 0
			if (flags & FL_ASYNC)
				flags &= ~FL_ASYNC;
#endif
			pcmd(lastcmd, redir, flags | FL_PIPE);
			lastcmd = p + 1;
			break;
		case '\0':
			redir[0] = pfd[0];
			redir[1] = -1;
			if (*lastcmd != '\0') {
				if (pcount == -1) {
					/* too many processes */
					sherror();
					return;
				}
				pcmd(lastcmd, redir, flags);
			}
			return;
		}
	}
}

/*
 * parse an entire given line.
 * break it down into commands.
 */
void
pline(char *l)
{
	char *p, *lastpipe;

	/* reset process count */
	pcount = 0;

	for (p = l, lastpipe = l; ; p++) {
		if ((p = quote(p)) == NULL) {
			syntax();
			return;
		}
		switch (*p) {
		/* no syntax checking is done here */
		case ';':
			*p = '\0';
			ppipe(lastpipe, 0);
			lastpipe = p + 1;
			break;
		case '&':
			*p = '\0';
			ppipe(lastpipe, FL_ASYNC);
			lastpipe = p + 1;
			break;
		case '\0':
			if (*lastpipe != '\0')
				ppipe(lastpipe, 0);
			return;
		}
	}
}

/*
 * read a line from fd, with error checking
 * quit if EBADF or overflow and not interactive
 * return the end of string if success
 * return NULL if EOF
 * return -1 if error
 */
char *
readline(char *b, size_t len, int fd)
{
	unsigned i;
	char *p, *end;

	end = b + len;
	for (p = b, i = 0; p < end; p++) {
		switch (read(fd, p, 1)) {
		case -1:
			/* avoid infinite loop in main() if we lose input */
			if (errno == EBADF) {
				fputs("Cannot read input\n", stderr);
				quit(1);
			}
			/* some other error */
			return((char *)-1);
		case 0:
			if (p > b) {
				*p = '\0';
				return(p);
			}
			return(NULL);
		}
		++i;
		if (*p == '\n') {
			*p = '\0';
			return(p);
		}
	}
	/* XXX: still need to revisit the command line overflow situation */
	fputs("Command line overflow\n", stderr);
	if (!interactive)
		quit(1);
	return((char *)-1);
}

