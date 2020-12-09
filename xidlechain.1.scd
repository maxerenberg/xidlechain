xidlechain(1)

# NAME

xidlechain - Idle manager for X

# SYNOPSIS

*xidlechain* [options] [events...]

# DESCRIPTION

xidlechain listens for idle activity on your X session and executes tasks
on various idle-related events. You can specify any number of events at the
command line. Systemd is required.

# OPTIONS

*-h*
	Show help message and quit.

*-d*
	Enable debug output.

*--no-wait-before-sleep*
	Do not wait for the before-sleep command to finish when the system is suspending.
	By default, xidlechain will wait (block) for this command to finish before the
	system goes to sleep.

*--no-kill-on-resume*
	Do not kill child processes from the timeout commands when user activity resumes.
	By default, xidlechain will send a SIGTERM to these processes when resuming from
	inactivity.

*--ignore-audio*
	Ignore audio events. By default, timeouts are disabled while audio is playing.

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

*after-wake* <command>
	Executes _command_ after logind signals that the computer resumed from sleep.

*lock* <command>
	Executes _command_ when logind signals that the session should be locked.

*unlock* <command>
	Executes _command_ when logind signals that the session should be unlocked.

*idlehint* <timeout>
	Set IdleHint to indicate an idle logind session after <timeout> seconds.
	Adding an idlehint event will also cause xidlechain to call
	SetIdleHint(false) when first run, as well as on resume and unlock events.

All commands are prepended with 'exec' and run with 'sh -c'.

# ENVIRONMENT VARIABLES

If the environment variable XDG_SESSION_ID is set, xidlechain will use it
for the session ID. If not, it will iterate over all sessions and use the first
one which matches the current user and has a seat.

# EXAMPLE

```
xidlechain \\
	timeout 300 'i3lock -c 000000' \\
	timeout 600 'xset dpms force off' \\
		resume 'xset dpms force on' \\
	before-sleep 'i3lock -c 000000' \\
	after-wake 'xset dpms force on'
```

This will lock your screen after 300 seconds of inactivity, then turn off your
displays after another 300 seconds, and turn your screens back on when user
activity is detected.
It will also lock your screen before your computer goes to sleep, and turn
the screen back on when the computer wakes up.

Note that the before-sleep command must exit as quickly as possible since
xidlechain will wait for it to complete before releasing the Inhibit lock
(to disable this behaviour, use the --no-wait-before-sleep flag).

# AUTHORS

Maintained by Max Erenberg.
This page was adapted from the man page for *swayidle*(1).

For more information about xidlechain development, see
https://github.com/maxerenberg/xidlechain.
