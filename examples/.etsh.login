: $h/.@OBN@.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) etsh (dot) io>,  "
: "  hereby grants it to the public domain.                           "
:
: "  From:  https://etsh.io/rc_files  "
:

: fd2 -e echo "debug: Executing `"$h/.@OBN@.login"' now..."

: " OpenBSD: Using /etc/man.conf is preferable to setting MANPATH if possible. "
setenv	MANPATH	$h/man:/usr/share/man:/usr/X11R6/man:/usr/local/man
: " But in the  ^^^^^^ case, MANPATH is the better way (assuming you need      "
: " or want your $h/man to remain private).                                    "

alias	logout	'(trap)|if { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";trap "" 1;(trap)|if ! { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";kill -HUP $$;false'
source	etshdir	$$

: " Print a message or two at login time (to standard error). "
fd2 -e echo ; fd2 -e date '+%A, %Y-%m-%d, %T %Z' ; fd2 -e echo
fd2 -e if -x /usr/games/fortune /usr/games/fortune -s
: zero status
