/*
 * goto - a port of the Sixth Edition (V6) Unix transfer command
 */
/*
 * Copyright (c) 2004, 2005
 *	Jeffrey Allen Neitzel <jneitzel@sdf1.org>.
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
 *	@(#)goto.c	1.2 (jneitzel) 2005/01/20
 */
/*
 *	Derived from: Sixth Edition Unix /usr/source/s1/goto.c
 */
/*
 * Copyright (C) Caldera International Inc.  2001-2002.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	lint
const char version[]
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
   __attribute__((used))
# elif defined(__GNUC__)
   __attribute__((unused))
# endif
 = "\100(#)goto.c	1.2 (jneitzel) 2005/01/20";
#endif	/* !lint */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	LABELSIZE	1024	/* size of the label buffer */

unsigned long long line;
off_t offset;

int	getlabel(int, char *);
int	xgetc(void);
void	xlseek(int, off_t, int);

int
main(int argc, char **argv)
{
	int rtn;
	char label[LABELSIZE];

	if (argc < 2 || *argv[1] == '\0' || isatty(STDIN_FILENO)) {
		xlseek(STDIN_FILENO, (off_t)0, SEEK_END);
		fprintf(stderr, "goto: error\n");
		return(2);
	}
	xlseek(STDIN_FILENO, (off_t)0, SEEK_SET);

	while ((rtn = getlabel(*argv[1], label)) > 0)
		if (strcmp(label, argv[1]) == 0) {
			xlseek(STDIN_FILENO, offset, SEEK_SET);
			return(0);
		}

	if (rtn == EOF) {
		fprintf(stderr, "goto: %s: label not found\n", argv[1]);
		return(1);
	}
	xlseek(STDIN_FILENO, (off_t)0, SEEK_END);
	fprintf(stderr, "goto: line %llu: label too long\n", line + 1);
	return(2);
}

/*
 * Get a possible label for comparison against argv[1],
 * and increment line each time a `\n' is encountered.
 *
 * Return values:
 *	1	possible label found
 *	0	label too long
 *	EOF	end-of-file
 */
int
getlabel(int first, char *buf)
{
	int ch;
	char *end, *p;

	end = buf + LABELSIZE - 1;
	for (p = buf; (ch = xgetc()) != EOF; ) {

		/* The `:' can be preceded by blanks. */
		while (ch == ' ' || ch == '\t')
			ch = xgetc();
		if (ch != ':') {
			while (ch != '\n' && ch != EOF)
				ch = xgetc();
			if (ch == '\n')
				line++;
			continue;
		}

		/* Skip all blanks following the `:' character. */
		while ((ch = xgetc()) == ' ' || ch == '\t')
			;	/* nothing */
		if (ch == '\n') {
			line++;
			continue;
		}
		if (ch != first) /* not a label */
			continue;

		/* Possible label found; try to copy first word to buf. */
		while (ch != '\n' && ch != ' ' && ch != '\t' && ch != EOF) {
			if (p >= end)
				return(0);
			*p++ = ch;
			ch = xgetc();
		}
		while (ch != '\n' && ch != EOF)
			ch = xgetc();
		if (ch == EOF)
			return(EOF);
		line++;
		*p = '\0';
		return(1);
	}
	return(EOF);
}

int
xgetc(void)
{

	offset++;
	return(getchar());
}

void
xlseek(int ifd, off_t ioff, int act)
{

	if (lseek(ifd, ioff, act) == -1) {
		if (errno == ESPIPE)
			while (getchar() != EOF)
				;	/* nothing */
		if (act == SEEK_SET) {
			fprintf(stderr, "goto: cannot seek\n");
			exit(2);
		}
	}
}
