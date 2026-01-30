#ifndef CG_DESKTOP_ENTRY_H
#define CG_DESKTOP_ENTRY_H

#include <wayland-server-core.h>

/* Represents a single application desktop entry */
struct cg_desktop_entry {
	char *name;           /* Application name (localized) */
	char *exec;           /* Command line to launch */
	char *icon;           /* Icon name (optional) */
	char *desktop_file;   /* Path to .desktop file */
	char *categories;     /* Categories (optional) */
	bool nodisplay;       /* If true, don't show in launcher */
	struct wl_list link;  /* For linking into entries list */
};

/* Manager for all desktop entries */
struct cg_desktop_entry_manager {
	struct wl_list entries;  /* List of cg_desktop_entry */
};

/* Create/destroy manager */
struct cg_desktop_entry_manager *desktop_entry_manager_create(void);
void desktop_entry_manager_destroy(struct cg_desktop_entry_manager *manager);

/* Load all desktop entries from XDG data directories */
int desktop_entry_manager_load(struct cg_desktop_entry_manager *manager);

/* Search/filter entries by query (case-insensitive substring match) */
/* Returns the number of matching entries (up to max_results) */
size_t desktop_entry_manager_search(
	struct cg_desktop_entry_manager *manager,
	const char *query,
	struct cg_desktop_entry **results,
	size_t max_results
);

/* Free a single entry (internal use) */
void desktop_entry_destroy(struct cg_desktop_entry *entry);

#endif
