: $h/.osh.login - " Modify to taste. "
:
: "  The author of this file, J.A. Neitzel <jan (at) v6shell (dot) org>,  "
: "  hereby grants it to the public domain.                               "
:
: "  From:  http://v6shell.org/rc_files  "
:

: fd2 -e echo "debug: Executing `"$h/.osh.login"' now..."

if X$u != Xroot goto Continue

	:
	: " Give the superuser a different default PATH. "
	:
	setenv	PATH	/opt/local/bin:/usr/local/bin
	setenv	PATH	$p:/bin:/sbin:/usr/bin:/usr/sbin:/opt/X11/bin
	: fallthrough

: Continue - " Continue as normal. "

	setenv	MANPATH	\
		/opt/local/share/man:/usr/local/man:/usr/share/man:/opt/X11/share/man
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
	setenv	POSIXLY_CORRECT	''

	alias	logout	'(trap)|if { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";trap "" 1;(trap)|if ! { grep "^trap ..  1$">/dev/null } fd2 -e echo "SIGHUP ignored - Type an EOT (^D) instead";kill -HUP $$;false'
	@SOURCE_OSHDIR@

	:
	: " Start ssh-agent & ssh-add key(s) if possible.        "
	: " Allow ssh key use across multiple login sessions.    "
	: " Change `$h/.ssh/SshKeyFile' to private ssh key file. "
	:
	which ssh ssh-agent ssh-add >/dev/null
	if ! \( $? = 0 -a -e $h/.ssh/SshKeyFile \) goto NoSshAgentOrKey
	if { printenv SSH_AUTH_SOCK } -a ! { printenv IS_OSH_SSH_AGENT } goto AddOrReAddKey >/dev/null
		if -e $h/.osh-ssh-agent goto SourceSshAgent
			( : ) >$h/.osh-ssh-agent ; chmod 0600 $h/.osh-ssh-agent
			echo ': Agent started by osh pid '$$ >$h/.osh-ssh-agent
			echo 'setenv IS_OSH_SSH_AGENT true;'>>$h/.osh-ssh-agent
			ssh-agent -c | head -2              >>$h/.osh-ssh-agent
			: fallthrough
		: SourceSshAgent
			source $h/.osh-ssh-agent
			: fallthrough
	: AddOrReAddKey
		ssh-add -l | grep $h/.ssh/SshKeyFile                 >/dev/null
		</dev/null if $? != 0 fd2 ssh-add $h/.ssh/SshKeyFile >/dev/null
		: " Copy & adjust previous 2 lines for additional ssh keys... "
		: fallthrough
	: NoSshAgentOrKey

	: " Print a message or two at login time. "
	echo ; date '+%A, %Y-%m-%d, %T %Z' ; echo
	if -x /usr/games/fortune /usr/games/fortune -s
	: zero status
