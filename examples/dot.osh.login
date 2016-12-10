: $h/.osh.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  http://v6shell.org/rc_files  "
:

: fd2 -e echo "debug: Executing `"$h/.osh.login"' now..."

setenv	MANPATH	$h/man:/usr/share/man:/usr/X11R6/man:/usr/local/man

alias	logout	'(trap)|if { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";trap "" 1;(trap)|if ! { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";kill -HUP $$;false'
source	oshdir	$$

: " Print a message or two at login time. "
fd2 -e echo ; fd2 -e date '+%A, %Y-%m-%d, %T %Z' ; fd2 -e echo
fd2 -e if -x /usr/games/fortune /usr/games/fortune -s
: zero status
