: /etc/osh.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  http://v6shell.org/rc_files  "
:

trap '' 1 2 3 13 14 15 ; : " Ignore HUP, INT, QUIT, PIPE, ALRM, and TERM. "
trap '' 18 21 22 ;       : " Ignore job-control signals: TSTP, TTIN, TTOU "

: fd2 -e echo "debug: Executing `/etc/osh.login' now..."

:
: " Set a default umask and PATH for all (root & !root) users. "
:
umask 0022
if X$u != Xroot goto Continue
	setenv	PATH	/sbin:/usr/sbin:/bin:/usr/bin:/usr/X11R6/bin
	setenv	PATH	$p:/usr/local/sbin:/usr/local/bin
	goto NotRootJump

: Continue
	: " Only add $h/bin if it exists as a searchable directory. "
	if ! \( X$h != X -a -d $h'' -a -d $h/bin -a -x $h/bin \) goto jump
		setenv	PATH	$h/bin
		: fallthrough
	: jump
		setenv	 PATH	$p:/bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin
		setenv	 PATH	$p:/usr/local/bin:/usr/local/sbin:/usr/games
		: setenv PATH	$p:. ; : " Current directory? Not recommended. "

: NotRootJump
	setenv MAIL /var/mail/$u
	stty status '^T' <-
	: fallthrough

if X$h = X -o ! -d $h'' goto Finish
set D $h/.osh.tmp.$$
if ! { mkdir $D } goto Finish

	:
	: " Set $T and/or setenv CTTY as needed. "
	:
	set S 0
	printenv CTTY | wc -l | tr -d ' ' | grep '^1$' >/dev/null
	if $? -ne 0 goto NoCttyJump
		( \
		   echo -n 'set T "' ; printenv CTTY | tr -d '\n' ; echo '"' \
		) >$D/TisCTTY
		source $D/TisCTTY
		set S 1
		rm $D/TisCTTY
		: fallthrough
	: NoCttyJump
		if $S -eq 1 goto TisCTTY
			setenv CTTY $t
			set    T    $t
		: TisCTTY
	unset S

	:
	: " Use the output from `uname -n' to `setenv HOST value'. "
	:
	uname -n | sed 's,\([^.]*\).*,setenv HOST \1,' >$D/HOST
	source $D/HOST ; rm -r $D
	: fallthrough

: Finish

unset D
trap - 1 2 3 13 14 15 ; : " Reset the ignored, non-job-control signals. "
