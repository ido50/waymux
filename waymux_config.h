/*
 * WayMux: Configuration file management
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_WAYMUX_CONFIG_H
#define CG_WAYMUX_CONFIG_H

#include "keybinding.h"
#include <stdbool.h>

/* WayMux configuration structure */
struct waymux_config {
	/* Keybindings - all allocated, use keybinding_free() to clean up */
	struct keybinding *next_tab;
	struct keybinding *prev_tab;
	struct keybinding *close_tab;
	struct keybinding *open_launcher;
	struct keybinding *toggle_background;
	struct keybinding *show_background_dialog;

	/* Path to config file (for logging) */
	char *config_path;
};

/**
 * Load WayMux configuration
 *
 * Searches for config file in this order:
 * 1. Path specified by -c flag (if provided)
 * 2. $XDG_CONFIG_HOME/waymux/config.toml
 * 3. ~/.config/waymux/config.toml (fallback if XDG_CONFIG_HOME not set)
 *
 * If no config file is found, returns a config with defaults.
 * If config file is found but has errors, returns NULL.
 *
 * @param custom_path Optional custom config path from -c flag (can be NULL)
 * @return Config struct, or NULL on parse error (returns defaults if file not found)
 */
struct waymux_config *waymux_config_load(const char *custom_path);

/**
 * Free a WayMux config structure
 */
void waymux_config_free(struct waymux_config *config);

/**
 * Get default keybindings for a specific action
 * @param action Action name: "next_tab", "prev_tab", "close_tab", "open_launcher",
 *               "toggle_background", "show_background_dialog"
 * @return Default keybinding struct (static, do not free)
 */
const struct keybinding *waymux_config_get_default(const char *action);

#endif
