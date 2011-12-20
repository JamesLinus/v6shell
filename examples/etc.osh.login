: Begin /etc/osh.login - " Modify to taste as this is just an example. "
:
: " The author of this file, J.A. Neitzel <jneitzel (at) sdf1 (dot) org>, "
: " hereby grants it to the public domain.                                "
:

sigign + 1 2 3 15; : " Ignore HUP, INT, QUIT, and TERM. "

:
: " First, ensure that both if(1) and goto(1) are available and working. "
:
goto HAVE_GOTO
	echo
	echo "/etc/osh.login: goto(1) required; terminating now"
	echo
	sleep 5; kill -9 0; exit

: HAVE_GOTO
	if $s = 0 goto HAVE_IF
	echo
	echo "/etc/osh.login: if(1) required; terminating now"
	echo
	sleep 5; kill -9 0; exit

: HAVE_IF - " Okay, continue as normal. "

: " Be sure the user has a sensible umask by default. "
umask 0022

if $u'' = root goto SUPATH
	:
	: " A default PATH for regular users might be ... "
	:
	setenv	PATH	/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin
	goto JUMP1

: SUPATH - " ... and for the superuser ... "
	setenv	PATH \
		/bin:/usr/bin:/sbin:/usr/sbin:/usr/X11R6/bin:/usr/local/bin

: JUMP1
	setenv	MAIL	/var/mail/$u

if ! { mkdir $h/.osh.setenv.$$ } goto JUMP2
	:
	: " Use the output from `uname -n' to `setenv HOST ???' ... "
	: " A temporary file is required to make this happen.       "
	:
	uname -n|sed 's,\([^.]*\).*,setenv HOST \1,' >$h/.osh.setenv.$$/HOST.$$
	source $h/.osh.setenv.$$/HOST.$$
	rm -r $h/.osh.setenv.$$

: JUMP2 - " Set CTTY and the terminal STATUS character. "

	setenv	CTTY	$t
	stty status '^T' <-

sigign - 1 2 3 15; : " Reset the ignored signals. "

: End /etc/osh.login
