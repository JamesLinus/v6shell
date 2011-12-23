/*
 * fd2 - adaptation of the PWB/Unix redirect diagnostic output command
 */
/*
 * Modified by Jeffrey Allen Neitzel, 2004.
 */
/*	Derived from: PWB/Unix /sys/source/s1/fd2.c	*/
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   Redistributions of source code and documentation must retain the
 *    above copyright notice, this list of conditions and the following
 *    disclaimer.
 *   Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *   All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 *   Neither the name of Caldera International, Inc. nor the names of
 *    other contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)fd2.c	1.4
 */

#ifndef	lint
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
#  define	ID_ATTR	__attribute__((used))
# elif defined(__GNUC__)
#  define	ID_ATTR	__attribute__((unused))
# else
#  define	ID_ATTR
# endif
const char revision[] ID_ATTR = "@(#)fd2.c	1.44 (jneitzel) 2004/06/28";
#endif	/* !lint */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

void	usage(void);
void	xdup2(int, int);

/*
 *	Usage: fd2 [[-]-file | +] command [command-arg ...]
 *
 *	Fd2 executes command with file descriptor 1 (standard output)
 *	redirected to file descriptor 2 (diagnostic output), or with
 *	diagnostic output redirected to a file or to standard output.
 *
 *		fd2 -file command ...
 *			Writes diagnostic output to "file".
 *
 *		fd2 --file command ...
 *			Appends diagnostic output to "file".
 *
 *		fd2 + command ...
 *			Redirects diagnostic output to standard output.
 *
 *		fd2 command ...
 *			Redirects standard output to diagnostic output.
 */
int
main(int argc, char **argv)
{
	int fd, start;
	unsigned char append;
	const char *file;

	/* Revoke set-id privileges. */
	setgid(getgid());
	setuid(getuid());

	if (argc < 2 || argv[1][0] == '\0')
		usage();

	start = 2;
	switch (argv[1][0]) {
	case '-':
		/* fd2 [-]-file command ... */
		if (argv[start] == NULL || argv[start][0] == '\0')
			usage();
		append = 0;
		switch (argv[1][1]) {
		case '\0':
			file = "";
			break;
		case '-':
			file = &argv[1][2];
			append = 1;
			break;
		default:
			file = &argv[1][1];
			break;
		}
		if (file[0] == '\0')
			usage();

		if(append)
			fd = open(file, O_WRONLY | O_APPEND | O_CREAT, 0666);
		else
			fd = open(file, O_WRONLY | O_TRUNC | O_CREAT, 0666);

		if (fd == -1) {
			fprintf(stderr, "fd2: %s: cannot create\n", file);
			return(1);
		}

		if (fd > STDERR_FILENO) {
			xdup2(fd, STDERR_FILENO);
			close(fd);
		}
		break;
	case '+':
		/* fd2 + command ... */
		if (argv[start] == NULL || argv[start][0] == '\0')
			usage();
		xdup2(STDOUT_FILENO, STDERR_FILENO);
		break;
	default:
		/* fd2 command ... */
		start = 1;
		xdup2(STDERR_FILENO, STDOUT_FILENO);
		break;
	}

	execvp(argv[start], &argv[start]);
	if (errno == ENOENT || errno == ENOTDIR) {
		fprintf(stderr, "fd2: %s: not found\n", argv[start]);
		return(127);
	}
	fprintf(stderr, "fd2: %s: cannot execute\n", argv[start]);
	return(126);
}

void
xdup2(int fda, int fdb)
{

	if (dup2(fda, fdb) == -1) {
		write(open("/dev/tty", O_WRONLY, 0),
		    "fd2: bad file descriptor?!?\n", (size_t)28);
		exit(1);
	}
}

void
usage(void)
{

	fputs("usage: fd2 [[-]-file | +] command [command-arg ...]\n", stderr);
	exit(1);
}
