/*
 * Waymux: Profile management
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include "profile.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wlr/util/log.h>
#include <tomlc17.h>

static char *
find_profile_file(const char *name)
{
	if (!name) {
		return NULL;
	}

	/* Try current directory first */
	char *path = NULL;
	struct stat st;

	/* Check ./name.toml */
	size_t len = strlen(name) + 9; /* "./" + ".toml" + null */
	path = malloc(len);
	if (!path) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate path");
		return NULL;
	}
	snprintf(path, len, "./%s.toml", name);

	if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
		return path;
	}
	free(path);

	/* Try $XDG_CONFIG_HOME/waymux/profiles.d/name.toml */
	/* Default to ~/.config if XDG_CONFIG_HOME is not set */
	const char *config_home = getenv("XDG_CONFIG_HOME");
	if (!config_home || config_home[0] == '\0') {
		const char *home = getenv("HOME");
		if (!home) {
			wlr_log(WLR_ERROR, "HOME environment variable not set");
			return NULL;
		}
		/* Allocate enough space for ~/.config/waymux/profiles.d/name.toml */
		len = strlen(home) + strlen(name) + 33;
		path = malloc(len);
		if (!path) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate path");
			return NULL;
		}
	snprintf(path, len, "%s/.config/waymux/profiles.d/%s.toml", home, name);
	} else {
		len = strlen(config_home) + strlen(name) + 25;
		path = malloc(len);
		if (!path) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate path");
			return NULL;
		}
		snprintf(path, len, "%s/waymux/profiles.d/%s.toml", config_home, name);
	}

	if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
		return path;
	}

	wlr_log(WLR_ERROR, "Profile file not found: %s.toml", name);
	free(path);
	return NULL;
}

static char *
dup_string(const char *s)
{
	if (!s) {
		return NULL;
	}
	return strdup(s);
}

static char **
dup_string_array(toml_datum_t *arr, int *out_count)
{
	if (!arr || arr->type != TOML_ARRAY) {
		*out_count = 0;
		return NULL;
	}

	int count = arr->u.arr.size;
	char **result = calloc(count + 1, sizeof(char *));
	if (!result) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate string array");
		*out_count = 0;
		return NULL;
	}

	for (int i = 0; i < count; i++) {
		toml_datum_t *elem = &arr->u.arr.elem[i];
		if (elem->type == TOML_STRING) {
			result[i] = strdup(elem->u.s);
			if (!result[i]) {
				wlr_log_errno(WLR_ERROR, "Failed to duplicate string");
				/* Free previously allocated strings */
				for (int j = 0; j < i; j++) {
					free(result[j]);
				}
				free(result);
				*out_count = 0;
				return NULL;
			}
		} else {
			/* Non-string element, skip */
			result[i] = NULL;
		}
	}
	result[count] = NULL;
	*out_count = count;
	return result;
}

static void
free_profile_tab(struct profile_tab *tab)
{
	if (!tab) {
		return;
	}
	free(tab->command);
	free(tab->title);
	if (tab->args) {
		for (int i = 0; i < tab->argc; i++) {
			free(tab->args[i]);
		}
		free(tab->args);
	}
}

struct profile *
profile_load(const char *name)
{
	if (!name) {
		wlr_log(WLR_ERROR, "profile name is NULL");
		return NULL;
	}

	char *path = find_profile_file(name);
	if (!path) {
		return NULL;
	}

	wlr_log(WLR_DEBUG, "Loading profile from: %s", path);

	toml_result_t result = toml_parse_file_ex(path);
	free(path);

	if (!result.ok) {
		wlr_log(WLR_ERROR, "Failed to parse profile: %s", result.errmsg);
		toml_free(result);
		return NULL;
	}

	struct profile *profile = calloc(1, sizeof(struct profile));
	if (!profile) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate profile");
		toml_free(result);
		return NULL;
	}

	profile->name = strdup(name);
	if (!profile->name) {
		wlr_log_errno(WLR_ERROR, "Failed to duplicate profile name");
		free(profile);
		toml_free(result);
		return NULL;
	}

	toml_datum_t root = result.toptab;

	/* Parse working_dir (optional) */
	toml_datum_t wd = toml_get(root, "working_dir");
	if (wd.type == TOML_STRING) {
		profile->working_dir = dup_string(wd.u.s);
	}

	/* Parse proxy_command (optional) - can be a string or an array */
	toml_datum_t pc = toml_get(root, "proxy_command");
	if (pc.type == TOML_STRING) {
		/* Legacy support for single string proxy_command */
		profile->proxy_argc = 1;
		profile->proxy_command = calloc(2, sizeof(char *));
		if (!profile->proxy_command) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate proxy command");
			profile_free(profile);
			toml_free(result);
			return NULL;
		}
		profile->proxy_command[0] = dup_string(pc.u.s);
		profile->proxy_command[1] = NULL;
	} else if (pc.type == TOML_ARRAY && pc.u.arr.size > 0) {
		/* New array format for proxy_command */
		profile->proxy_command = dup_string_array(&pc, &profile->proxy_argc);
		if (!profile->proxy_command) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate proxy command array");
			profile_free(profile);
			toml_free(result);
			return NULL;
		}
	}

	/* Parse [env] table (optional) */
	toml_datum_t env = toml_get(root, "env");
	if (env.type == TOML_TABLE && env.u.tab.size > 0) {
		profile->env_count = env.u.tab.size;
		profile->env_vars = calloc(profile->env_count, sizeof(struct profile_env));
		if (!profile->env_vars) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate env vars");
			profile_free(profile);
			toml_free(result);
			return NULL;
		}

		for (int i = 0; i < env.u.tab.size; i++) {
			profile->env_vars[i].key = strndup(env.u.tab.key[i], env.u.tab.len[i]);
			toml_datum_t *val = &env.u.tab.value[i];
			if (val->type == TOML_STRING) {
				profile->env_vars[i].value = dup_string(val->u.s);
			} else {
				profile->env_vars[i].value = NULL;
			}
		}
	}

	/* Parse [[tabs]] array (required for usefulness, but technically optional) */
	toml_datum_t tabs = toml_get(root, "tabs");
	if (tabs.type == TOML_ARRAY && tabs.u.arr.size > 0) {
		profile->tab_count = tabs.u.arr.size;
		profile->tabs = calloc(profile->tab_count, sizeof(struct profile_tab));
		if (!profile->tabs) {
			wlr_log_errno(WLR_ERROR, "Failed to allocate tabs");
			profile_free(profile);
			toml_free(result);
			return NULL;
		}

		for (int i = 0; i < tabs.u.arr.size; i++) {
			toml_datum_t *tab_datum = &tabs.u.arr.elem[i];
			if (tab_datum->type != TOML_TABLE) {
				wlr_log(WLR_ERROR, "Tab at index %d is not a table", i);
				continue;
			}

			struct profile_tab *tab = &profile->tabs[i];

			/* command is required */
			toml_datum_t cmd = toml_get(*tab_datum, "command");
			if (cmd.type != TOML_STRING) {
				wlr_log(WLR_ERROR, "Tab at index %d missing command", i);
				continue;
			}
			tab->command = dup_string(cmd.u.s);

			/* title is optional */
			toml_datum_t title = toml_get(*tab_datum, "title");
			if (title.type == TOML_STRING) {
				tab->title = dup_string(title.u.s);
			}

			/* args is optional */
			toml_datum_t args = toml_get(*tab_datum, "args");
			if (args.type == TOML_ARRAY) {
				tab->args = dup_string_array(&args, &tab->argc);
			}
		}
	} else {
		wlr_log(WLR_INFO, "Profile has no tabs defined");
	}

	toml_free(result);

	wlr_log(WLR_INFO, "Loaded profile '%s' with %d tabs", name, profile->tab_count);

	return profile;
}

void
profile_free(struct profile *profile)
{
	if (!profile) {
		return;
	}

	free(profile->name);
	free(profile->working_dir);

	/* Free proxy command */
	if (profile->proxy_command) {
		for (int i = 0; i < profile->proxy_argc; i++) {
			free(profile->proxy_command[i]);
		}
		free(profile->proxy_command);
	}

	/* Free env vars */
	if (profile->env_vars) {
		for (int i = 0; i < profile->env_count; i++) {
			free(profile->env_vars[i].key);
			free(profile->env_vars[i].value);
		}
		free(profile->env_vars);
	}

	/* Free tabs */
	if (profile->tabs) {
		for (int i = 0; i < profile->tab_count; i++) {
			free_profile_tab(&profile->tabs[i]);
		}
		free(profile->tabs);
	}

	free(profile);
}
