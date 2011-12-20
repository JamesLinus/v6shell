: Begin /etc/osh.login - " Modify to taste as this is just an example. "
:
: " The author of this file, J.A. Neitzel <jneitzel (at) sdf1 (dot) org>, "
: " hereby grants it to the public domain.                                "
:

sigign + 1 2 3 13 14 15; : " Ignore HUP, INT, QUIT, PIPE, ALRM, and TERM. "

:
: " Set a default PATH and umask for all users. "
:
setenv	PATH	/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin
umask 0022

: echo "debug: Executing `/etc/osh.login' now..."

:
: " Ensure that goto(1) and if(1) are available and working. "
: " Simply assume this is the case for echo(1) and sleep(1). "
:
goto goto_ok
	echo
	echo '/etc/osh.login: Terminating session now - goto(1) required'
	echo
	sleep 5
	exit

: goto_ok
	if $s = 0 goto continue
	echo
	echo '/etc/osh.login: Terminating session now - if(1) required'
	echo
	sleep 5
	exit

: continue - " Okay, continue as normal. "

setenv	MAIL	/var/mail/$u
setenv	CTTY	$t
stty status '^T' <-

if X$h = X -o ! -d $h'' goto finish
	if ! { mkdir $h/.osh.setenv.$$ } goto finish
	:
	: " Use the output from `uname -n' to `setenv HOST value'. "
	: " Notice that doing so requires using a temporary file.  "
	:
	uname -n | sed 's,\([^.]*\).*,setenv HOST \1,' >$h/.osh.setenv.$$/HOST
	source $h/.osh.setenv.$$/HOST
	rm -r $h/.osh.setenv.$$

: finish

sigign - 1 2 3 13 14 15; : " Reset the ignored signals. "

: End /etc/osh.login
