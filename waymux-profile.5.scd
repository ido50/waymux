waymux-profile(5)

# NAME

waymux-profile - WayMux profile file format

# DESCRIPTION

WayMux profiles are TOML configuration files that define multiple applications to
launch, shared environment variables, working directory, and other settings.

Profiles allow you to preconfigure a WayMux session with specific applications
and settings, which is useful for creating dedicated workspaces or quickly
launching a set of related applications.

# FILE FORMAT

Profile files use the TOML format. The file must be named with a *.toml*
extension (e.g., *dev.toml*, *work.toml*).

## Top-Level Fields

*working_dir* = _path_
	Sets the working directory for all applications launched from this
	profile. If not specified, applications inherit WayMux's working
	directory.

*proxy_command* = [_command_, _arg_, ...] or _command_
	A command prefix that is prepended to all application commands. This is
	useful for running applications through a wrapper or launcher.

	Can be specified as a string for simple commands (e.g., *"sudo"*) or as
	an array for multi-word commands (e.g., *["uv", "run"]*,
	*["distrobox", "enter", "--"]*).

	When specified as an array, each element becomes a separate argument in
	the command execution.

## The [env] Section

The *[env]* section defines environment variables that are shared by all
applications in the profile. Environment variables are specified as key-value
pairs:

```
[env]
EDITOR = "nvim"
DEBUG = "1"
PATH = "/custom/path:$PATH"
```

## The [[tabs]] Array

The *[[tabs]]* array defines the applications to launch. Each tab entry supports
the following fields:

*command* = _path_ (required)
	The command to execute. This can be an absolute path, a command name
	(relying on *PATH* lookup), or a shell command.

*title* = _string_ (optional)
	A human-readable title for the tab. If not specified, the tab will use
	the application's window title.

*args* = [_arg1_, _arg2_, ...] (optional)
	An array of command-line arguments to pass to the application. Arguments
	are passed directly to the application without shell interpretation.

# EXAMPLES

## Basic Profile

Launch two terminals and a web browser:

```
[[tabs]]
command = "kitty"
title = "Terminal 1"

[[tabs]]
command = "kitty"
title = "Terminal 2"
args = ["-e", "htop"]

[[tabs]]
command = "firefox"
title = "Browser"
```

## Development Profile

Launch development tools with a shared working directory and environment:

```
working_dir = "/home/user/projects/myapp"

[env]
EDITOR = "nvim"
DEBUG = "1"
DATABASE_URL = "postgresql://localhost/myapp"

[[tabs]]
command = "kitty"
title = "Editor"
args = ["-e", "nvim"]

[[tabs]]
command = "kitty"
title = "Tests"
args = ["-e", "cargo", "test"]

[[tabs]]
command = "kitty"
title = "Server"
args = ["-e", "cargo", "run"]

[[tabs]]
command = "firefox"
title = "Docs"
args = ["--new-window", "https://docs.rs"]
```

## Using a Proxy Command

Run applications through a tool runner (array format for multi-word commands):

```
proxy_command = ["uv", "run"]

working_dir = "/home/user/projects/myproject"

[env]
PYTHONPATH = "/home/user/projects/myproject/src"

[[tabs]]
command = "python"
title = "App"
args = ["-m", "myapp.main"]

[[tabs]]
command = "python"
title = "Tests"
args = ["-m", "pytest"]
```

The proxy command can also be a simple string for single-word commands:

```
proxy_command = "sudo"

[[tabs]]
command = "vim"
title = "Editor"
args = ["/etc/config"]
```

## Shell Commands

The *command* field can also be a shell command string:

```
[[tabs]]
command = "sh"
args = ["-c", "cd /tmp && ls -la"]

[[tabs]]
command = "bash"
args = ["-c", "echo 'Hello from WayMux' && sleep 10"]
```

# BEHAVIOR

When WayMux launches a profile:

1. All tabs defined in the profile are launched sequentially
2. Each tab receives the environment variables from the *[env]* section
3. If *working_dir* is set, all tabs use it as their working directory
4. If *proxy_command* is set, it is prepended to each tab's command
5. Launch failures for individual tabs do not prevent other tabs from launching

# FILE LOCATIONS

WayMux searches for profile files in the following locations (in order):

1. The current directory (e.g., *./dev.toml*)
2. *$XDG_CONFIG_HOME/waymux/profiles.d/* (or *~/.config/waymux/profiles.d/* if
	*$XDG_CONFIG_HOME* is not set)

To use a profile, reference it by name without the *.toml* extension:

```
$ waymux dev
```

# SEE ALSO

*waymux*(1), *waymuxctl*(1)

# BUGS

Report bugs at https://github.com/ido50/waymux

# AUTHORS

Ido Perlmuter <ido@ido50.net>
