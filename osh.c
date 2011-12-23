/*
 * osh - clone of the Sixth Edition (V6) UNIX shell
 *
 * Modified by Jeffrey Allen Neitzel, July 2003.
 *
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
 */

#ident	"@(#)osh.c	1.6 (gritter) 2/14/02>"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>

#define	LINELEN		1024
#define	ARGMAX		256
#ifndef	MAXPATHLEN
#define	MAXPATHLEN	1024
#endif

/* flags */
#define	FL_ASYNC	01
#define	FL_PIPE		02
#define	FL_EXEC		04

#ifndef	WIFCORED
#define	WIFCORED(s)	((s) & 0200)
#endif

char		retval;		/* return value for shell */
unsigned	interactive;	/* if shell reads from stdin */
uid_t		myuid;		/* uid */
pid_t		mypid;		/* pid of shell */
char		**posav;	/* positional arguments */
int		posac;		/* count of positional arguments */
int		input;		/* input file */

void	pline(char *l);
void	quit(int code);
char	*smalloc(size_t s);
void	sherror(void);
void	syntax(void);
char	*quote(char *s);
char	**globargs(char **args);
char	*substvars(char *s);
char	*striparg(char *a);
int	*getredir(char **args, int ac, int *redir);
void	sigmsg(int s);
void	ecmd(char **args, int ac, int *redir, int flags);
void	comtype(char **args, int ac, int *redir, int flags);
void	pcmd(char *co, int *redir, int flags);
void	ppipe(char *r, int flags);
char	*readline(char *b, size_t len, int fd);

void
quit(int code)
{
	exit(code);
}

char *
smalloc(size_t s)
{
	char *m;

	if ((m = (char *)malloc(s)) == NULL) {
		fputs("Cannot malloc\n", stderr);
		quit(1);
	}
	return m;
}

void
sherror()
/* all errors detected by the shell */
{
	if (!interactive)
		quit(1);
	retval = 1;
}

void
syntax()
/* syntax errors */
{
	fputs("syntax error\n", stderr);
	sherror();
}

char *
quote(char *s)
/* return the next unquoted character
 * return NULL at syntax error
 */
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
					return NULL;
			}
			break;
		case '\'':
			for (p++;; p++) {
				if (*p == '\'')
					break;
				if (*p == '\\')
					p++;
				if (*p == '\0')
					return NULL;
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
					return NULL;
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
	return p;
}

char **
globargs(char **args)
/* expand * ? [
 * return args if successful
 * return NULL if error
 */
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
			if (strcmp(gl.gl_pathv[0], args[i])) {
				free(args[i]);
				args[i] = smalloc(strlen(gl.gl_pathv[0]) + 1);
				strcpy(args[i], gl.gl_pathv[0]);
			}
		} else {
			for (j = i; args[j]; j++);
			if (j + gl.gl_pathc - 1 >= ARGMAX) {
				fputs("Too many args\n", stderr);
				sherror();
				return NULL;
			}
			for ( ; j > i; j--)
				args[j + gl.gl_pathc - 1] = args[j];
			free(args[i]);
			for (j = i + gl.gl_pathc, k = 0; i < j; i++, k++) {
				args[i] = smalloc(strlen(gl.gl_pathv[k]) + 1);
				strcpy(args[i], gl.gl_pathv[k]);
			}
			i--;
		}
		globfree(&gl);
	}
	return args;
}

char *
substvars(char *s)
/* substitute $...
 * return s if successful
 * return NULL at errors
 */
{
	unsigned i, argno, changed = 0;
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
					sprintf(num, "%05u", mypid);
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
		free(s);
		s = smalloc(i + 1);
		strcpy(s, b);
	}
	return s;
}

char *
striparg(char *a)
/* XXX: if a is a NULL pointer this function will fail.
 * remove " ' \
 * because a previous call of 'quote' has done this,
 * no syntax checking is done here.
 * returns a
 */
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
			/*FALLTHROUGH*/
		default:
			b[i] = c;
		}
	}
	b[i] = 0;
	/* reassign to argument */
	strcpy(a, b);
	return a;
}

int *
getredir(char **args, int ac, int *redir)
/* get redirection info from args and strip it away.
 * return file descriptors
 * return NULL at error
 */
{
	unsigned i, j, fd, error = 0, append = 0;
	char *p, *file;
	int strip[ac]; /* 1 indicates redirections to free and strip away.
			* 0 indicates regular (non-redirection) arguments. */

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
			/* striparg() will fail if we pass it a NULL pointer.
			 * if file is NULL break and give syntax error.
			 */
			if (file == NULL) {
				/* assign NULL to next arg. */
				args[i + 1] = NULL;
				fputs("syntax error\n", stderr);
				error = 1;
				break;
			}
			striparg(file);
			/* open file (if not already), or ignore if piped */
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
			/* striparg() will fail if we pass it a NULL pointer.
			 * if file is NULL break and give syntax error.
			 */
			if (file == NULL) {
				/* assign NULL to next arg. */
				args[i + 1] = NULL;
				fputs("syntax error\n", stderr);
				error = 1;
				break;
			}
			striparg(file);
			/* open file (if not already), or ignore if piped */
			if (redir[1] == -1) {
				if (append)
					fd = open(file, O_WRONLY | O_APPEND);
				else
					fd = open(file,
						O_CREAT | O_TRUNC | O_WRONLY,
						0666);
				if (fd == -1) {
					fprintf(stderr, "%s: cannot open\n",
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
	/* free memory used by redirection arguments, and strip
	 * those args away from the list.
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
	if (error)
		return NULL;
	return redir;
}

void
sigmsg(int s)
/* display messages according to signal s */
{
	char *n;
	int signo = WTERMSIG(s);
	
	switch (signo) {
#ifdef	SIGBUS
	case SIGBUS:	n = "Bus error";		break;
#endif
#ifdef	SIGTRAP
	case SIGTRAP:	n = "Trace/BPT trap";		break;
#endif
#ifdef	SIGIOT
	case SIGIOT:	n = "IOT trap";			break;
#endif
#ifdef	SIGEMT
	case SIGEMT:	n = "EMT trap";			break;
#endif
#ifdef	SIGSYS
	case SIGSYS:	n = "Bad system call";		break;
#endif
	/* the following are POSIX signals */
	case SIGTERM:	/* these two are not being reported */
	case SIGINT:	return;
	case SIGILL:	n = "Illegal instruction";	break;
	case SIGQUIT:	n = "Quit";			break;
	case SIGFPE:	n = "Floating exception";	break;
	case SIGSEGV:	n = "Memory violation";		break;
	case SIGKILL:	n = "Killed";			break;
	case SIGPIPE:	n = "Broken Pipe";		break;
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

void
ecmd(char **args, int ac, int *redir, int flags)
/* fork and execute a command */
{
	int i, status;
	pid_t cpid;
	char spid[20];
	char *p;

	/* If getredir() returns NULL non-interactive shells terminate,
	 * but interactive shells do not.
	 */
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
		fputs("Cannot fork\n", stderr);
		sherror();
		goto commanderr;
	case 0:
		break;
	default:
		for (i = 0; i < 2; i++) {
			if (redir[i] != -1)
				close(redir[i]);
		}
		if (flags & FL_PIPE)
			return;
		if (flags & FL_ASYNC) {
			sprintf(spid, "%u\n", cpid);
			write(1, spid, strlen(spid));
			return;
		}
		while (wait(&status) != cpid);
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
	/* try current directory first */
	execv(args[0], args);
	if (errno == ENOENT)
		/* try $PATH. in original osh, this was always /bin:/usr/bin */
		execvp(args[0], args);
#else
	/* search $PATH only; try current directory only if it's in $PATH */
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

void
comtype(char **args, int ac, int *redir, int flags)
/* distinguish between internal, external and special commands */
{
	int status;

	if (!strcmp(args[0], "chdir")) {
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
	if (!strcmp(args[0], ":")) {
		retval = 0;
		return;
	}
	if (!strcmp(args[0], "wait")) {
		while (wait(&status) != -1) {
			if (status) {
				if (WIFSIGNALED(status))
					sigmsg(status);
				else if (WIFEXITED(status))
					retval = WEXITSTATUS(status);
			} else
				retval = 0;
		}
		return;
	}
	if (!strcmp(args[0], "shift")) {
		if (posac <= 1) {
			retval = 1;
			return;
		}
		posav = &posav[1];
		posac--;
		retval = 0;
		return;
	}
	if (!strcmp(args[0], "login"))
		flags |= FL_EXEC;
	ecmd(args, ac, redir, flags);
}
		
void
pcmd(char *co, int *redir, int flags)
/* parse a command.
 * break it down into args
 */
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
			if (i != 0 || p == co || *(p - 1) == '\0'
				|| *lastarg != '(')
				goto parserr;
			/*FALLTHROUGH*/
		case ' ':
		case '\t':
			*p = '\0';
			/* ignore whitespace at beginning */
			if (i == 0 && (p == co || *(p - 1) == '\0')) {
				lastarg++;
				continue;
			}
			if (*(p - 1) != '\0') {
				/* we will have too many args.  give error. */
				if (i + 1 >= ARGMAX) {
					maxerr = 1;
					goto parserr;
				}
				args[i] = smalloc(strlen(lastarg) + 1);
				strcpy(args[i++], lastarg);
			}
			lastarg = p + 1;
			break;
		case '\0':
			if (*(p - 1) != '\0') {
				/* we will have too many args.  give error. */
				if (i + 1 >= ARGMAX) {
					maxerr = 1;
					goto parserr;
				}
				args[i] = smalloc(strlen(lastarg) + 1);
				strcpy(args[i++], lastarg);
			}
			lastarg = p + 1;
			goto parsend;
		}
	}
parsend:
	args[i] = NULL;
	/* Ignore empty commandlines.
	 * If commandline contains only whitespace args[0] will be NULL.
	 * Only call comtype() when args[0] != NULL.  If no command,
	 * nothing to do.  New prompt in main(); continue as normal.
	 */
	if (args[0] != NULL)
		comtype(args, i, redir, flags);
	for (i = 0; args[i]; i++) {
		free(args[i]);
		args[i] = NULL;
	}
	return;
parserr:
	while (i != 0) {
		free(args[--i]);
		args[i] = NULL;
	}
	/* XXX: need for maxerr seems a little redundant? */
	if (maxerr) {
		fputs("Too many args\n", stderr);
		sherror();
	} else
		syntax();
	return;
}

void
ppipe(char *r, int flags)
/* split a pipe */
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
			if (*lastcmd != '\0')
				pcmd(lastcmd, redir, flags);
			return;
		}
	}
}

void
pline(char *l)
/* parse an entire given line.
 * break it down into commands.
 */
{
	char *p, *lastpipe;

	for (p = l, lastpipe = l; ; p++) {
		if ((p = quote(p)) == NULL) {
			syntax();
			return;
		}
		switch (*p) {
		/* no syntax checking is done here */
		case ';':
			*p = 0;
			ppipe(lastpipe, 0);
			lastpipe = p + 1;
			break;
		case '&':
			*p = 0;
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

char *
readline(char *b, size_t len, int fd)
/* read a line from fd, with error checking
 * return NULL if EOF
 * return -1 if error
 * return the end of string if success
 */
{
	char *p, *end;

	end = b + len;
	for (p = b; p < end; p++) {
		switch (read(fd, p, 1)) {
		case -1:
			return (char *)-1;
		case 0:
			if (p > b) {
				*p = '\0';
				return p;
			}
			return NULL;
		}
		if (*p == '\n') {
			*p = '\0';
			return p;
		}
	}
	fputs("Command line overflow\n", stderr);
	return (char *)-1;
}

int
main(int argc, char **argv)
{
	char line[LINELEN];
	char *pr;

	myuid = getuid();
	mypid = getpid();
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
			return retval;
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
			return retval;
		default: /* silly, but compatible */
			argc = 1;
			input = 0;
			goto mainloop;
		}
	}
	if (argc == 1) {
		input = 0;
		if (isatty(0) && isatty(1)) {
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
		posav = &argv[1];
		posac = argc - 1;
	}
mainloop:
	for (;;) {
mainext:
		if (interactive) {
			if (myuid == 0)
				write(1, "# ", 2);
			else
				write(1, "% ", 2);
		}
		pr = readline(line, LINELEN, input);
		if (pr == NULL)
			break;
		if (pr == (char *)-1)
			continue;
		while (*(pr - 1) == '\\' && (pr - 2) >= line
			&& *(pr - 2) != '\\') {
			*(pr - 1) = ' ';
			pr = readline(pr, LINELEN - (pr - line), input);
			if (pr == NULL)
				break;
			if (pr == (char *)-1)
				goto mainext;
		}
		pline(line);
	}
	quit(retval);
	/*NOTREACHED*/
	return 0;
}
