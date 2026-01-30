waymux(1)

# NAME

waymux - a Wayland multiplexer compositor

# SYNOPSIS

*waymux* [options...] [--] [_application_...]

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

# OPTIONS

*-d*
	Don't draw client side decorations when possible.

*-D*
	Enable debug logging.

*-h*
	Show the help message.

*-m* <mode>
	Set the multi-monitor behavior. Supported modes are:
	*last* WayMux uses only the last connected monitor.
	*extend* WayMux extends the display across all connected monitors.

*-s*
	Allow VT switching

*-v*
	Show the version number and exit.

# KEYBOARD SHORTCUTS

WayMux uses the Super (Windows/Logo) key as a modifier for tab management
commands:

*Super+J*
	Switch to the previous tab.

*Super+K*
	Switch to the next tab.

*Super+D*
	Close the current tab. If the tab has a running application, WayMux will
	ask for confirmation before closing. Use *waymuxctl close-tab --force*
	to close without confirmation.

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

For controlling WayMux from other terminals or scripts, see *waymuxctl*(1).

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

*waymuxctl*(1), *xkeyboard-config*(7)

# BUGS

Report bugs at https://github.com/ido50/waymux

# AUTHORS

Ido Perlmuter <ido@ido50.net>

WayMux is a fork of Cage, originally written by Jente Hidskes <dev@hjdskes.nl>.
