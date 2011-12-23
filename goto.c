/*
 * goto - port of the Sixth Edition (V6) UNIX transfer command
 *
 * Modified by Jeffrey Allen Neitzel, July-October 2003.
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
const char revision[] = "@(#)goto.c	1.60 (jneitzel) 2003/10/12";
#endif	/* !lint */

#include <stdio.h>
#include <unistd.h>

#define	EFD	2	/* file descriptor used for error messages */
#define	LBUF	128	/* buffer size for label */

off_t	offset = 0;

int	equal(const char *, const char *);
int	getlin(char *);
int	readc(void);

int
main(int argc, char **argv)
{
	int fin, res;
	char label[LBUF];	/* goto command in V6 used 64 for buffer size */

	if (argc < 2 || isatty(0)) {
		write(EFD, "goto error\n", 11);
		lseek(0, (off_t)0, SEEK_END);
		return(1);
	}
	lseek(0, (off_t)0, SEEK_SET);
	fin = dup(0);

	while ((res = getlin(label)) != 0)
		if (res != -1 && equal(label, argv[1])) {
			lseek(0, offset, SEEK_SET);
			return(0);
		}
	write(EFD, "label not found\n", 16);
	return(1);
}

/*
 * Return value:
 *	 1  -  strings are equal
 *	 0  -  strings are not equal
 */
int
equal(register const char *a, register const char *b)
{
	while (*a == *b) {
		if (*a == '\0')
			return(1);
		++a;
		++b;
	}
	return(0);
}

/*
 * Search for a line containing a possible label.
 * If a possible label is found place it into the buffer pointed to by s.
 * Return value:
 *	 1  -  possible label found
 *	 0  -  label not found
 *	-1  -  error, label too long
 */
int
getlin(register char *s)
{
	register int ch, i;

	i = 0;
	while ((ch = readc()) != EOF) {
		if (ch != ':') {
			while(ch != '\n' && ch != EOF)
				ch = readc();
			continue;
		}
		while ((ch = readc()) == ' ');

		/* Take the first word after ':' as a possible label. */
		while (ch != ' ' && ch != '\t' && ch != '\n' && ch != EOF) {
			if (i >= LBUF - 1) {
				write(EFD, "label too long\n", 15);
				return(-1);
			}
			s[i] = ch;
			++i;
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
	offset++;
	return(getchar());
}

