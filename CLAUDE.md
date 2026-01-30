# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

WayMux uses Meson as its build system.

### Building

```bash
# Configure (debug build by default)
meson setup build

# Build
meson compile -C build

# Release build
meson setup build --buildtype=release
```

### Testing

Tests use the Check framework. Tests must be enabled at configure time:

```bash
# Configure with tests enabled
meson setup build -Dtests=true

# Run all tests
meson compile -C build
ninja -C build test

# Run specific test
./build/desktop_entry_test
./build/tab_test
./build/control_test
./build/waymuxctl_test
```

Note: Some tests currently fail due to known bugs in the implementation. See `test/README.md` for details.

### Dependencies

- wlroots 0.19
- wayland-server
- wayland-protocols
- xkbcommon
- check (for tests, optional)
- scdoc (for man pages, optional)

## Architecture Overview

WayMux is a Wayland compositor that provides tabbed application management, forked from Cage (a kiosk compositor). The core architecture follows the standard wlroots compositor pattern with WayMux-specific tab management layered on top.

### Core Structures

**`cg_server`** (server.h): The main compositor instance. Contains:
- `wl_list views`: List of all views (applications)
- `wl_list tabs`: List of all tabs
- `cg_tab *active_tab`: Currently active tab
- `cg_tab_bar *tab_bar`: UI element showing tabs
- `cg_launcher *launcher`: Application launcher system
- `cg_control_server *control`: Unix socket control server
- Scene graph for rendering (`wlr_scene`)
- Output management for multi-monitor support

**`cg_view`** (view.h): Represents an application window. Abstract base type with implementations for XDG shell and XWayland surfaces. Each view has:
- Scene tree node for rendering
- Position in layout coordinates
- Implementation vtable for type-specific operations

**`cg_tab`** (tab.h): Wrapper around a view that enables tab management:
- Links to server's tab list
- Scene tree for visibility control (only active tab is visible)
- Association with its view

**`cg_tab_bar`** (tab_bar.h): Visual UI element displaying tabs:
- Scene tree with background and buttons
- Mouse click handling for tab switching
- Updates on tab lifecycle changes

**`cg_seat`** (seat.h): Manages input devices (keyboards, pointers, touch):
- Keyboard groups for multiple keyboards
- Cursor management
- Drag-and-drop icons
- Focus management

**`cg_control_server`** (control.h): Unix domain socket server for external control:
- Accepts client connections
- Protocol for tab management commands
- Used by `waymuxctl` CLI tool

### Key Modules

- **`xdg_shell.c`**: Handles XDG shell protocol surfaces (modern Wayland apps)
- **`xwayland.c`**: XWayland support for X11 applications (conditional compilation)
- **`output.c`**: Output/head management and mode configuration
- **`desktop_entry.c`**: XDG desktop entry parsing for application launcher
- **`launcher.c`**: Application launcher UI and interaction
- **`idle_inhibit_v1.c`:** Idle inhibit protocol handler

### Tab Lifecycle

1. Application surfaces arrive via XDG shell or XWayland protocols
2. `view` created and mapped to server's views list
3. `tab` created wrapping the view, added to server's tabs list
4. Tab activated via `tab_activate()`: hides other tabs, shows this tab
5. Tab bar updated to reflect new state
6. On close: `tab_destroy()` closes view and removes from lists

### Multi-Output Mode

WayMux supports two multi-output modes (configured via `-m` flag):
- **extend**: Extend across all connected monitors (default)
- **last**: Use only the last connected monitor

## Source Organization

All source files are in the root directory:
- Main files: `waymux.c` (entry point), `seat.c`, `view.c`, `output.c`
- Tab management: `tab.c`, `tab_bar.c`
- Protocol handlers: `xdg_shell.c`, `xwayland.c`, `idle_inhibit_v1.c`
- Utilities: `desktop_entry.c`, `launcher.c`, `control.c`
- Headers in root: matching `.h` files for each module

## Control Protocol

WayMux provides a Unix socket-based control interface for external commands. The control server listens on a socket and accepts commands from clients like `waymuxctl`. Protocol implementation is in `control.c`.

## Code Style

- C11 standard
- Compiler warnings treated as errors (`-Werror`)
- No unused parameters warnings
- Uses wlroots' scene graph for rendering
- Wayland coding conventions: `wl_list` for lists, `wl_listener` for events
- Prefix: `cg_` (from Cage, kept for consistency with the fork)

## XWayland Support

XWayland support is conditional based on wlroots configuration. Code checks `WAYMUX_HAS_XWAYLAND` macro. When enabled, `xwayland.c` and `xwayland.h` are compiled in.
