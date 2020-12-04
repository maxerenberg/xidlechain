xidlechain(1)

# NAME

xidlechain - Idle manager for X

# SYNOPSIS

*xidlechain* [options] [events...]

# OPTIONS

*-h*
	Show help message and quit.

*-d*
	Enable debug output.

*-w*
	Wait for command to finish executing before continuing, helpful for ensuring
	that a *before-sleep* command has finished before the system goes to sleep.

	Note: using this option causes xidlechain to block until the command finishes.

# DESCRIPTION

xidlechain listens for idle activity on your X session and executes tasks
on various idle-related events. You can specify any number of events at the
command line. Systemd is required.

# EVENTS

*timeout* <timeout> <timeout command> [resume <resume command>]
	Execute _timeout command_ if there is no activity for <timeout> seconds.

	If you specify "resume <resume command>", _resume command_ will be run when
	there is activity again.

*before-sleep* <command>
	Executes _command_ before systemd puts the computer to sleep.

	Note: this only delays sleeping up to the limit set in *logind.conf(5)* by
	the option InhibitDelayMaxSec. A command that has not finished by then will
	continue running after resuming from sleep.

*after-resume* <command>
	Executes _command_ after logind signals that the computer resumed from sleep.

*lock* <command>
	Executes _command_ when logind signals that the session should be locked

*unlock* <command>
	Executes _command_ when logind signals that the session should be unlocked

*idlehint* <timeout>
	Set IdleHint to indcate an idle logind session after <timeout> seconds.
	Adding an idlehint event will also cause xidlechain to call
	SetIdleHint(false) when first run, as well as on resume and unlock events.

All commands are executed in a shell.

# ENVIRONMENT VARIABLES

If the environment variable XDG_SESSION_ID is set, xidlechain will use it
for the session ID. If not, it will iterate over all sessions and use the first
one which matches the current user and has a seat.

# EXAMPLE

```
xidlechain -w \\
	timeout 300 'i3lock -c 000000' \\
	timeout 600 'xset dpms force off' \\
		resume 'xset dpms force on' \\
	before-sleep 'i3lock -c 000000'
```

This will lock your screen after 300 seconds of inactivity, then turn off your
displays after another 300 seconds, and turn your screens back on when resumed.
It will also lock your screen before your computer goes to sleep.

To make sure xidlechain waits for i3lock to lock the screen before it releases the
inhibition lock, the *-w* options is used in xidlechain (i3lock forks by default).

# AUTHORS

Maintained by Max Erenberg.
This page was adapted from the man page for *swayidle*(1), which is
maintained by Drew DeVault.

For more information about xidlechain development, see
https://github.com/maxerenberg/xidlechain.
