/*
 * osh.c - primary routines for osh(1)
 */
/*-
 * Copyright (c) 2003, 2004, 2005, 2006
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
 *	@(#)osh.c	1.108 (jneitzel) 2006/01/20
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
OSH_SOURCEID("osh.c	1.108 (jneitzel) 2006/01/20");
#endif	/* !lint */

#include "osh.h"

/*
 * Diagnostics
 */
const char *const dgn[] = {
	"Cannot fork - try again",	/* ERR_FORK       */
	"Cannot pipe - try again",	/* ERR_PIPE       */
	"Cannot read descriptor",	/* ERR_READ       */
	"Cannot redirect descriptor",	/* ERR_REDIR      */
	"Cannot trim",			/* ERR_TRIM       */
	"Command line overflow",	/* ERR_CLOVERFLOW */
	"Invalid argument list",	/* ERR_BADARGLIST */
	"No match",			/* ERR_NOMATCH    */
	"Out of memory",		/* ERR_NOMEM      */
	"Set-ID execution denied",	/* ERR_SETID      */
	"Too many args",		/* ERR_TMARGS     */
	"Too many characters",		/* ERR_TMCHARS    */
	"arg count",			/* ERR_ARGCOUNT   */
	"bad directory",		/* ERR_BADDIR     */
	"cannot create",		/* ERR_CREATE     */
	"cannot execute",		/* ERR_EXEC       */
	"cannot open",			/* ERR_OPEN       */
	"no args",			/* ERR_NOARGS     */
	"no shell",			/* ERR_NOSHELL    */
	"not found",			/* ERR_NOTFOUND   */
	"syntax error",			/* ERR_SYNTAX     */
	"bad mask",			/* ERR_BADMASK    */
	"bad name",			/* ERR_BADNAME    */
	"bad signal",			/* ERR_BADSIGNAL  */
	"error",			/* ERR_SIGIGN     */
	"no home directory",		/* ERR_BADHOME    */
	"no old directory",		/* ERR_BADOLD     */
	NULL
};

int		chintr;		/* child action flag for SIGINT and SIGQUIT */
int		dupfd0;		/* duplicate of the shell's standard input  */
int		err_source;	/* error flag for the `source' command      */
int		ppac;		/* positional-parameter argument count      */
char	*const	*ppav;		/* positional-parameter argument values     */
int		shtype;		/* shell type (determines shell behavior)   */
int		status;		/* shell exit status                        */
struct shinfo	si;		/* shell and user information               */

static const char
		*arg2p;		/* string argument for the `-c' option      */
static char	line[LINEMAX];	/* command-line buffer                      */

static void	fdfree(void);
static void	rcfile(int *);
static int	readc(void);
static int	readline(int);
static void	sh_init(void);
static void	sh_magic(void);

int
main(int argc, char *const *argv)
{
	int rcflag;

	sh_init();
	if (argv[0] == NULL || *argv[0] == '\0')
		error(SH_ERR, FMT1S, dgn[ERR_BADARGLIST]);
	if (fdtype(FD0, S_IFDIR))
		goto done;

	/*
	 * This implementation behaves exactly like the Sixth Edition
	 * Unix shell WRT options.  For compatibility, any option is
	 * accepted w/o producing an error.
	 */
	shtype = 0;
	if (argc > 1) {
		/* Initialize $0 and any positional parameters. */
		si.name = argv[1];
		ppav = &argv[1];
		ppac = argc - 1;

		switch (*argv[1]) {
		case '-':
			if (signal(SIGINT, SIG_IGN) == SIG_DFL)
				chintr |= CH_SIGINT;
			if (signal(SIGQUIT, SIG_IGN) == SIG_DFL)
				chintr |= CH_SIGQUIT;
			fdfree();

			switch (*(argv[1] + 1)) {
			case 'c':
				/*
				 * osh -c [string [arg1 ...]]
				 */
				shtype = CTOPTION;
				if (argv[2] == NULL)	/* suppress prompting */
					break;
				ppav += 1;	/* incompatible w/ original */
				ppac -= 1;	/* $1 != string, $1 == arg1 */
				arg2p = argv[2];
				if (readline(1) == 1)
					execline(line);
				goto done;
			case 't':
				/*
				 * osh -t [arg1 ...]
				 */
				shtype = CTOPTION;
				if (readline(1) == 1)
					execline(line);
				goto done;
			default:
				/*
				 * Suppress prompting, and read/execute
				 * command lines from the standard input.
				 *	For example:
				 *		osh -  [arg1 ...]
				 *		osh -s [arg1 ...]
				 *	etc.
				 */
				break;
			}
			break;
		default:
			/*
			 * osh file [arg1 ...]
			 */
			shtype = COMMANDFILE;
			close(FD0);
			if (open(argv[1], O_RDONLY, 0) != FD0)
				error(SH_ERR, FMT2S, argv[1], dgn[ERR_OPEN]);
			if (fdtype(FD0, S_IFDIR))
				goto done;
			fdfree();
		}
	} else {
		if (signal(SIGINT, SIG_IGN) == SIG_DFL)
			chintr |= CH_SIGINT;
		if (signal(SIGQUIT, SIG_IGN) == SIG_DFL)
			chintr |= CH_SIGQUIT;
		fdfree();

		if (isatty(FD0) && isatty(FD2)) {
			shtype = INTERACTIVE;
			signal(SIGTERM, SIG_IGN);
			rcflag = (*argv[0] == '-') ? 1 : 3;
			rcfile(&rcflag);
		}
	}

	while (shtype & RCFILE) {
		cmd_loop(0);
		rcfile(&rcflag);
	}
	cmd_loop(0);

done:
	return status;
}

/*
 * Read and execute command lines forever, or until one of the
 * following conditions is true.  If cease == 1, return on EOF
 * or when err_source == 1.  If cease == 0, return on EOF.
 */
void
cmd_loop(int cease)
{
	int gz;

	sh_magic();

	for (err_source = gz = 0; ; ) {
		if (PROMPT(shtype))
			write(FD2, si.euid ? "% " : "# ", (size_t)2);
		switch (readline(0)) {
		case EOF:
			if (!gz)
				status = 0;
			return;
		case 0:
			break;
		case 1:
			execline(line);
			if (!gz)
				gz = 1;
			break;
		}
		if (cease && err_source)
			return;
	}
	/* NOTREACHED */
}

/*
 * Free or release all of the appropriate file descriptors.
 */
static void
fdfree(void)
{
	long lfd;
	int fd;

	if ((lfd = sysconf(_SC_OPEN_MAX)) == -1 || lfd > INT_MAX)
		lfd = NOFILE;
	for (fd = lfd, fd--; fd > DUPFD0; fd--)
		close(fd);
	for (fd--; fd > FD2; fd--)
		close(fd);
}

/*
 * Ignore any `#!shell' sequence as the first line of a regular file.
 *
 * NOTE: This assumes that the kernel on the system where the shell
 *	 is used supports the `#!' mechanism, which may or may not
 *	 be true.  In either case, the first line is always ignored
 *	 if it begins w/ `#!'.
 */
static void
sh_magic(void)
{

	if (fdtype(FD0, S_IFREG) && lseek(FD0, (off_t)0, SEEK_CUR) == 0) {
		if (readc() == '#' && readc() == '!')
			readline(1);
		else
			lseek(FD0, (off_t)0, SEEK_SET);
	}
}

/*
 * Initialize the shell.
 */
static void
sh_init(void)
{

	/* Get process ID and effective user ID. */
	si.pid  = getpid();
	si.euid = geteuid();

	/*
	 * Set-ID execution is not supported.
	 *
	 * NOTE: Do not remove the following code w/o understanding
	 *	 the ramifications of doing so.  For further info,
	 *	 see the SECURITY section of the osh(1) manual page.
	 */
	if (si.euid != getuid() || getegid() != getgid())
		error(SH_ERR, FMT1S, dgn[ERR_SETID]);

	/*
	 * Fail if any of the descriptors 0, 1, or 2 is not open,
	 * or if dupfd0 cannot be set up properly.
	 */
	if (!fdtype(FD0, FD_OPEN) || !fdtype(FD1, FD_OPEN) ||
	    !fdtype(FD2, FD_OPEN) || (dupfd0 = dup2(FD0, DUPFD0)) == -1 ||
	    fcntl(dupfd0, F_SETFD, FD_CLOEXEC) == -1)
		error(SH_ERR, FMT1S, strerror(errno));

	/*
	 * Set the SIGCHLD signal to its default action.
	 * Correct operation of the shell requires that zombies
	 * be created for its children when they terminate.
	 */
	signal(SIGCHLD, SIG_DFL);
}

/*
 * Try to open each rc file for shell initialization.
 * If successful, temporarily assign the shell's standard input to
 * come from said file and return.  Restore original standard input
 * after all files have been tried.
 *
 * NOTE: The shell places no restrictions on the ownership/permissions
 *	 of any rc file.  If the file is readable and regular, the shell
 *	 will use it.  Though, users should protect themselves and not
 *	 do something like `chmod 0666 $h/.osh.login' w/o good reason.
 */
static void
rcfile(int *rcflag)
{
	int nfd, r;
	char path[PATHMAX];
	const char *file, *home;

	/*
	 * Try each rc file in turn.
	 */
	while (*rcflag < 4) {
		file = NULL;
		*path = '\0';
		switch (*rcflag) {
		case 1:
			file = _PATH_SYSTEM_LOGIN;
			break;
		case 2:
			file = _PATH_DOT_LOGIN;
			/* FALLTHROUGH */
		case 3:
			if (file == NULL)
				file = _PATH_DOT_OSHRC;
			home = getenv("HOME");
			if (home != NULL && *home != '\0') {
				r = snprintf(path, sizeof(path),
					"%s/%s", home, file);
				if (r < 0 || r >= (int)sizeof(path)) {
					error(-1, "%s/%s: %s", home, file,
					    strerror(ENAMETOOLONG));
					*path = '\0';
				}
			}
			file = path;
			break;
		}
		*rcflag += 1;
		if (file == NULL || *file == '\0' ||
		    (nfd = open(file, O_RDONLY | O_NONBLOCK, 0)) == -1)
			continue;

		/* It must be a regular file (or a link to a regular file). */
		if (!fdtype(nfd, S_IFREG)) {
			error(-1, FMT2S, file, dgn[ERR_EXEC]);
			close(nfd);
			continue;
		}

		if (dup2(nfd, FD0) == -1 ||
		    fcntl(FD0, F_SETFL, O_RDONLY & ~O_NONBLOCK) == -1)
			error(SH_ERR, FMT2S, file, strerror(errno));

		/* success - clean up, return, and execute the rc file */
		close(nfd);
		shtype |= RCFILE;
		return;
	}
	shtype &= ~RCFILE;

	/*
	 * Restore original standard input.
	 */
	if (dup2(dupfd0, FD0) == -1)
		error(SH_ERR, FMT1S, strerror(errno));
}

/*
 * Read a character from the string pointed to by arg2p or from
 * the shell's standard input.  When reading from arg2p, return
 * the character or `\n' at end-of-string.  When reading from
 * the standard input, return the character or EOF.  If read(2)
 * fails, exit the shell w/ a status of SH_ERR.
 */
static int
readc(void)
{
	unsigned char c;

	if (arg2p != NULL) {
		if ((c = *arg2p) != '\0') {
			arg2p++;
			return (int)c;
		}
		arg2p = NULL;
		return '\n';
	}
	switch (read(FD0, &c, (size_t)1)) {
	case -1:
		error(SH_ERR, "%s %u", dgn[ERR_READ], FD0);
	case 0:
		return EOF;
	}
	return (int)c;
}

/*
 * Read a line into the command-line buffer.
 *
 * Return values:
 *	1	A line has been read that is neither blank nor empty.
 *	0	A blank or empty line has been read, or command-line
 *		overflow has occurred in an interactive shell, an
 *		initialization file, or a sourced file.
 *	EOF	end-of-file
 */
static int
readline(int oln)
{
	int c;
	unsigned bsc, eot, nulc;
	char *b;

	b = line, *b = '\0';
	bsc = eot = nulc = 0;
	while (b < &line[LINEMAX]) {
		if ((c = readc()) == EOF) {
			/*
			 * If interactive and not at the beginning of a line,
			 * allow 2 sequential EOTs (^D) w/o returning EOF.
			 * The 3rd sequential EOT causes EOF to be returned.
			 * Otherwise, always return EOF immediately.
			 */
			if (PROMPT(shtype) && *line != '\0' && ++eot < 3)
				continue;
			return EOF;
		}
		eot = 0;

		switch (c) {
		case '\0':
			/*
			 * Ignore a finite number (2047) of NUL characters.
			 * Then, fail w/ command-line overflow if exceeded.
			 */
			if (++nulc >= (unsigned)sizeof(line))
				goto fail;
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
			bsc++;
			*b++ = c;
			continue;
		case '\n':
			if (bsc & 1) {
				if (oln == 1)
					return EOF;
				if (b - 1 > line)
					*(b - 1) = ' ';
				else
					b--;
				bsc = 0;
				continue;
			}
			*b = '\0';
			return *line != '\0';
		}
		bsc = 0;
		*b++ = c;
	}

fail:
	/*
	 * Deal w/ command-line overflow.
	 */
	if (arg2p == NULL)
		while ((c = readc()) != '\n' && c != EOF)
			if (c == '\0' && ++nulc >= (unsigned)sizeof(line))
				break;
	error(-1, FMT1S, dgn[ERR_CLOVERFLOW]);
	return 0;
}
