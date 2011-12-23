: Begin .osh.login - modify to taste as this is just an example.

: " Set the umask to a sensible `paranoid' value if it is not already ;) "
( umask ) | grep '^0077$' >/dev/null
if $sx = 0x goto PATH

umask 0077

: PATH - " Modify the current value of PATH to include some "
:	 " additional directories.                          "
setenv	PATH	$h/oshbin:$p:/usr/games:$h/bin

: " some other useful environment variables "
setenv	EDITOR	'/bin/ed -p:'
setenv	VISUAL	/usr/bin/vi
setenv	PAGER	'less -is'
setenv	LESS	-M

if { expr $t : '.*/tty[p-zP-T].*' = 0 } goto vt220 >/dev/null

setenv	TERM	xterm-xfree86

: " Set xterm(1) or rxvt(1) title.  xtitle is an osh command file I wrote. "
xtitle
goto jump1

: vt220
setenv	TERM	vt220

: jump1
: " Set terminal options. "
stty altwerase -ixany imaxbel brkint ignpar -oxtabs -parenb <--

:
: " Regarding compatibility mode of the shell:				"
: "	You can choose either (or neither) of the following options	"
: "	according to your needs.					"
:
: "	I only mention `Option 2' below since it can be useful even	"
: "	though this is the default mode of the shell.  I leave it	"
: "	to the user to determine how and why it might be useful.	"
:

: " Option 1:								"
: "	You can tell this and future invocations of the shell to be	"
: "	compatible w/ the Thompson shell by removing the initial `: '	"
: "	from the following two lines:					"
: setenv	OSH_COMPAT	clone
: set clone

: " Option 2:								"
: "	If you wish to run in non-compatible mode, you might want to	"
: "	remove the initial `: ' from the next line:			"
: setenv	OSH_COMPAT	noclone

: End .osh.login
