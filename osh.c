/*
 * osh.c - core routines for osh and sh6
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
 *	@(#)osh.c	1.104 (jneitzel) 2004/12/28
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
 = "\100(#)osh.c	1.104 (jneitzel) 2004/12/28";
#endif	/* !lint */

#include "osh.h"

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
int		posac;		/* positional-parameter count */
char		**posav;	/* positional-parameter values */
int		shtype;		/* type of shell */
int		spc;		/* # of processes in current pipeline */
int		splim;		/* current limit on # of processes in splist */
pid_t		*splist;	/* array of process IDs in current pipeline */
int		status;		/* exit status of the shell */
struct shinfo	si;		/* shell and user information */

static char	*arg2p;		/* string argument for the -c option */
static char	line[LINESIZE];	/* command-line buffer */

#ifndef	SH6
static void	rcfile(int *);
#endif	/* !SH6 */
static int	readc(void);
static int	readline(int);
static void	sh_init(void);

int
main(int argc, char **argv)
{
	off_t ioff;
	int c;
#ifndef	SH6
	int rcflag;
#endif	/* !SH6 */

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
	shtype = OTHER;
	if (argc > 1 && *argv[1] == '-') {

		if (signal(SIGINT, SIG_IGN) == SIG_DFL)
			dointr |= DO_SIGINT;
		if (signal(SIGQUIT, SIG_IGN) == SIG_DFL)
			dointr |= DO_SIGQUIT;

		switch (*(argv[1] + 1)) {
		case 'c':
			/*
			 * osh -c [string] [arg1 ...]
			 */
			if (argv[2] == NULL) /* suppress prompting */
				break;
			posav += 1;
			posac -= 1;
			arg2p = argv[2];
			/* FALLTHROUGH */
		case 't':
			/*
			 * osh -t [arg1 ...]
			 */
			if (readline(1) == 1)
				execline(line);
			return(status);
		default:
			/*
			 * Suppress prompting while the shell reads and
			 * executes command lines from the standard input.
			 *	For example:
			 *		osh -  [arg1 ...]
			 *		osh -s [arg1 ...]
			 *	etc.
			 */
			break;
		}

	} else if (argc == 1) {
		if (isatty(FD0) && isatty(FD2)) {
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
		}
	} else {
		/*
		 * osh file [arg1 ...]
		 */
		shtype = COMMANDFILE;
		close(FD0);
		if (open(argv[1], O_RDONLY | O_NONBLOCK, 0) != FD0)
			error(SH_ERR, "%s: cannot open", argv[1]);

		/* Ensure that file is seekable, and unset O_NONBLOCK flag. */
		if (lseek(FD0, (off_t)0, SEEK_CUR) == -1)
			error(SH_ERR, "%s: cannot seek", argv[1]);
		if (fcntl(FD0, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1)
			error(SH_FATAL, "%s: %s", argv[1], strerror(errno));

		/* Ignore the first line if it begins w/ `#!' . */
		ioff = 0;
		if (readc() == '#' && readc() == '!') {
			ioff += 3;
			while ((c = readc()) != '\n' && c != EOF)
				ioff++;
			if (c == EOF)
				return(1);
		}
		lseek(FD0, ioff, SEEK_SET);
	}

#ifndef	SH6
	/* Try to run the initialization files if possible. */
	while (shtype & RCFILE) {
		cmd_loop(0);
		rcfile(&rcflag);
	}
#endif	/* !SH6 */
	cmd_loop(0);
	return(status);
}

/*
 * Read and execute command lines forever or until one of the
 * following conditions is true.  If erract == 1, return on EOF or
 * when errflg == 1.  If erract == 0, return on EOF.  In any case,
 * any error which is deemed to be fatal in a particular context
 * can cause the shell to exit from the error() function.
 */
void
cmd_loop(int erract)
{

	for (errflg = 0; ; ) {
		if (PROMPT(shtype))
			write(FD2, si.euid ? "% " : "# ", (size_t)2);
		switch (readline(0)) {
		case EOF:
			return;
		case 0:
			continue;
		}
		execline(line);
		if (erract && errflg)
			return;
	}
	/* NOTREACHED */
}

/*
 * Initialize the shell.
 */
static void
sh_init(void)
{
#ifndef	SH6
	struct passwd *pw;
	char *str;
#endif	/* !SH6 */

	/* Get process ID and effective user ID. */
	si.pid = getpid();
	si.euid = geteuid();

	/*
	 * Set-ID execution is not supported.
	 *
	 * NOTE: Do not remove the following code w/o understanding
	 *	 the ramifications of doing so.  For further info,
	 *	 see the SECURITY section of the shell manual pages,
	 *	 osh(1) and sh6(1).
	 */
	if (si.euid != getuid() || getegid() != getgid())
		error(SH_FATAL, "Set-ID execution not supported");

	/* File descriptors 0, 1, and 2 must be open. */
	if (!isopen(FD0) || !isopen(FD1) || !isopen(FD2))
		error(SH_FATAL, "%s", strerror(errno));

	/* Allocate memory for aplist and splist; reallocate in forkexec(). */
	aplim = splim = PLSIZE;
	aplist = xmalloc(aplim * sizeof(pid_t));
	splist = xmalloc(splim * sizeof(pid_t));

#ifndef	SH6
	dupfd0 = dup2(FD0, DUPFD0);
	if (dupfd0 == -1 || fcntl(dupfd0, F_SETFD, FD_CLOEXEC) == -1)
		error(SH_FATAL, "%s", strerror(errno));

	/*
	 * Check if `OSH_COMPAT' exists in the shell's environment.
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

	/* Get effective user name for $u. */
	if ((pw = getpwuid(si.euid)) != NULL) {
		si.user = xmalloc(strlen(pw->pw_name) + 1);
		strcpy(si.user, pw->pw_name);
	} else
		si.user = NULL;

	/* Get terminal name for $t. */
	if ((str = ttyname(FD0)) != NULL) {
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
 * NOTE: The shell places no restrictions on the ownership/permissions
 *	 of any rc file.  If the file is readable and regular, the shell
 *	 will use it.  In general, users should protect themselves and not
 *	 do something like `chmod 0666 $h/.osh.login' w/o good reason.
 */
static void
rcfile(int *flag)
{
	struct stat sb;
	int nfd;
	char rc[PATHMAX];
	const char *file;

	/*
	 * Try each rc file in turn.
	 */
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
		if (file == NULL || *file == '\0' ||
		    (nfd = open(file, O_RDONLY | O_NONBLOCK, 0)) == -1)
			continue;

		if (dup2(nfd, FD0) == -1 ||
		    fcntl(FD0, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1)
			error(SH_FATAL, "%s: %s", file, strerror(errno));

		/* It must be a regular file. */
		if (fstat(FD0, &sb) == -1 || !S_ISREG(sb.st_mode)) {
			error(0, "%s: Not a regular file", file);
			close(nfd);
			continue;
		}

		close(nfd);
		shtype |= RCFILE;
		return;
	}
	shtype &= ~RCFILE;

	/*
	 * Restore original standard input.
	 */
	if (dup2(dupfd0, FD0) != FD0)
		error(SH_FATAL, "%s", strerror(errno));
}
#endif	/* !SH6 */

/*
 * Read a character from the string pointed to by arg2p or from
 * the shell's standard input.  When reading from arg2p, return
 * the character or `\n' at end-of-string.  When reading from
 * the standard input, return the character or EOF.  If read(2)
 * fails, exit the shell w/ a status of SH_FATAL.
 */
static int
readc(void)
{
	unsigned char c;

	if (arg2p != NULL) {
		if ((c = *arg2p) != '\0') {
			arg2p++;
			return((int)c);
		}
		return('\n');
	}
	switch (read(FD0, &c, (size_t)1)) {
	case -1:
		error(SH_FATAL, "%s", strerror(errno));
	case 0:
		return(EOF);
	}
	return((int)c);
}

/*
 * Read a line into the command-line buffer.
 * If command-line overflow occurs, call the error() function.
 * This does not terminate the shell if it is interactive
 * or is reading commands from an initialization file.
 *
 * Return values:
 *	1	A line has been read that is neither blank nor empty.
 *	0	A blank or empty line has been read, or command-line
 *		overflow has occurred in either an interactive shell
 *		or an initialization file.
 *	EOF	end-of-file
 */
static int
readline(int oneline)
{
	int backslash, c, eot;
	char *b, *bufmax;

	bufmax = line + LINESIZE;
	for (backslash = eot = 0, b = line, *b = '\0'; b < bufmax; ) {
		if ((c = readc()) == EOF) {
			/*
			 * If interactive and not at the beginning of a line,
			 * allow up to 2 sequential EOTs (^D) w/o returning
			 * EOF.  The third sequential EOT typed by the user
			 * then causes EOF to be returned.  Otherwise, always
			 * return EOF immediately.
			 */
			if (PROMPT(shtype) && *line != '\0' && ++eot < 3)
				continue;
			return(EOF);
		}
		eot = 0;

		switch (c) {
		case '\0':
			/*
			 * Ignore all NUL characters.
			 */
			continue;
		case ' ':
		case '\t':
			/*
			 * Ignore all blanks until any non-blank character
			 * is copied into the command-line buffer below.
			 * Assign a space as a special case for EOT above.
			 */
			if (b > line)
				break;
			*b = ' ';
			continue;
		case '\\':
			backslash++;
			*b++ = c;
			continue;
		case '\n':
			if (backslash & 1) {
				if (oneline == 1)
					return(EOF);
				if (b - 1 > line)
					*(b - 1) = ' ';
				else
					b--;
				backslash = 0;
				continue;
			}
			*b = '\0';
			arg2p = NULL;
			return((*line != '\0') ? 1 : 0);
		}
		backslash = 0;
		*b++ = c;
	}

	/*
	 * Deal w/ command-line overflow.
	 */
	if (isatty(FD0) && arg2p == NULL)
		tcflush(FD0, TCIFLUSH);
	error(0, "Command line overflow");
#ifndef	SH6
	if ((shtype & RCFILE) && !(shtype & DOSOURCE))
		seek_eof();
#endif	/* !SH6 */
	return(0);
}
