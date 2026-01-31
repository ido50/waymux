waymux-config(5)

# NAME

waymux-config - WayMux configuration file format

# DESCRIPTION

WayMux can be configured using a TOML configuration file. This file allows you to
customize keyboard shortcuts and other settings.

# CONFIG FILE LOCATION

WayMux searches for the configuration file in the following order:

1. Path specified with the *-c* command-line option
2. *$XDG_CONFIG_HOME/waymux/config.toml*
3. *~/.config/waymux/config.toml* (used only if *$XDG_CONFIG_HOME* is not set)

If no configuration file is found, WayMux uses the default keybindings.

# FILE FORMAT

The configuration file uses TOML format. Currently, the only supported section is
*[keybindings]*, which allows you to customize keyboard shortcuts.

## KEYBINDINGS SECTION

The *[keybindings]* section defines keyboard shortcuts for WayMux commands. Each
keybinding is specified as a string in the form *"Modifier+Key"*, where:

- *Modifier* (optional): One or more of *Super*, *Ctrl*, *Alt*, *Shift*
- *Key*: An XKB key name (case-insensitive)

Multiple modifiers can be combined with *+*. For example: *"Super+Shift+B"*.

The following keybindings can be configured:

*next_tab*
	Switch to the next tab.
	Default: *"Super+K"*

*prev_tab*
	Switch to the previous tab.
	Default: *"Super+J"*

*close_tab*
	Close the current tab. If the tab has a running application, WayMux will
	ask for confirmation before closing.
	Default: *"Super+D"*

*open_launcher*
	Show the application launcher.
	Default: *"Super+N"*

*toggle_background*
	Toggle the current tab as a background tab. Background tabs are hidden
	from the tab bar but remain running.
	Default: *"Super+B"*

*show_background_dialog*
	Show the background tabs dialog. The dialog displays all background tabs
	(hot tabs hidden from the tab bar).
	Default: *"Super+Shift+B"*

# EXAMPLES

## Default Configuration

The following configuration shows the default keybindings:

```
[keybindings]
next_tab = "Super+K"
prev_tab = "Super+J"
close_tab = "Super+D"
open_launcher = "Super+N"
toggle_background = "Super+B"
show_background_dialog = "Super+Shift+B"
```

## Custom Keybindings

To customize keybindings, create a configuration file with only the bindings you
wish to change. Unspecified bindings will use their defaults:

```
[keybindings]
next_tab = "Ctrl+Tab"
prev_tab = "Ctrl+Shift+Tab"
close_tab = "Ctrl+W"
open_launcher = "Super+Space"
```

## Using Ctrl for Tab Navigation

Some users prefer Emacs-style keybindings:

```
[keybindings]
next_tab = "Ctrl+X" "Ctrl+N"
prev_tab = "Ctrl+X" "Ctrl+P"
close_tab = "Ctrl+X" "Ctrl+K"
```

Note: Multi-key sequences like *"Ctrl+X Ctrl+N"* are not currently supported.
Use single key combinations instead.

## Vim-Style Keybindings

For users who prefer Vim-style navigation:

```
[keybindings]
next_tab = "Ctrl+N"
prev_tab = "Ctrl+P"
close_tab = "Ctrl+D"
open_launcher = "Ctrl+P"
toggle_background = "Ctrl+B"
```

# KEY NAMES

WayMux uses XKB key names. Common key names include:

- Letters: *A* through *Z*
- Numbers: *0* through *9*
- Function keys: *F1* through *F12*
- Special keys: *Escape*, *Return*, *Space*, *Tab*, *BackSpace*
- Arrow keys: *Up*, *Down*, *Left*, *Right*
- Punctuation: *comma*, *period*, *semicolon*, *quote*, *backslash*, etc.

For a complete list of XKB key names, see the XKB specification documentation.

# SEE ALSO

*waymux*(1), *waymuxctl*(1)

# BUGS

Report bugs at https://github.com/ido50/waymux

# AUTHORS

Ido Perlmuter <ido@ido50.net>
