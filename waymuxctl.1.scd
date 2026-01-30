waymuxctl(1)

# NAME

waymuxctl - control client for WayMux

# SYNOPSIS

*waymuxctl* _command_ [_arguments_...]

# DESCRIPTION

waymuxctl is a command-line tool for controlling a running WayMux instance. It
communicates with WayMux via a Unix domain socket, allowing you to manage tabs
from other terminals or scripts.

waymuxctl automatically connects to the WayMux control socket by searching in
the *XDG_RUNTIME_DIR*/waymux directory. If multiple WayMux instances are
running, you can specify which one to control using the *WAYMUX_PID* environment
variable.

# COMMANDS

*list-tabs*
	List all open tabs, showing their index and the application running in
	each tab.

*focus-tab* _NUM_
	Switch to tab _NUM_, where _NUM_ is the tab index as shown by
	*list-tabs*.

*close-tab* [--force] _NUM_
	Close tab _NUM_. If the tab has a running application, WayMux will ask
	for confirmation before closing. Use *--force* to close the tab without
	confirmation.

*new-tab* -- _CMD_ [_args..._]
	Create a new tab and run _CMD_ in it. The *--* separator is required
	to distinguish waymuxctl options from the command to run. Any arguments
	after the command will be passed to the command.

# EXAMPLES

*List all tabs*

```
$ waymuxctl list-tabs
0: foot
1: emacs
2: firefox
```

*Switch to a specific tab*

```
$ waymuxctl focus-tab 1
```

*Close a tab with confirmation*

```
$ waymuxctl close-tab 2
```

*Force-close a tab without confirmation*

```
$ waymuxctl close-tab --force 2
```

*Open a new tab with an application*

```
$ waymuxctl new-tab -- foot
```

*Open a new tab with arguments*

```
$ waymuxctl new-tab -- emacs README.md
```

*Control a specific WayMux instance*

```
$ WAYMUX_PID=1234 waymuxctl list-tabs
```

# ENVIRONMENT

_WAYMUX_PID_
	Specify the PID of the WayMux instance to control. The control socket
	is located at *XDG_RUNTIME_DIR*/waymux/$WAYMUX_PID.sock. If not set,
	waymuxctl will use the first .sock file it finds in the socket directory.

_XDG_RUNTIME_DIR_
	Path to the runtime directory containing the WayMux control socket
	(defaults to */run/user/UID*).

# EXIT STATUS

*0*
	Success.

*1*
	Failure (command not recognized, connection failed, or WayMux returned an
	error).

# SEE ALSO

*waymux*(1)

# BUGS

Report bugs at https://github.com/ido50/waymux

# AUTHORS

Ido Perlmuter <ido@ido50.net>
