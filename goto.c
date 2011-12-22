/*
 * goto - a port of the Sixth Edition (V6) Unix transfer command
 */
/*
 * Modified by Jeffrey Allen Neitzel, 2003, 2004.
 *
 * Modified by Gunnar Ritter, Freiburg i. Br., Germany, February 2002.
 */
/*
 *	Derived from: Sixth Edition Unix /usr/source/s1/goto.c
 */
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
 *	@(#)goto.c	1.3 (gritter) 2/14/02
 */

#ifndef	lint
const char vid[]
# if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4
   __attribute__((used))
# elif defined(__GNUC__)
   __attribute__((unused))
# endif
 = "@(#)osh-041028: goto.c	1.66 (jneitzel) 2004/10/28";
#endif	/* !lint */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	LABELSIZE	1024	/* size of label buffer */

unsigned long long line;
off_t offset;

int	getlabel(int, char *);
int	xgetc(void);

int
main(int argc, char **argv)
{
	int rtn;
	char label[LABELSIZE];

	if (lseek(STDIN_FILENO, (off_t)0, SEEK_SET) == -1 && errno == ESPIPE) {
		fprintf(stderr, "goto: error\n");
		while (getchar() != EOF)
			;	/* nothing */
		return(2);
	}
	if (argc < 2 || *argv[1] == '\0' || isatty(STDIN_FILENO)) {
		fprintf(stderr, "goto: error\n");
		lseek(STDIN_FILENO, (off_t)0, SEEK_END);
		return(2);
	}

	while ((rtn = getlabel(*argv[1], label)) > 0)
		if (strcmp(label, argv[1]) == 0) {
			lseek(STDIN_FILENO, offset, SEEK_SET);
			return(0);
		}

	if (rtn == 0) {
		fprintf(stderr, "goto: %s: label not found\n", argv[1]);
		return(1);
	}
	lseek(STDIN_FILENO, (off_t)0, SEEK_END);
	if (rtn == -1)
		fprintf(stderr, "goto: line %llu: label too long\n", line + 1);
	else
		fprintf(stderr, "goto: bad input\n");
	return(2);
}

/*
 * Get a possible label for comparison against argv[1], and
 * increment the line count whenever newlines are encountered.
 *	Return values:
 *		1	success (possible label found)
 *		0	EOF (label not found)
 *		-1	label too long
 *		-2	bad input
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
			while (ch != '\n' && ch != EOF && ch != '\0')
				ch = xgetc();
			if (ch == '\0')
				return(-2);
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
			if (ch == '\0')
				return(-2);
			if (p >= end)
				return(-1);
			*p++ = ch;
			ch = xgetc();
		}
		while (ch != '\n' && ch != EOF && ch != '\0')
			ch = xgetc();
		if (ch == EOF)
			return(0);
		if (ch == '\0')
			return(-2);
		line++;
		*p = '\0';
		return(1);
	}
	return(0);
}

int
xgetc(void)
{

	offset++;
	return(getchar());
}
