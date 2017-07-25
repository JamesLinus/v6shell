: @SYSCONFDIR@/@OBN@.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  https://etsh.io/rc_files  "
:

trap '' 1 2 3 13 14 15 ; : " Ignore HUP, INT, QUIT, PIPE, ALRM, and TERM. "
trap '' 18 21 22 ;       : " Ignore job-control signals: TSTP, TTIN, TTOU "

: fd2 -e echo "debug: Executing `@SYSCONFDIR@/@OBN@.login' now..."

unset X
set   X @PREFIX@ ; : " This is the PREFIX where shell is installed. "

:
: " Set a default umask and PATH for all (root & !root) users. "
:
umask 0022
if X$u != Xroot goto NotRoot
	setenv	PATH	/sbin:/usr/sbin:/bin:/usr/bin:/usr/X11R6/bin
	:
	: " Notice that I have chosen to hard code the /usr/local/sbin and   "
	: " /usr/local/bin directories below to avoid problems when building "
	: " an OpenBSD release.  That is conceivable / dangerous / possible  "
	: " or was in the past at least, but flip the commented setenv PATH  "
	: " lines below if you wish.  Choose one or the other but not both.  "
	:
	setenv		PATH	$p:/usr/local/sbin:/usr/local/bin
	: setenv	PATH	$p:$X/sbin:$X/bin
	:
	: " There are other ways to selectively change root's PATH to allow "
	: " etsh use in ($h/@OBN@.login, ~/.profile, etc), as needed.       "
	:
	: " Note to self: etsh is not a job-control shell; I can explore      "
	: " setpgid(2) more fully and see what I can do, but IIRC this breaks "
	: " compatibility.  If true, that is a showstopper for this project.  "
	:
	goto Continue

: NotRoot
	setenv	 PATH	$X/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/X11R6/bin
	setenv	 PATH	$p:/usr/local/sbin:/usr/local/bin:/usr/games
	: setenv PATH	$p:. ; : " Current directory is not recommended. "
	: fallthrough

: Continue
	setenv MAIL /var/mail/$u
	stty status '^T' <-
	: fallthrough

if X$h = X -o ! -d $h'' goto Finish
set D $h/.etsh.tmp.$$
if ! { mkdir $D } goto Finish

	:
	: " Set $T and/or setenv CTTY as needed. "
	:
	set S 0
	printenv CTTY | wc -l | tr -d ' ' | grep '^1$' >/dev/null
	if $? -ne 0 goto NoCtty
		( \
		   echo -n 'set T "' ; printenv CTTY | tr -d '\n' ; echo '"' \
		) >$D/TisCTTY
		source $D/TisCTTY
		set S 1
		rm $D/TisCTTY
		: fallthrough
	: NoCtty
		if $S -eq 1 goto TisCTTY
			setenv CTTY $t
			set    T    $t
		: TisCTTY
			: fallthrough
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
