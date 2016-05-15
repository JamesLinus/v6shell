: /etc/osh.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  http://v6shell.org/rc_files/OpenBSD  "
:

trap '' 1 2 3 13 14 15 ; : " Ignore HUP, INT, QUIT, PIPE, ALRM, and TERM. "
trap '' 18 21 22 ;       : " Ignore job-control signals: TSTP, TTIN, TTOU "

:
: " Set a default umask and PATH for all (root & !root) users. "
:
umask 0022
if X$u != Xroot goto continue
	setenv	PATH	/sbin:/usr/sbin:/bin:/usr/bin:/usr/X11R6/bin
	setenv	PATH	$p:/usr/local/sbin:/usr/local/bin
	goto NotRootJump
: continue
	if X$h = X -o ! -d $h'' goto jump
		setenv	PATH	$h/bin
	: jump
		setenv	 PATH	$p:/bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin
		setenv	 PATH	$p:/usr/local/bin:/usr/local/sbin:/usr/games
		: setenv PATH	$p:. ; : " Current directory? Not recommended. "
: NotRootJump

: fd2 -e echo "debug: Executing `/etc/osh.login' now..."

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
	: fallthrough

: finish

trap - 1 2 3 13 14 15 ; : " Reset the ignored, non-job-control signals. "
