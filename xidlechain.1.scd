xidlechain(1)

# NAME

xidlechain - Idle manager for X

# SYNOPSIS

*xidlechain* [options]

# DESCRIPTION

xidlechain listens for idle activity on your X session and executes tasks
on various idle-related events. You can specify any number of events in a
configuration file.

# OPTIONS

*-h*
	Show help message and quit.

*-d*
	Enable debug output.

*-c* _FILE_
	Specify an alternate configuration file path.

# FILES

When starting, xidlechain reads its configuration from ~/.config/xidlechain.conf,
unless an alternate path was specified using the -c option. The file follows the
*Desktop Entry Specification[1]*, which is similar to the INI format.

## MAIN SECTION
General options are placed under the *[Main]* section:

*ignore_audio* = _true_ or _false_
	If true, audio events will be ignored. If false, timeouts will be
	disabled while audio is playing. The default value is false.

*wait_before_sleep* = _true_ or _false_
	If true, xidlechain will wait for the before-sleep command to finish
	when the system is suspending. The default value is true.

*idlehint_timeout* = _timeout_sec_
	After _timeout_sec_ seconds, xidlechain will call SetIdleHint(true) on
	the *org.freedesktop.login1(5)* session object to indicate an idle
	session. SetIdleHint(false) will be called on resume and unlock events.
	If _timeout_sec_ is 0, then the idle hint will never be set. The default
	value is 0.

*disable_automatic_dpms_activation* = _true_ or _false_
	If true, automatic DPMS activation will be disabled by executing 'xset
	dpms 0 0 0'. The default value is true.

*disable_screensaver* = _true_ or _false_
	If true, the X11 screensaver extension will be disabled by executing
	'xset s off'. The default value is true.

*lock_exec* = _command_
	Execute _command_ when logind signals that the session should be locked.

*unlock_exec* = _command_
	Execute _command_ when logind signals that the session should be unlocked.

*before_sleep_exec* = _command_
	Execute _command_ before systemd puts the computer to sleep.

	Note: this only delays sleeping up to the limit set in *logind.conf(5)* by
	the option InhibitDelayMaxSec. A command that has not finished by then will
	continue running after resuming from sleep.

*after_wake_exec* = _command_
	Execute _command_ after logind signals that the computer resumed from sleep.

## ACTION SECTIONS
Custom actions may be specified in sections beginning with the prefix 'Action '.
Each action section may have the following entries:

*timeout* = _timeout_sec_
	Execute the command in *exec* if there is no activity for _timeout_sec_
	seconds.

*exec* = _command_
	Execute this command if there is no activity for the period specified in
	*timeout*.

*resume_exec* = _command_
	Execute this command if activity resumes after the inactivity period
	specified in *timeout*. This entry is optional.

## SAMPLE CONFIGURATION
```
[Main]
ignore_audio = false
wait_before_sleep = true
idlehint_timeout = 0
disable_automatic_dpms_activation = true
disable_screensaver = true

lock_exec = ~/bin/locker.sh

[Action dim]
timeout = 600
exec = ~/bin/brightness.sh dim
resume_exec = ~/bin/brightness.sh restore

[Action turn off monitor]
timeout = 660
exec = xset dpms force off

[Action restore brightness before going to sleep]
timeout = 1198
exec = xset dpms force on && ~/bin/brightness.sh restore

[Action suspend]
timeout = 1200
exec = systemctl suspend
```

# ENVIRONMENT VARIABLES

If the environment variable XDG_SESSION_ID is set, xidlechain will use it
for the session ID. If not, it will iterate over all sessions and use the first
one which matches the current user and has a seat.

# NOTES
	. Desktop Entry Specification
	   https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-1.1.html

# AUTHORS

Maintained by Max Erenberg.

For more information about xidlechain development, see
https://github.com/maxerenberg/xidlechain.
