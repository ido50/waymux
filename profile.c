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
#include <wlr/util/log.h>

struct profile *
profile_load(const char *name)
{
	if (!name) {
		wlr_log(WLR_ERROR, "profile name is NULL");
		return NULL;
	}

	struct profile *profile = calloc(1, sizeof(struct profile));
	if (!profile) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate profile");
		return NULL;
	}

	profile->name = strdup(name);
	if (!profile->name) {
		wlr_log_errno(WLR_ERROR, "Failed to duplicate profile name");
		free(profile);
		return NULL;
	}

	wlr_log(WLR_DEBUG, "Profile loading not yet implemented: %s", name);

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
	free(profile->proxy_command);
	free(profile);
}
