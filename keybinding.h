/*
 * WayMux: A Wayland multiplexer.
 *
 * Copyright (C) 2025 Ido Perlmuter
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_KEYBINDING_H
#define CG_KEYBINDING_H

#include <stdint.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

/* Keybinding structure: holds a modifier mask and keysym */
struct keybinding {
	uint32_t modifiers; /* WLR_MODIFIER_* flags */
	uint32_t keysym; /* xkb_keysym_t */
};

/* Parse a keybinding string like "Super+J" or "Ctrl+Shift+Q"
 * Returns true on success, false on failure.
 * On success, fills in the keybinding struct.
 *
 * Supported modifiers (case-insensitive): Super, Ctrl, Alt, Shift
 * Key is any XKB key name (case-insensitive)
 */
bool keybinding_parse(const char *str, struct keybinding *binding);

/* Check if a keyboard event matches this keybinding
 * modifiers: current modifier state from wlr_keyboard_get_modifiers()
 * keysym: keysym from xkb_state_key_get_syms()
 */
bool keybinding_match(const struct keybinding *binding, uint32_t modifiers, uint32_t keysym);

/* Default keybindings */
#define KEYBINDING_DEFAULT_NEXT_TAB     &(struct keybinding){WLR_MODIFIER_LOGO, XKB_KEY_k}
#define KEYBINDING_DEFAULT_PREV_TAB     &(struct keybinding){WLR_MODIFIER_LOGO, XKB_KEY_j}
#define KEYBINDING_DEFAULT_CLOSE_TAB    &(struct keybinding){WLR_MODIFIER_LOGO, XKB_KEY_d}
#define KEYBINDING_DEFAULT_OPEN_LAUNCHER &(struct keybinding){WLR_MODIFIER_LOGO, XKB_KEY_n}
#define KEYBINDING_DEFAULT_TOGGLE_BG    &(struct keybinding){WLR_MODIFIER_LOGO, XKB_KEY_b}
#define KEYBINDING_DEFAULT_SHOW_BG_DIALOG &(struct keybinding){WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_b}

#endif
