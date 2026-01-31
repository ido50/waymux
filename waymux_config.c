/*
 * WayMux: Configuration file management
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include "waymux_config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wlr/util/log.h>
#include <tomlc17.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

/* Default keybindings as static constants */
static const struct keybinding default_next_tab = {
	WLR_MODIFIER_LOGO, XKB_KEY_k
};
static const struct keybinding default_prev_tab = {
	WLR_MODIFIER_LOGO, XKB_KEY_j
};
static const struct keybinding default_close_tab = {
	WLR_MODIFIER_LOGO, XKB_KEY_d
};
static const struct keybinding default_open_launcher = {
	WLR_MODIFIER_LOGO, XKB_KEY_n
};
static const struct keybinding default_toggle_background = {
	WLR_MODIFIER_LOGO, XKB_KEY_b
};
static const struct keybinding default_show_background_dialog = {
	WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_b
};

const struct keybinding *
waymux_config_get_default(const char *action)
{
	if (strcmp(action, "next_tab") == 0) {
		return &default_next_tab;
	} else if (strcmp(action, "prev_tab") == 0) {
		return &default_prev_tab;
	} else if (strcmp(action, "close_tab") == 0) {
		return &default_close_tab;
	} else if (strcmp(action, "open_launcher") == 0) {
		return &default_open_launcher;
	} else if (strcmp(action, "toggle_background") == 0) {
		return &default_toggle_background;
	} else if (strcmp(action, "show_background_dialog") == 0) {
		return &default_show_background_dialog;
	}
	return NULL;
}

static char *
find_config_file(const char *custom_path)
{
	/* If custom path provided, try it first */
	if (custom_path) {
		struct stat st;
		if (stat(custom_path, &st) == 0 && S_ISREG(st.st_mode)) {
			wlr_log(WLR_DEBUG, "Using custom config path: %s", custom_path);
			return strdup(custom_path);
		}
		wlr_log(WLR_ERROR, "Custom config path not found: %s", custom_path);
		return NULL;
	}

	/* Try $XDG_CONFIG_HOME/waymux/config.toml */
	const char *config_home = getenv("XDG_CONFIG_HOME");
	char *path = NULL;
	struct stat st;

	if (config_home && config_home[0] != '\0') {
		size_t len = strlen(config_home) + 20; /* "/waymux/config.toml" + null */
		path = malloc(len);
		if (!path) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate config path");
			return NULL;
		}
		snprintf(path, len, "%s/waymux/config.toml", config_home);

		if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
			wlr_log(WLR_DEBUG, "Found config at: %s", path);
			return path;
		}
		free(path);
	}

	/* Try ~/.config/waymux/config.toml */
	const char *home = getenv("HOME");
	if (!home) {
		wlr_log(WLR_ERROR, "HOME environment variable not set");
		return NULL;
	}

	size_t len = strlen(home) + 29; /* "/.config/waymux/config.toml" + null */
	path = malloc(len);
	if (!path) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate config path");
		return NULL;
	}
	snprintf(path, len, "%s/.config/waymux/config.toml", home);

	if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
		wlr_log(WLR_DEBUG, "Found config at: %s", path);
		return path;
	}

	wlr_log(WLR_INFO, "No config file found, using defaults");
	free(path);
	return NULL;
}

static struct keybinding *
parse_keybinding_from_table(toml_datum_t *table, const char *key)
{
	toml_datum_t value = toml_get(*table, key);
	if (value.type != TOML_STRING) {
		return NULL;
	}

	struct keybinding *binding = calloc(1, sizeof(struct keybinding));
	if (!binding) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate keybinding");
		return NULL;
	}

	if (!keybinding_parse(value.u.s, binding)) {
		wlr_log(WLR_ERROR, "Failed to parse keybinding '%s' for '%s'", value.u.s, key);
		free(binding);
		return NULL;
	}

	return binding;
}

static void
apply_keybinding_defaults(struct waymux_config *config)
{
	if (!config->next_tab) {
		config->next_tab = calloc(1, sizeof(struct keybinding));
		if (config->next_tab) {
			*config->next_tab = default_next_tab;
		}
	}
	if (!config->prev_tab) {
		config->prev_tab = calloc(1, sizeof(struct keybinding));
		if (config->prev_tab) {
			*config->prev_tab = default_prev_tab;
		}
	}
	if (!config->close_tab) {
		config->close_tab = calloc(1, sizeof(struct keybinding));
		if (config->close_tab) {
			*config->close_tab = default_close_tab;
		}
	}
	if (!config->open_launcher) {
		config->open_launcher = calloc(1, sizeof(struct keybinding));
		if (config->open_launcher) {
			*config->open_launcher = default_open_launcher;
		}
	}
	if (!config->toggle_background) {
		config->toggle_background = calloc(1, sizeof(struct keybinding));
		if (config->toggle_background) {
			*config->toggle_background = default_toggle_background;
		}
	}
	if (!config->show_background_dialog) {
		config->show_background_dialog = calloc(1, sizeof(struct keybinding));
		if (config->show_background_dialog) {
			*config->show_background_dialog = default_show_background_dialog;
		}
	}
}

struct waymux_config *
waymux_config_load(const char *custom_path)
{
	char *config_path = find_config_file(custom_path);

	/* If no config file found, return defaults */
	if (!config_path) {
		wlr_log(WLR_INFO, "Using default keybindings");
		struct waymux_config *config = calloc(1, sizeof(struct waymux_config));
		if (!config) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate config");
			return NULL;
		}
		config->config_path = NULL;
		apply_keybinding_defaults(config);
		return config;
	}

	wlr_log(WLR_INFO, "Loading config from: %s", config_path);

	toml_result_t result = toml_parse_file_ex(config_path);
	free(config_path);

	if (!result.ok) {
		wlr_log(WLR_ERROR, "Failed to parse config file: %s", result.errmsg);
		toml_free(result);
		return NULL;
	}

	struct waymux_config *config = calloc(1, sizeof(struct waymux_config));
	if (!config) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate config");
		toml_free(result);
		return NULL;
	}

	toml_datum_t root = result.toptab;

	/* Parse [keybindings] table (optional) */
	toml_datum_t keybindings = toml_get(root, "keybindings");
	if (keybindings.type == TOML_TABLE) {
		config->next_tab = parse_keybinding_from_table(&keybindings, "next_tab");
		config->prev_tab = parse_keybinding_from_table(&keybindings, "prev_tab");
		config->close_tab = parse_keybinding_from_table(&keybindings, "close_tab");
		config->open_launcher = parse_keybinding_from_table(&keybindings, "open_launcher");
		config->toggle_background = parse_keybinding_from_table(&keybindings, "toggle_background");
		config->show_background_dialog = parse_keybinding_from_table(&keybindings, "show_background_dialog");

		/* If user specified a binding but it failed to parse, that's an error */
		/* We need to check if the key was present but failed */
		if (toml_get(keybindings, "next_tab").type == TOML_STRING && !config->next_tab) {
			wlr_log(WLR_ERROR, "Invalid keybinding for next_tab");
			goto error;
		}
		if (toml_get(keybindings, "prev_tab").type == TOML_STRING && !config->prev_tab) {
			wlr_log(WLR_ERROR, "Invalid keybinding for prev_tab");
			goto error;
		}
		if (toml_get(keybindings, "close_tab").type == TOML_STRING && !config->close_tab) {
			wlr_log(WLR_ERROR, "Invalid keybinding for close_tab");
			goto error;
		}
		if (toml_get(keybindings, "open_launcher").type == TOML_STRING && !config->open_launcher) {
			wlr_log(WLR_ERROR, "Invalid keybinding for open_launcher");
			goto error;
		}
		if (toml_get(keybindings, "toggle_background").type == TOML_STRING && !config->toggle_background) {
			wlr_log(WLR_ERROR, "Invalid keybinding for toggle_background");
			goto error;
		}
		if (toml_get(keybindings, "show_background_dialog").type == TOML_STRING && !config->show_background_dialog) {
			wlr_log(WLR_ERROR, "Invalid keybinding for show_background_dialog");
			goto error;
		}
	}

	toml_free(result);

	/* Apply defaults for any keybindings not specified in config */
	apply_keybinding_defaults(config);

	wlr_log(WLR_INFO, "Config loaded successfully");

	return config;

error:
	toml_free(result);
	waymux_config_free(config);
	return NULL;
}

void
waymux_config_free(struct waymux_config *config)
{
	if (!config) {
		return;
	}

	free(config->config_path);
	free(config->next_tab);
	free(config->prev_tab);
	free(config->close_tab);
	free(config->open_launcher);
	free(config->toggle_background);
	free(config->show_background_dialog);
	free(config);
}
