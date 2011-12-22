: Begin .osh.login - " Modify to taste as this is just an example. "

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

if -f $h/.oshrc -a -r $h/.oshrc goto End
	setenv	TTY	$t

	if { expr $t : '.*/tty[p-zP-T].*' = 0 } goto vt220 >/dev/null

		setenv	TERM	xterm-xfree86

		: " Set xterm(1) or rxvt(1) title. "
		xtitle
		goto jump

	: vt220
		setenv	TERM	vt220

	: jump - " Set terminal options. "
		stty altwerase -ixany imaxbel brkint ignpar -oxtabs -parenb <-
		stty status '^T' <-

: End .osh.login example
