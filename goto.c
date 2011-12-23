/*
 * goto - port of the Sixth Edition (V6) Unix transfer command
 *
 * Modified by Jeffrey Allen Neitzel, 2003, 2004.
 *
 * Modified by Gunnar Ritter, Freiburg i. Br., Germany, February 2002.
 *
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
const char revision[] = "@(#)goto.c	1.61 (jneitzel) 2004/02/01";
#endif	/* !lint */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	LBUF	129	/* size of label buffer */

unsigned long long lnum;
off_t offset;

int	getlin(char *);
int	readc(void);

int
main(int argc, char **argv)
{
	int res;
	char label[LBUF];

	if (argc < 2 || isatty(0)) {
		fputs("goto error\n", stderr);
		lseek(0, (off_t)0, SEEK_END);
		return(1);
	}
	lseek(0, (off_t)0, SEEK_SET);

	lnum = 1;
	while ((res = getlin(label)) > 0)
		if (strcmp(label, argv[1]) == 0) {
			lseek(0, offset, SEEK_SET);
			return(0);
		}
	if (res == -1) { /* Label is longer than 128 characters. */
		fprintf(stderr, "line %llu: label too long\n", lnum);
		lseek(0, (off_t)0, SEEK_END);
		return(1);
	}
	fputs("label not found\n", stderr);
	return(1);
}

/*
 * Search for a line containing a possible label.
 * Return  1 if possible label found.
 * Return  0 if label not found.
 * Return -1 if label too long.
 */
int
getlin(char *s)
{
	int ch, i;

	i = 0;
	while ((ch = readc()) != EOF) {
		if (ch != ':') {
			while (ch != '\n' && ch != EOF)
				ch = readc();
			continue;
		}
		while ((ch = readc()) == ' ');

		/* First word after ':' is a possible label; copy it to s. */
		while (ch != ' ' && ch != '\t' && ch != '\n' && ch != EOF) {
			if (i + 1 >= LBUF)
				return(-1);
			s[i++] = ch;
			ch = readc();
		}

		/* Does command file lack a final newline before EOF? */
		if (ch == EOF)
			return(0);

		/* Reposition offset to first line after label. */
		while (ch != '\n')
			ch = readc();

		s[i] = '\0';
		return(1);
	}
	return(0);
}

int
readc()
{
	int ch;

	offset++;
	if ((ch = getchar()) == '\n')
		lnum++;
	return(ch);
}
