/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include "keybinding.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

struct modifier_name {
	const char *name;
	uint32_t flag;
};

static const struct modifier_name modifiers[] = {
	{"super", WLR_MODIFIER_LOGO},
	{"ctrl", WLR_MODIFIER_CTRL},
	{"alt", WLR_MODIFIER_ALT},
	{"shift", WLR_MODIFIER_SHIFT},
	{"mod4", WLR_MODIFIER_LOGO}, /* X11 synonym for Super */
	{NULL, 0},
};

static struct keybinding *
keybinding_create(void)
{
	struct keybinding *binding = calloc(1, sizeof(struct keybinding));
	if (!binding) {
		return NULL;
	}
	binding->modifiers = 0;
	binding->keysym = XKB_KEY_NoSymbol;
	return binding;
}

bool
keybinding_parse(const char *str, struct keybinding *out)
{
	if (!str || !out) {
		return false;
	}

	char *copy = strdup(str);
	if (!copy) {
		return false;
	}

	struct keybinding *binding = keybinding_create();
	if (!binding) {
		free(copy);
		return false;
	}

	char *token;
	char *rest = copy;
	bool has_key = false;

	while ((token = strtok_r(rest, "+", &rest))) {
		/* Check if it's a modifier (case-insensitive) */
		bool is_modifier = false;
		for (int i = 0; modifiers[i].name != NULL; i++) {
			if (strcasecmp(token, modifiers[i].name) == 0) {
				binding->modifiers |= modifiers[i].flag;
				is_modifier = true;
				break;
			}
		}

		if (!is_modifier) {
			/* Last non-modifier token is the key */
			xkb_keysym_t keysym = xkb_keysym_from_name(token, XKB_KEYSYM_CASE_INSENSITIVE);
			if (keysym == XKB_KEY_NoSymbol) {
				/* Invalid key name */
				free(binding);
				free(copy);
				return false;
			}
			binding->keysym = keysym;
			has_key = true;
		}
	}

	free(copy);

	if (!has_key) {
		/* Must have a key, not just modifiers */
		free(binding);
		return false;
	}

	*out = *binding;
	free(binding);
	return true;
}

bool
keybinding_match(const struct keybinding *binding, uint32_t modifiers, xkb_keysym_t keysym)
{
	if (!binding) {
		return false;
	}
	return binding->modifiers == modifiers && binding->keysym == keysym;
}
