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
	If true, xidlechain will wait for actions triggered by *sleep* to finish
	when the system is suspending. The default value is true.

	Note: this only delays sleeping up to the limit set in *logind.conf(5)* by
	the option InhibitDelayMaxSec. A command that has not finished by then will
	continue running after resuming from sleep.

*disable_automatic_dpms_activation* = _true_ or _false_
	If true, automatic DPMS activation will be disabled by executing 'xset
	dpms 0 0 0' when the program starts. The default value is true.

*disable_screensaver* = _true_ or _false_
	If true, the X11 screensaver extension will be disabled by executing
	'xset s off' when the program starts. The default value is true.

*enable_dbus* = _true_ or _false_
	If true, the settings will be exported over a D-Bus interface at
	io.github.maxerenberg.xidlechain. The default value is true.

## ACTION SECTIONS
Custom actions may be specified in sections beginning with the prefix 'Action '.
Each action section may have the following entries.

*trigger* = _trigger_
	When the trigger is activated, the *exec* command is executed. When it
	is deactivated, the *resume_exec* command is executed. Required.

*exec* = _command_
	Execute this command when the trigger activates. Optional.

*resume_exec* = _command_
	Execute this command when the trigger deactivates. Optional.

At least one of *exec* and *resume_exec* must be specified.

## TRIGGERS
Each trigger must be one of the following:

*timeout <seconds>*
	Activated after the specified number of seconds of inactivity.
	Deactivated once activity resumes.

*sleep*
	Activated when the system suspends (i.e. "goes to sleep") and
	deactivated when it resumes.

*lock*
	Activated when the system is locked and deactivated when it is
	unlocked. Note that this refers to the LockSession and
	UnlockSession dbus methods of *org.freedesktop.login1(5)*.

## COMMANDS
If a command starts with the string "builtin:", then one of the builtin actions
below will be executed; otherwise, it is assumed to be a shell command, and it
will be executed with "sh -c".

*builtin:dim*
	Dim the monitor's brightness to 0 over a period of five seconds.

*builtin:undim*
	Restore the monitor's brightness from before the dimming started.

*builtin:suspend*
	Suspend the system (i.e. "go to sleep").

*builtin:set_idle_hint*
	Call SetIdleHint(true) on the *org.freedesktop.login1(5)* session
	object to indicate an idle session.

*builtin:unset_idle_hint*
	Call SetIdleHint(false) on the *org.freedesktop.login1(5)* session
	object to indicate that the session is no longer idle.

## SAMPLE CONFIGURATION
```
[Main]
ignore_audio = false
wait_before_sleep = true
disable_automatic_dpms_activation = true
disable_screensaver = true

[Action dim]
trigger = timeout 600
exec = builtin:dim
resume_exec = builtin:undim

[Action turn off monitor]
trigger = timeout 660
exec = xset dpms force off
resume_exec = xset dpms force on

[Action suspend]
trigger = timeout 1200
exec = builtin:suspend

[Action turn on monitor on resume]
trigger = sleep
resume_exec = xset dpms force on

[Action restore brightness on resume]
trigger = sleep
resume_exec = builtin:undim
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
