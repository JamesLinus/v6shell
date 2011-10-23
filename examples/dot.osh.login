: $h/.osh.login - " Modify to taste. "
:
: "  @(#)$Id$  "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  http://v6shell.org/rc_files  "
:

: fd2 -e echo "debug: Executing `"$h/.osh.login"' now..."

if X$u != Xroot goto continue

	:
	: " Give the superuser a different default PATH. "
	:
	setenv	PATH	/opt/local/bin:/usr/local/bin
	setenv	PATH	$p:/bin:/sbin:/usr/bin:/usr/sbin:/usr/X11/bin
	: fallthrough

: continue - " Okay, continue as normal. "

	setenv	MANPATH	\
		/opt/local/share/man:/usr/local/man:/usr/share/man:/usr/X11/share/man
	setenv	PATH		/usr/local/v6bin:$h/v6bin:$p:/usr/games
	setenv	COMMAND_MODE	unix2003
	setenv	EXECSHELL	@EXECSHELL@
	setenv	EDITOR		'/bin/ed -p:'
	setenv	VISUAL		/opt/local/bin/vim
	setenv	PAGER		'less -is'
	setenv	LESS		-M
	setenv	TZ		UTC

	:
	: " Make getopt(3) in the fd2 utility behave the same on  "
	: " GNU/Linux systems as it does on *BSD & other systems. "
	:
	setenv	POSIXLY_CORRECT

	if $n = 1 -a X$1 = Xsh6 goto sh6
		alias	logout	'(sigign)|if { grep "^sigign +  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";sigign + 1;(sigign)|if ! { grep "^sigign +  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";kill -HUP $$;false'
		@SOURCE_OSHDIR@
		: fallthrough
	: sh6

	:
	: " Start ssh-agent & ssh-add key(s) if possible.        "
	: " Change `$h/.ssh/SshKeyFile' to private ssh key file. "
	:
	which ssh-agent ssh-add >/dev/null
	if ! \( $s = 0 -a -e $h/.ssh/SshKeyFile \) goto NoSshAgentOrKey
	if { printenv SSH_AUTH_SOCK } -o { printenv SSH_AGENT_PID } \
		goto Jump >/dev/null
		if ! { mkdir $h/.osh.setenv.$$ } goto Jump
			ssh-agent -c >$h/.osh.setenv.$$/SSH_AGENT
			source $h/.osh.setenv.$$/SSH_AGENT >/dev/null
			rm -r $h/.osh.setenv.$$
		: fallthrough
	: Jump
		ssh-add -l | grep $h/.ssh/SshKeyFile                 >/dev/null
		</dev/null if $s != 0 fd2 ssh-add $h/.ssh/SshKeyFile >/dev/null
		: " Copy previous 2 lines for additional ssh keys if desired. "
	: NoSshAgentOrKey

	: " Print a message or two at login time. "
	echo ; date '+%A, %Y-%m-%d, %T %Z' ; echo
	if -x /usr/games/fortune /usr/games/fortune -s
	: zero status
