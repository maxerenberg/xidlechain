These are some simple eyeball tests for the different events that xidlechain
can receive. Run `make tests` from the root directory to build them.

The ActivityManager test should print a timeout event after 2 and 5 seconds
of inactivity, as well as a resume event.

The LogindManager test should print Lock, Unlock, Sleep and Wake events.
Locking/unlocking the system can be triggered using `loginctl lock-session`
and `loginctl unlock-session`. Suspending the system can be triggered using
`systemctl suspend`.
