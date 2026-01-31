waymux(1)

# NAME

waymux - a Wayland multiplexer compositor

# SYNOPSIS

*waymux* [options...] [_profile_] [--] [_application_...]

# DESCRIPTION

WayMux is a Wayland multiplexer compositor that allows you to run multiple
applications in tabs within a single window. Each tab contains a maximized
application, and you can switch between tabs using keyboard shortcuts or the
tab bar.

WayMux is designed to run inside an existing Wayland compositor (such as sway,
GNOME, or KDE), where it appears as a single window containing your tabbed
applications. This makes it ideal for organizing related applications or
creating dedicated workspaces.

Each tab runs its own application independently, preventing applications from
interacting with each other or with activities outside their tab.

# PROFILES

WayMux can be started with a profile: a TOML configuration file that defines
multiple applications to launch, shared environment variables, working directory,
and other settings.

Profiles are loaded by name (e.g., *dev* for *dev.toml*). WayMux searches for
profile files in:
- The current directory (e.g., *./dev.toml*)
- *$XDG_CONFIG_HOME/waymux/profiles.d/* (or *~/.config/waymux/profiles.d/* if
  *$XDG_CONFIG_HOME* is not set)

See *waymux-profile*(5) for details on the profile file format.

# OPTIONS

*-d*
	Don't draw client side decorations when possible.

*-D*
	Enable debug logging.

*-h*
	Show the help message.

*-c* <path>
	Load configuration from the specified file instead of the default
	location. If not specified, WayMux searches for a config file in
	*$XDG_CONFIG_HOME/waymux/config.toml* or *~/.config/waymux/config.toml*.
	See *waymux-config*(5) for details on the configuration file format.

*-i* <name>
	Set the instance name. Multiple WayMux instances can run simultaneously,
	each with a unique name. The default instance name is *default*.
	See *waymuxctl*(1) for information on controlling specific instances.

*-m* <mode>
	Set the multi-monitor behavior. Supported modes are:
	*last* WayMux uses only the last connected monitor.
	*extend* WayMux extends the display across all connected monitors.

*-s*
	Allow VT switching

*-v*
	Show the version number and exit.

# KEYBOARD SHORTCUTS

WayMux uses keyboard shortcuts for tab management. The default shortcuts use the
Super (Windows/Logo) key as a modifier, but these can be customized in the
configuration file. See *waymux-config*(5) for details.

The default shortcuts are:

*Super+J*
	Switch to the previous tab.

*Super+K*
	Switch to the next tab.

*Super+D*
	Close the current tab. If the tab has a running application, WayMux will
	ask for confirmation before closing. Use *waymuxctl close-tab --force*
	to close without confirmation.

*Super+B*
	Toggle the current tab as a background tab. Background tabs are hidden
	from the tab bar but remain running. They can be accessed via the
	background tabs dialog (Super+Shift+B) or via *waymuxctl*.

*Super+Shift+B*
	Show the background tabs dialog. The dialog displays all background tabs
	(hot tabs hidden from the tab bar). Type to filter, use arrow keys to
	navigate, press Enter to bring the selected tab to the foreground and
	activate it, or press Escape to cancel.

*Super+N*
	Show the application launcher. The launcher displays available applications
	from your XDG desktop entries. Type to filter, use arrow keys to navigate,
	press Enter to launch in a new tab, or press Escape to cancel.

In the launcher:

*Arrow Keys*
	Navigate through the search results.

*Enter*
	Launch the selected application in a new tab.

*Escape*
	Close the launcher without launching an application.

# EXAMPLES

*Start WayMux with an initial application*

```
$ waymux foot
```

*Start WayMux with multiple initial applications*

```
$ waymux foot -- emacs
```

*Start WayMux with a profile*

```
$ waymux dev
```

*Start WayMux with a profile and an additional application*

```
$ waymux dev -- foot
```

*Start WayMux on the last connected monitor only*

```
$ waymux -m last foot
```

*Start WayMux with debug logging enabled*

```
$ waymux -D foot
```

*Run WayMux without client-side decorations*

```
$ waymux -d foot
```

*Start a named WayMux instance*

```
$ waymux -i work dev
```

*Start multiple WayMux instances simultaneously*

```
$ waymux -i work dev &
$ waymux -i personal foot &
```

For controlling WayMux from other terminals or scripts, see *waymuxctl*(1).
For information on writing profile files, see *waymux-profile*(5).

# ENVIRONMENT

_DISPLAY_
	If compiled with Xwayland support, this will be set to the name of the
	X display used for Xwayland. Otherwise, probe the X11 backend.

_WAYLAND_DISPLAY_
	Specifies the name of the Wayland display that WayMux is running on.

_XCURSOR_PATH_
	Directory where cursors are located.

_XCURSOR_SIZE_
	Specifies the configured cursor size.

_XCURSOR_THEME_
	Specifies the configured cursor theme.

_XKB_DEFAULT_RULES_, _XKB_DEFAULT_MODEL_, _XKB_DEFAULT_LAYOUT_,
_XKB_DEFAULT_VARIANT_, _XKB_DEFAULT_OPTIONS_
	Configures the xkb keyboard settings. See *xkeyboard-config*(7).

# SEE ALSO

*waymuxctl*(1), *waymux-config*(5), *waymux-profile*(5), *xkeyboard-config*(7)

# BUGS

Report bugs at https://github.com/ido50/waymux

# AUTHORS

Ido Perlmuter <ido@ido50.net>

WayMux is a fork of Cage, originally written by Jente Hidskes <dev@hjdskes.nl>.
