: Begin $h/.osh.login - " Modify to taste as this is just an example. "
:
: " The author of this file, J.A. Neitzel <jneitzel (at) sdf1 (dot) org>, "
: " hereby grants it to the public domain.                                "
:

: " Set the umask to `0077' if the system administrator did not "
: " already do this in /etc/osh.login, or possibly elsewhere.   "
( umask ) | grep '^0077$' >/dev/null
if $s = 0 goto PATH

	umask 0077

: PATH - " Modify the current value of PATH to include some "
:	 " additional directories.                          "
setenv	PATH	$h/oshbin:$p:/usr/games:$h/bin

: " some other useful environment variables "
setenv	EDITOR	'/bin/ed -p:'
setenv	VISUAL	/usr/bin/vi
setenv	PAGER	'less -is'
setenv	LESS	-M

if -x /usr/games/fortune /usr/games/fortune -s
echo
date '+%A, %Y-%m-%d, %R %Z'
echo

: End $h/.osh.login
