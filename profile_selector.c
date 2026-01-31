/*
 * Waymux: Profile selector dialog
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#include "profile_selector.h"
#include "server.h"

#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>

struct cg_profile_selector *
profile_selector_create(struct cg_server *server)
{
	(void)server;
	wlr_log(WLR_INFO, "profile_selector_create: TODO - implement");
	return NULL;
}

void
profile_selector_destroy(struct cg_profile_selector *selector)
{
	(void)selector;
	wlr_log(WLR_INFO, "profile_selector_destroy: TODO - implement");
}

void
profile_selector_show(struct cg_profile_selector *selector)
{
	(void)selector;
	wlr_log(WLR_INFO, "profile_selector_show: TODO - implement");
}

void
profile_selector_hide(struct cg_profile_selector *selector)
{
	(void)selector;
	wlr_log(WLR_INFO, "profile_selector_hide: TODO - implement");
}

bool
profile_selector_handle_key(struct cg_profile_selector *selector, xkb_keysym_t sym, uint32_t keycode)
{
	(void)selector;
	(void)sym;
	(void)keycode;
	wlr_log(WLR_INFO, "profile_selector_handle_key: TODO - implement");
	return false;
}
