: Begin /etc/osh.login - " Modify to taste as this is just an example. "

: " Ignore HUP, INT, QUIT, and TERM. "; sigign + 1 2 3 15

:
: " The following is just to ensure that if(1) and goto(1) are both "
: " available and working.  This is not really necessary, but doing "
: " so can help to avoid unexpected errors and to aid in debugging. "
:
goto HAVE_GOTO
	echo
	echo "/etc/osh.login: goto(1) required; terminating now"
	echo
	sleep 5; false; exit

: HAVE_GOTO
	if $s = 0 goto HAVE_IF
	echo
	echo "/etc/osh.login: if(1) required; terminating now"
	echo
	sleep 5; false; exit

: HAVE_IF - " Okay, both if(1) and goto(1) work as expected; continue. "

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
	if $s = 0 rm -r $h/.osh.setenv.$$

: JUMP2 - " Set TTY and terminal STATUS character if user has no .osh* files. "
	if \( -f $h/.osh.login -a -r $h/.osh.login \) -o \
	   \( -f $h/.oshrc     -a -r $h/.oshrc     \) goto JUMP3
		setenv	TTY	$t
		stty status '^T' <-

: JUMP3
	: " Reset the ignored signals. "; sigign - 1 2 3 15

: End /etc/osh.login
