/*
 * Waymux: Profile management
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_PROFILE_H
#define CG_PROFILE_H

#include <stdbool.h>

/* Profile structure representing a parsed TOML profile */
struct profile {
	char *name;
	char *working_dir;
	char *proxy_command;
};

/**
 * Load a profile by name
 * Searches for name.toml in current directory, then $XDG_CONFIG_HOME/waymux/profiles.d
 * Returns NULL on failure
 */
struct profile *profile_load(const char *name);

/**
 * Free a profile structure
 */
void profile_free(struct profile *profile);

#endif
