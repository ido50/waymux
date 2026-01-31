# WayMux: a Wayland Multiplexer Compositor

WayMux is a tabbed Wayland compositor that allows you to run multiple applications
in tabs within a single window. It's a fork of [Cage](https://github.com/cage-kiosk/cage)
enhanced with tab management, application launching, and profile support.

## Why WayMux?

I created WayMux for my own personal use. My desktop setup uses Hyprland with the
Master Layout - I have a permanent stack area with two windows that I always want
displayed, and I want the master area to always show one main app.

When that main app isn't in use, I don't want it in the stack nor in another
workspace. That's why WayMux has a tabbed interface - it keeps related apps
organized in a single window that I can switch between as needed.

I also often need to start background processes that I don't want to see anymore
(for example, when working on a project, I might start an HTTP server to serve
local files). I don't want that server window visible, and I don't want to have
to skip it every time I cycle through windows. WayMux's background tabs feature
solves this - tabs can be hidden from the tab bar while continuing to run.

## Features

- **Tabbed interface**: Run multiple applications in a single window with easy tab switching
- **Background tabs**: Hide tabs from the tab bar while keeping them running
- **Application launcher**: Quick access to all installed applications via XDG desktop entries
- **Profiles**: Preconfigure sessions with multiple applications, environment variables, and settings
- **Multiple instances**: Run several WayMux instances simultaneously, each with a unique name
- **Keyboard-driven**: Full keyboard control for tab management and application launching
- **Wayland native**: Built on wlroots for a pure Wayland experience

## Installation

### Arch Linux (AUR)

WayMux is available as `waymux-bin` in the AUR:

```bash
paru -S waymux-bin
# or
yay -S waymux-bin
```

You can also download prebuilt packages from the [Releases](https://github.com/ido50/waymux/releases) page.

### From Source

WayMux uses the [Meson](https://mesonbuild.com/) build system and requires:
- wlroots 0.19
- wayland-protocols
- libxkbcommon
- cairo
- tomlc17
- scdoc (for man pages, optional)
- check (for tests, optional)

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

For a release build:

```bash
meson setup build --buildtype=release
```

## Usage

### Basic Usage

Start WayMux with an application:

```bash
waymux foot
```

Start with multiple applications:

```bash
waymux foot -- emacs
```

### Keyboard Shortcuts

WayMux uses the Super (Windows/Logo) key as the default modifier:

| Shortcut | Action |
|----------|--------|
| `Super+J` | Switch to previous tab |
| `Super+K` | Switch to next tab |
| `Super+D` | Close current tab (with confirmation) |
| `Super+B` | Toggle current tab as background tab |
| `Super+Shift+B` | Show/hide background tabs dialog |
| `Super+N` | Show application launcher |

In dialogs and launchers:
- Arrow keys to navigate
- Enter to select
- Escape to cancel

### Profiles

Profiles are TOML configuration files that define multiple applications to launch
with shared environment and settings. This is particularly useful for my work as
an independent contractor - I can have a profile per company, so on days I work
for company A, I open company A's profile with all the relevant apps.

Create a profile in `~/.config/waymux/profiles.d/` (e.g., `work.toml`):

```toml
working_dir = "~/projects/company-a"

[env]
EDITOR = "nvim"
DATABASE_URL = "postgresql://localhost/companya"

[[tabs]]
command = "kitty"
title = "Terminal"

[[tabs]]
command = "kitty"
title = "Editor"
args = ["-e", "nvim"]

[[tabs]]
command = "firefox"
title = "Documentation"
```

Start WayMux with a profile:

```bash
waymux work
```

See `waymux-profile(5)` for more details on profile configuration.

### Profile Selector

The `-P` flag shows an interactive profile selector dialog that lists all
available profiles from `~/.config/waymux/profiles.d/`:

```bash
waymux -P
```

This is the default behavior of the `.desktop` file installed by WayMux,
making it easy to launch different profiles from your application menu.

### Creating Profile-Specific Desktop Entries

To create a desktop entry that launches a specific profile, copy the default
WayMux desktop file and modify the `Exec` line:

```bash
# Copy the default desktop file
cp /usr/share/applications/waymux.desktop ~/.local/share/applications/waymux-work.desktop

# Edit the file to change the name and Exec line
```

In `~/.local/share/applications/waymux-work.desktop`:

```ini
[Desktop Entry]
Type=Application
Name=WayMux (Work Profile)
Comment=Tabbed Wayland Compositor - Work Profile
Exec=waymux work
Icon=waymux
Terminal=false
Categories=System;WM;
Keywords=wayland;compositor;window manager;
```

### Multiple Instances

WayMux can run multiple instances simultaneously using instance names:

```bash
waymux -i work dev &
waymux -i personal foot &
```

Control specific instances with `waymuxctl`:

```bash
waymuxctl -i work list-tabs
waymuxctl -i personal focus-tab 0
```

### Background Tabs

Tabs can be hidden from the tab bar while continuing to run:

- Press `Super+B` to toggle the current tab as a background tab
- Press `Super+Shift+B` to show the background tabs dialog
- Use `waymuxctl background/foreground` to manage background tabs from scripts

This is useful for long-running processes you don't want to see anymore
(servers, monitors, etc.).

## Documentation

WayMux includes comprehensive man pages:

- `waymux(1)` - Main application and usage
- `waymuxctl(1)` - Control client for managing WayMux
- `waymux-config(5)` - Configuration file format
- `waymux-profile(5)` - Profile file format

Example:

```bash
man waymux
man waymux-profile
```

## About

WayMux is a fork of [Cage](https://github.com/cage-kiosk/cage), a Wayland kiosk
compositor originally written by Jente Hidskes. WayMux extends Cage with tab
management, application launching, profiles, and instance management.

### Cage Attribution

WayMux is a fork of Cage, originally written by Jente Hidskes <dev@hjdskes.nl>.
Cage is based on the annotated source of tinywl and rootston.

## License

WayMux is licensed under the same license as Cage - please see the [LICENSE](LICENSE)
file for details.

## Bugs

Report bugs at https://github.com/ido50/waymux/issues

## Author

Ido Perlmuter <ido@ido50.net>
