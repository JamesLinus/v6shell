: Begin /etc/osh.login - modify to taste as this is just an example.

: " Ignore HUP, INT, QUIT, and TERM. "; trap + 1 2 3 15

:
: "	If  the  first  character  of  the name used to invoke the	"
: "	shell is `-' (as it is when you login), it first  attempts	"
: "	to  read  `/etc/osh.login'.   Next,  it  attempts  to read	"
: "	`.osh.login' in the user's home directory.   For  each  of	"
: "	these files which is readable and seekable (see lseek(2)),	"
: "	the shell executes the commands contain within.  Upon suc-	"
: "	cessful  completion,  the shell prompts the user for input	"
: "	as usual.							"
:
: "	In the normal case, a SIGINT or SIGQUIT signal received by	"
: "	the  shell  during  execution  of either file causes it to	"
: "	cease execution of that file.  This does not terminate the	"
: "	shell.   If  desired, the trap special command can be used	"
: "	to ignore signals.						"
:
: "	Any untrapped signal, shell-detected error  (e.g.,  syntax	"
: "	error), or exit command in either file causes the shell to	"
: "	terminate immediately.						"
:

: " Be sure the user has a sensible umask by default. "
umask 0022

setenv	PATH	/bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin
setenv	MAIL	/var/mail/$u
setenv	HOST	locutus

setenv	TTY	$t

stty status '^T' <-

: " Reset the trapped signals. "; trap - 1 2 3 15

: End /etc/osh.login
