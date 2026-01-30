/*
 * Waymux: Instance registry for multi-instance support
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_REGISTRY_H
#define CG_REGISTRY_H

#include <stdbool.h>

/* Forward declarations */
struct cg_server;

/**
 * Register an instance in the registry
 * Creates a registry file for the instance with its metadata
 * Returns true on success, false on failure
 */
bool registry_register_instance(struct cg_server *server);

/**
 * Unregister an instance from the registry
 * Removes the instance's registry file
 * Returns true on success, false on failure
 */
bool registry_unregister_instance(struct cg_server *server);

/**
 * Check if a profile is already in use by another instance
 * Returns true if the profile is locked (in use), false otherwise
 */
bool registry_is_profile_locked(const char *profile_name);

#endif
