/*
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
 */

#ident	"@(#)goto.c	1.3 (gritter) 2/14/02>"

#include	<unistd.h>
#include	<limits.h>
#include	<stdio.h>

off_t	offset = 0;

int	getlin(char *);
int	compar(char *, char *);
int	readc(void);

int
main(int argc, char **argv)
{
	int fin;
	char line[LINE_MAX];

	if (argc < 2 || isatty(0)) {
		write(1, "goto error\n", 11);
		lseek(0, (off_t)0, SEEK_END);
		return(1);
	}
	lseek(0, (off_t)0, SEEK_SET);
	fin = dup(0);

loop:
	if (getlin(line)) {
		write(1, "label not found\n", 16);
		return(1);
	}
	if (compar(line, argv[1]))
		goto loop;
	lseek(0, offset, SEEK_SET);
	return(0);
}

int
getlin(register char *s)
{
	register int ch, i;

	i = 0;
l:
	if ((ch = readc()) == EOF)
		return(1);
	if (ch != ':') {
		while(ch != '\n' && ch != '\0')
			ch = readc();
		goto l;
	}
	while ((ch = readc()) == ' ')
		continue;
	while (ch != ' ' && ch != '\n' && ch != '\0') {
		s[i++] = ch;
		ch = readc();
	}
	while(ch != '\n')
		ch = readc();
	s[i] = '\0';
	return(0);
}

int
compar(register char *s1, register char *s2)
{
	register int i;

	i = 0;
l:
	if(s1[i] != s2[i])
		return(1);
	if (s1[i++] == '\0')
		return(0);
	goto l;
}

int
readc()
{
	offset++;
	return(getchar());
}
