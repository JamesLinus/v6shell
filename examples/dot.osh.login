: Begin $h/.osh.login - " Modify to taste as this is just an example. "
:
: " The author of this file, J.A. Neitzel <jneitzel (at) sdf1 (dot) org>, "
: " hereby grants it to the public domain.                                "
:
: echo "debug: Executing `"$h/.osh.login"' now..."

if X$u != Xroot goto PATH
	:
	: " The superuser gets a different default PATH. "
	:
	setenv	PATH \
		/bin:/usr/bin:/sbin:/usr/sbin:/usr/X11R6/bin:/usr/local/bin

: PATH - " Modify PATH slightly, and set some other environment variables. "

setenv	PATH		$h/oshbin:$p:/usr/games
setenv	EXECSHELL	/usr/local/bin/osh
setenv	EDITOR		'/bin/ed -p:'
setenv	VISUAL		/usr/bin/vi
setenv	PAGER		'less -is'
setenv	LESS		-M

: " Print a message or two at login time. "
echo
date '+%A, %Y-%m-%d, %T %Z'; echo
if -x /usr/games/fortune /usr/games/fortune -s
echo

: End $h/.osh.login
