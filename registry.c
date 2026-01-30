/*
 * Waymux: Instance registry for multi-instance support
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include "registry.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <tomlc17.h>

#include "server.h"

#define REGISTRY_DIR "/waymux/registry"

/* Get the registry directory path */
static char *
get_registry_dir_path(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR not set");
		return NULL;
	}

	size_t len = strlen(runtime_dir) + sizeof(REGISTRY_DIR);
	char *path = malloc(len);
	if (!path) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate registry dir path");
		return NULL;
	}

	snprintf(path, len, "%s%s", runtime_dir, REGISTRY_DIR);
	return path;
}

/* Get the registry file path for an instance */
static char *
get_registry_file_path(const char *instance_name)
{
	char *registry_dir = get_registry_dir_path();
	if (!registry_dir) {
		return NULL;
	}

	size_t len = strlen(registry_dir) + strlen(instance_name) + 7; /* "/" + ".toml" + null */
	char *path = malloc(len);
	if (!path) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate registry file path");
		free(registry_dir);
		return NULL;
	}

	snprintf(path, len, "%s/%s.toml", registry_dir, instance_name);
	free(registry_dir);
	return path;
}

/* Ensure the registry directory exists */
static bool
ensure_registry_dir(void)
{
	char *registry_dir = get_registry_dir_path();
	if (!registry_dir) {
		return false;
	}

	/* Create directory if it doesn't exist */
	if (mkdir(registry_dir, 0755) != 0) {
		if (errno != EEXIST) {
			wlr_log_errno(WLR_ERROR, "Failed to create registry directory: %s", registry_dir);
			free(registry_dir);
			return false;
		}
	}

	free(registry_dir);
	return true;
}

bool
registry_register_instance(struct cg_server *server)
{
	if (!server || !server->instance_name) {
		wlr_log(WLR_ERROR, "Invalid server or instance name");
		return false;
	}

	/* Ensure registry directory exists */
	if (!ensure_registry_dir()) {
		return false;
	}

	/* Check if this instance is already registered (shouldn't happen) */
	char *registry_file = get_registry_file_path(server->instance_name);
	if (!registry_file) {
		return false;
	}

	/* Check if file already exists */
	struct stat st;
	if (stat(registry_file, &st) == 0) {
		wlr_log(WLR_ERROR, "Instance '%s' is already registered", server->instance_name);
		free(registry_file);
		return false;
	}

	/* Create the registry file */
	FILE *f = fopen(registry_file, "w");
	if (!f) {
		wlr_log_errno(WLR_ERROR, "Failed to create registry file: %s", registry_file);
		free(registry_file);
		return false;
	}

	/* Write instance metadata */
	fprintf(f, "name = \"%s\"\n", server->instance_name);
	fprintf(f, "pid = %d\n", getpid());

	if (server->profile_name) {
		fprintf(f, "profile = \"%s\"\n", server->profile_name);
	}

	fclose(f);
	wlr_log(WLR_INFO, "Registered instance '%s' in registry", server->instance_name);

	free(registry_file);
	return true;
}

bool
registry_unregister_instance(struct cg_server *server)
{
	if (!server || !server->instance_name) {
		wlr_log(WLR_ERROR, "Invalid server or instance name");
		return false;
	}

	char *registry_file = get_registry_file_path(server->instance_name);
	if (!registry_file) {
		return false;
	}

	/* Remove the registry file */
	if (unlink(registry_file) != 0) {
		if (errno == ENOENT) {
			wlr_log(WLR_DEBUG, "Registry file for instance '%s' does not exist (already unregistered?)",
				server->instance_name);
		} else {
			wlr_log_errno(WLR_ERROR, "Failed to remove registry file: %s", registry_file);
			free(registry_file);
			return false;
		}
	} else {
		wlr_log(WLR_INFO, "Unregistered instance '%s' from registry", server->instance_name);
	}

	free(registry_file);
	return true;
}

bool
registry_is_profile_locked(const char *profile_name)
{
	if (!profile_name) {
		return false; /* No profile means no lock */
	}

	char *registry_dir = get_registry_dir_path();
	if (!registry_dir) {
		return false;
	}

	/* Scan registry directory for instances using this profile */
	DIR *dir = opendir(registry_dir);
	if (!dir) {
		if (errno == ENOENT) {
			/* No registry directory means no locks */
			free(registry_dir);
			return false;
		}
		wlr_log_errno(WLR_ERROR, "Failed to open registry directory: %s", registry_dir);
		free(registry_dir);
		return false;
	}

	free(registry_dir);

	struct dirent *entry;
	bool locked = false;

	while ((entry = readdir(dir)) != NULL) {
		/* Skip . and .. */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		/* Check if it's a .toml file */
		size_t len = strlen(entry->d_name);
		if (len < 6 || strcmp(entry->d_name + len - 5, ".toml") != 0) {
			continue;
		}

		/* Build full path */
		char *registry_dir = get_registry_dir_path();
		if (!registry_dir) {
			continue;
		}

		char *file_path = malloc(strlen(registry_dir) + strlen(entry->d_name) + 2);
		if (!file_path) {
			free(registry_dir);
			continue;
		}

		sprintf(file_path, "%s/%s", registry_dir, entry->d_name);
		free(registry_dir);

		/* Parse the TOML file using tomlc17 */
		toml_result_t result = toml_parse_file_ex(file_path);
		free(file_path);

		if (!result.ok) {
			continue;
		}

		/* Check if this instance has the profile we're looking for */
		toml_datum_t profile = toml_get(result.toptab, "profile");
		if (profile.type == TOML_STRING && profile.u.s) {
			if (strcmp(profile.u.s, profile_name) == 0) {
				/* Found an instance using this profile */
				toml_free(result);
				locked = true;
				break;
			}
		}

		toml_free(result);
	}

	closedir(dir);
	return locked;
}
