/*
 * osh - a reimplementation of the Sixth Edition (V6) Unix shell
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
 = "@(#)osh-041028: osh.c	1.103 (jneitzel) 2004/10/28";
#endif	/* !lint */

#include "osh.h"

struct shinfo	si;		/* shell and user information */
#ifndef	SH6
unsigned	clone;		/* current compatibility mode of the shell */
int		dupfd0;		/* duplicate standard input of the shell */
#endif	/* !SH6 */
int		apc;		/* # of known asynchronous processes */
int		aplim;		/* current limit on # of processes in aplist */
pid_t		*aplist;	/* array of known asynchronous process IDs */
pid_t		apstat = -1;	/* whether or not to call apwait() */
int		dointr;		/* if child processes can be interrupted */
int		errflg;		/* shell error flag */
int		posac;		/* count of positional parameters */
char		**posav;	/* positional parameters */
int		shtype;		/* type of shell */
int		spc;		/* # of processes in current pipeline */
int		splim;		/* current limit on # of processes in splist */
pid_t		*splist;	/* array of process IDs in current pipeline */
int		status;		/* exit status of the shell */

#ifndef	SH6
static void	rcfile(int *);
#endif	/* !SH6 */
static char	*readline(char *, size_t);
static void	sh_init(void);

int
main(int argc, char **argv)
{
#ifndef	SH6
	int rcflag = 0;
#endif	/* !SH6 */
	off_t ioff;
	int ch, onecmd;
	char line[LINESIZE];
	char *bs, *rr;

	sh_init();

	/* Setup the positional parameters. */
	si.name = argv[1];
	posav = &argv[1];
	posac = argc - 1;

	/*
	 * This implementation behaves exactly like the Sixth Edition
	 * Unix shell WRT options.  For compatibility, any option is
	 * accepted w/o producing an error.
	 */
	if (argc > 1 && *argv[1] == '-') {

		shtype = OTHER;
		rr = NULL;
		onecmd = 0;

		if (signal(SIGINT, SIG_IGN) == SIG_DFL)
			dointr |= DO_SIGINT;
		if (signal(SIGQUIT, SIG_IGN) == SIG_DFL)
			dointr |= DO_SIGQUIT;

		switch (*(argv[1] + 1)) {
		case 'c':
			/*
			 * osh -c [string] [arg1 ...]
			 */
			if (argv[2] == NULL) /* read from input w/o prompting */
				break;

			posav += 1;
			posac -= 1;

			strncpy(line, argv[2], (size_t)LINESIZE);
			if (line[LINESIZE - 1] != '\0')
				error(0, "Command line overflow");
			rr = line + strlen(line) - 1;
			onecmd = 1;
			break;
		case 't':
			/*
			 * osh -t [arg1 ...]
			 */
			rr = readline(line, (size_t)LINESIZE);
			if (rr == NULL)
				return(0);
			rr -= 1;
			onecmd = 1;
			break;
		default:
			/*
			 * All other possible options are also accepted.
			 *	For example:
			 *		osh -  [arg1 ...]
			 *		osh -s [arg1 ...]
			 *	etc.
			 */
			break;
		}

		if (onecmd) {
			/* Deal w/ `\' characters at end of command line. */
			for (bs = rr; bs >= line && *bs == '\\'; --bs)
				;	/* nothing */
			if ((rr - bs) % 2 == 1)
				return(0);

			/* Parse and execute command line. */
			if (substparm(line) && !pxline(line, PX_SYNEXEC))
				error(0, "syntax error");
			return(status);
		}

	} else if (argc == 1) {
		if (isatty(STDIN_FILENO) && isatty(STDERR_FILENO)) {
			shtype = INTERACTIVE;
			if (signal(SIGINT, SIG_IGN) == SIG_DFL)
				dointr |= DO_SIGINT;
			if (signal(SIGQUIT, SIG_IGN) == SIG_DFL)
				dointr |= DO_SIGQUIT;
			signal(SIGTERM, SIG_IGN);
#ifndef	SH6
			rcflag = (*argv[0] == '-') ? 3 : 1;
			rcfile(&rcflag);
#endif	/* !SH6 */
		} else
			shtype = OTHER;
	} else {
		/*
		 * osh file [arg1 ...]
		 */
		shtype = COMMANDFILE;
		close(STDIN_FILENO);
		if (open(argv[1], O_RDONLY | O_NONBLOCK, 0) != STDIN_FILENO)
			error(SH_ERR, "%s: cannot open", argv[1]);

		/* Ensure that file is seekable, and unset O_NONBLOCK flag. */
		if (lseek(STDIN_FILENO, (off_t)0, SEEK_CUR) == -1)
			error(SH_ERR, "%s: cannot seek", argv[1]);
		if (fcntl(STDIN_FILENO, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1)
			error(SH_FATAL, "%s: %s", argv[1], strerror(errno));

		/* Ignore the first line if it begins w/ `#!' . */
		ioff = 0;
		if (getchar() == '#' && getchar() == '!') {
			ioff += 3;
			while ((ch = getchar()) != '\n' && ch != EOF)
				++ioff;
			if (ch == EOF)
				return(1);
		}
		lseek(STDIN_FILENO, ioff, SEEK_SET);
	}

#ifndef	SH6
	/* Try to run the initialization files if possible. */
	while (shtype & RCFILE) {
		cmd_loop(line, 0);
		rcfile(&rcflag);
	}
#endif	/* !SH6 */
	cmd_loop(line, 0);
	return(status);
}

/*
 * Read, parse, and execute commands forever or until one of the
 * following conditions is true.  If erract == 1, return on EOF or
 * when errflg == 1.  If erract == 0, return on EOF.  In any case,
 * any error which is deemed to be fatal in a particular context
 * can cause the shell to exit from the error() function.
 */
void
cmd_loop(char *lp, int erract)
{
	char *bs, *rr;

	for (errflg = 0; ; ) {
again:
		if (PROMPT(shtype))
			write(STDERR_FILENO, si.euid ? "% " : "# ", (size_t)2);

		rr = readline(lp, (size_t)LINESIZE);
		if (rr == NULL)
			return;
		if (rr == (char *)-1)
			continue;

		/* Deal w/ `\' characters at end of line. */
		while ((rr - 1) >= lp && *(rr - 1) == '\\') {
			for (bs = rr - 2; bs >= lp && *bs == '\\'; --bs)
				;	/* nothing */
			if ((rr - bs - 1) % 2 == 0)
				break;
			*(rr - 1) = ' ';
			rr = readline(rr, (size_t)LINESIZE - (rr - lp));
			if (rr == NULL)
				return;
			if (rr == (char *)-1)
				goto again;
		}

		/* Parse and execute command line. */
		if (substparm(lp) && !pxline(lp, PX_SYNEXEC))
			error(0, "syntax error");
		if (erract && errflg)
			return;
	}
	/* NOTREACHED */
}

/*
 * Handle all errors detected by the shell.
 * es == 0:
 *	The error is only fatal when the shell takes commands from
 *	a file.  Fatal or not, if status has not already been set
 *	to a non-zero value it is set to the value of SH_ERR.
 * es > 0:
 *	The error is fatal unless it happens after a successful call
 *	to fork().  In such a case the child process terminates, but
 *	the main shell process ($$) may continue or exit depending on
 *	the context of the error.  Status is set to the value of es.
 */
void
error(int es, const char *msgfmt, ...)
{
	char fmt[FMTSIZE];
	va_list va;

	if (msgfmt != NULL) {
		va_start(va, msgfmt);
		snprintf(fmt, sizeof(fmt), "%s\n", msgfmt);
		vfprintf(stderr, fmt, va);
		va_end(va);
	}
	if (es == 0) {
		if (status == 0)
			status = SH_ERR;
		if ((shtype & INTERACTIVE) && !(shtype & DOSOURCE))
			return;
		if (lseek(STDIN_FILENO, (off_t)0, SEEK_END) == -1 &&
		    errno == ESPIPE)
			while (getchar() != EOF)
				;	/* nothing */
		if (shtype & (COMMANDFILE|OTHER) && !(shtype & DOSOURCE))
			DOEXIT(status);
		errflg = 1;
		return;
	}
	status = es;
	DOEXIT(status);
}

/*
 * Write the message specified by msgfmt and any arguments from
 * the variable argument list to file descriptor wfd.
 */
void
sh_write(int wfd, const char *msgfmt, ...)
{
	char fmt[FMTSIZE];
	char msg[FMTSIZE];
	va_list va;

	if (msgfmt != NULL) {
		va_start(va, msgfmt);
		snprintf(fmt, sizeof(fmt), "%s\n", msgfmt);
		vsnprintf(msg, sizeof(msg), fmt, va);
		write(wfd, msg, strlen(msg));
		va_end(va);
	}
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

/*
 * Initialize the shell.
 */
static void
sh_init(void)
{
	int fd;
	struct stat fdsb;
#ifndef	SH6
	struct passwd *pw;
	char *str;
#endif	/* !SH6 */

	/* Get process ID and effective user ID. */
	si.pid = getpid();
	si.euid = geteuid();

	/* Set-id execution is not supported. */
	if (si.euid != getuid() || getegid() != getgid())
		error(SH_FATAL, "%s: Set-ID execution not supported",
#ifndef	SH6
		    "osh");
#else
		    "sh6");
#endif	/* !SH6 */

	/* Ensure that stdin, stdout, and stderr are not closed. */
	for (fd = 0; fd < 3; ++fd)
		if (fstat(fd, &fdsb) == -1) {
			close(fd);
			if (open("/dev/null", O_RDWR, 0) != fd)
				error(SH_ERR, "/dev/null: cannot open");
		}

	/* Allocate memory for aplist and splist; reallocate in forkexec(). */
	aplim = splim = PLSIZE;
	aplist = xmalloc(aplim * sizeof(pid_t));
	splist = xmalloc(splim * sizeof(pid_t));

#ifndef	SH6
	dupfd0 = dup2(STDIN_FILENO, DUPFD0);
	if (dupfd0 == -1 || fcntl(dupfd0, F_SETFD, FD_CLOEXEC) == -1)
		error(SH_FATAL, "%s", strerror(errno));

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
			if (pw->pw_uid != si.euid)
				pw = NULL;
	if (pw == NULL)
		pw = getpwuid(si.euid);
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
#endif	/* !SH6 */
}

#ifndef	SH6
/*
 * Try to open each rc file for shell initialization.
 * If successful, temporarily assign the shell's standard input to
 * come from said file and return.  Restore original standard input
 * after all files have been tried.
 *
 * XXX: The shell places no restrictions on the ownership/permissions
 *	of any rc file.  If the file is readable and regular, the shell
 *	will use it.  In general, users should protect themselves and not
 *	do something like `chmod 0666 $h/.osh.login' w/o good reason.
 */
static void
rcfile(int *flag)
{
	struct stat sb;
	int nfd;
	char rc[PATHMAX];
	const char *file;

	/* Try each rc file. */
	while (*flag > 0) {
		file = NULL;
		rc[0] = '\0';
		switch (*flag) {
		case 3:
			file = _PATH_SYSTEM_LOGIN;
			break;
		case 2:
			file = _PATH_DOT_LOGIN;
			/* FALLTHROUGH */
		case 1:
			if (file == NULL)
				file = _PATH_DOT_OSHRC;
			if (si.home != NULL && snprintf(rc, (size_t)PATHMAX,
			    "%s/%s", si.home, file) >= PATHMAX) {
				error(0, "%s/%s: %s", si.home, file,
				    strerror(ENAMETOOLONG));
				rc[0] = '\0';
			}
			file = rc;
			break;
		}
		*flag -= 1;
		if (*file == '\0')
			continue;

		if ((nfd = open(file, O_RDONLY | O_NONBLOCK, 0)) == -1)
			continue;
		/* The rc file must be a regular file. */
		if (fstat(nfd, &sb) == 0 && !S_ISREG(sb.st_mode)) {
			error(0, "%s: Not a regular file", file);
			close(nfd);
			continue;
		}

		if (dup2(nfd, STDIN_FILENO) != STDIN_FILENO)
			error(SH_FATAL, "%s: %s", file, strerror(errno));
		if (fcntl(STDIN_FILENO, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1)
			error(SH_FATAL, "%s: %s", file, strerror(errno));

		close(nfd);
		shtype |= RCFILE;
		return;
	}
	shtype &= ~RCFILE;

	/* Restore original standard input. */
	if (dup2(dupfd0, STDIN_FILENO) != STDIN_FILENO)
		error(SH_FATAL, "%s", strerror(errno));
}
#endif	/* !SH6 */

/*
 * Read a line from the shell's standard input.
 * If read(2) returns -1, terminate the shell w/ non-zero status.
 * If command line overflow occurs in a non-interactive shell,
 * terminate w/ non-zero status.  Otherwise, return -1.
 * Return end-of-string on success.
 * Return NULL at EOF.
 */
static char *
readline(char *b, size_t len)
{
	unsigned eot;
	char *end, *p;

	end = b + len;
	for (eot = 0, p = b; p < end; p++) {
		switch (read(STDIN_FILENO, p, (size_t)1)) {
		case -1:
			DOEXIT(SH_FATAL);
		case 0:
			/* If EOF occurs at the beginning of line, return NULL
			 * immediately.  Otherwise, ignore EOF until it occurs
			 * 3 times in sequence and then return NULL.
			 */
			if (p > b && ++eot < 3) {
				p--;
				continue;
			}
			return(NULL);
		}
		if (*p == '\n') {
			*p = '\0';
			return(p);
		}
		/* Do not allow any NUL characters.
		 * Input to the shell is expected to be text.
		 */
		if (*p == '\0')
			p--;
		eot = 0;
	}

	/* Flush all unread characters from the input queue if
	 * command line overflow occurs and input is from a tty.
	 */
	if (isatty(STDIN_FILENO))
		tcflush(STDIN_FILENO, TCIFLUSH);
	error(0, "Command line overflow");
	return((char *)-1);
}
