#include "config.h"
#include "desktop_entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <wlr/util/log.h>

/* XDG data directories to search */
static const char *xdg_data_dirs[] = {
	"/usr/share/applications",
	"/usr/local/share/applications",
	NULL  /* Terminated by NULL */
};

/* Get XDG DATA HOME directory (usually ~/.local/share/applications) */
static char *
get_xdg_data_home(void)
{
	const char *env = getenv("XDG_DATA_HOME");
	if (env && env[0] != '\0') {
		/* Check if it's an absolute path */
		if (env[0] == '/') {
			char *path = strdup(env);
			if (path) {
				/* Append /applications */
				size_t len = strlen(path);
				char *new_path = realloc(path, len + strlen("/applications") + 1);
				if (new_path) {
					strcat(new_path, "/applications");
					return new_path;
				}
				free(path);
			}
		}
	}

	/* Fallback to ~/.local/share/applications */
	const char *home = getenv("HOME");
	if (home && home[0] != '\0') {
		char *path = malloc(strlen(home) + strlen("/.local/share/applications") + 1);
		if (path) {
			sprintf(path, "%s/.local/share/applications", home);
			return path;
		}
	}

	return NULL;
}

/* Trim whitespace from string (in-place) */
static void
trim_whitespace(char *str)
{
	if (!str || str[0] == '\0') {
		return;
	}

	/* Trim leading whitespace */
	char *start = str;
	while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
		start++;
	}

	if (start != str) {
		memmove(str, start, strlen(start) + 1);
	}

	/* Trim trailing whitespace */
	char *end = str + strlen(str) - 1;
	while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
		end--;
	}
	*(end + 1) = '\0';
}

/* Parse a single .desktop file */
static struct cg_desktop_entry *
parse_desktop_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		return NULL;
	}

	struct cg_desktop_entry *entry = calloc(1, sizeof(struct cg_desktop_entry));
	if (!entry) {
		fclose(f);
		return NULL;
	}

	entry->desktop_file = strdup(path);
	entry->nodisplay = false;

	char line[1024];
	bool in_desktop_entry = false;

	while (fgets(line, sizeof(line), f)) {
		/* Remove newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
		}

		/* Skip empty lines and comments */
		if (line[0] == '\0' || line[0] == '#') {
			continue;
		}

		/* Check for [Desktop Entry] section */
		if (strcmp(line, "[Desktop Entry]") == 0) {
			in_desktop_entry = true;
			continue;
		}

		/* If we hit another section, stop parsing */
		if (line[0] == '[' && in_desktop_entry) {
			break;
		}

		/* Only parse keys in [Desktop Entry] section */
		if (!in_desktop_entry) {
			continue;
		}

		/* Parse key=value pairs */
		char *equals = strchr(line, '=');
		if (!equals) {
			continue;
		}

		*equals = '\0';
		char *key = line;
		char *value = equals + 1;

		trim_whitespace(key);
		trim_whitespace(value);

		/* Extract relevant fields */
		if (strcmp(key, "Name") == 0 && !entry->name) {
			entry->name = strdup(value);
		} else if (strcmp(key, "Exec") == 0 && !entry->exec) {
			entry->exec = strdup(value);
		} else if (strcmp(key, "Icon") == 0 && !entry->icon) {
			entry->icon = strdup(value);
		} else if (strcmp(key, "Categories") == 0 && !entry->categories) {
			entry->categories = strdup(value);
		} else if (strcmp(key, "NoDisplay") == 0) {
			if (strcmp(value, "true") == 0) {
				entry->nodisplay = true;
			}
		}
	}

	fclose(f);

	/* Validate: must have at least Name and Exec */
	if (!entry->name || !entry->exec) {
		desktop_entry_destroy(entry);
		return NULL;
	}

	wlr_log(WLR_DEBUG, "Parsed desktop entry: %s from %s", entry->name, path);
	return entry;
}

/* Scan a directory for .desktop files */
static void
scan_applications_directory(struct cg_desktop_entry_manager *manager, const char *dir_path)
{
	DIR *dir = opendir(dir_path);
	if (!dir) {
		/* Not an error - directory might not exist */
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		/* Skip hidden files and non-.desktop files */
		if (entry->d_name[0] == '.') {
			continue;
		}

		size_t name_len = strlen(entry->d_name);
		if (name_len < 8 || strcmp(entry->d_name + name_len - 8, ".desktop") != 0) {
			continue;
		}

		/* Construct full path */
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

		/* Parse the desktop file */
		struct cg_desktop_entry *desktop_entry = parse_desktop_file(full_path);
		if (desktop_entry) {
			/* Add to entries list */
			wl_list_insert(&manager->entries, &desktop_entry->link);
		}
	}

	closedir(dir);
}

struct cg_desktop_entry_manager *
desktop_entry_manager_create(void)
{
	struct cg_desktop_entry_manager *manager = calloc(1, sizeof(struct cg_desktop_entry_manager));
	if (!manager) {
		wlr_log(WLR_ERROR, "Failed to allocate desktop entry manager");
		return NULL;
	}

	wl_list_init(&manager->entries);
	wlr_log(WLR_DEBUG, "Desktop entry manager created");
	return manager;
}

void
desktop_entry_manager_destroy(struct cg_desktop_entry_manager *manager)
{
	if (!manager) {
		return;
	}

	/* Free all entries */
	struct cg_desktop_entry *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &manager->entries, link) {
		desktop_entry_destroy(entry);
	}

	free(manager);
	wlr_log(WLR_DEBUG, "Desktop entry manager destroyed");
}

int
desktop_entry_manager_load(struct cg_desktop_entry_manager *manager)
{
	if (!manager) {
		return -1;
	}

	/* First, scan XDG DATA HOME */
	char *data_home = get_xdg_data_home();
	if (data_home) {
		scan_applications_directory(manager, data_home);
		free(data_home);
	}

	/* Then scan system directories */
	for (int i = 0; xdg_data_dirs[i] != NULL; i++) {
		scan_applications_directory(manager, xdg_data_dirs[i]);
	}

	/* Count entries */
	int count = 0;
	struct cg_desktop_entry *entry;
	wl_list_for_each(entry, &manager->entries, link) {
		count++;
	}

	wlr_log(WLR_INFO, "Loaded %d desktop entries", count);
	return count;
}

/* Search for desktop entries matching query.
 * Populates results array with pointers to matching entries.
 * Returns the number of results found (up to max_results).
 */
size_t
desktop_entry_manager_search(struct cg_desktop_entry_manager *manager,
	const char *query, struct cg_desktop_entry **results, size_t max_results)
{
	if (!manager || !results || max_results == 0) {
		return 0;
	}

	size_t count = 0;

	/* If query is empty, return all entries */
	if (!query || query[0] == '\0') {
		struct cg_desktop_entry *entry;
		wl_list_for_each(entry, &manager->entries, link) {
			/* Filter out NoDisplay entries */
			if (!entry->nodisplay && count < max_results) {
				results[count++] = entry;
			}
		}
		return count;
	}

	/* Case-insensitive substring search */
	char query_lower[256];
	snprintf(query_lower, sizeof(query_lower), "%s", query);
	for (size_t i = 0; query_lower[i]; i++) {
		query_lower[i] = tolower(query_lower[i]);
	}

	struct cg_desktop_entry *entry;
	wl_list_for_each(entry, &manager->entries, link) {
		if (count >= max_results) {
			break;
		}

		/* Skip NoDisplay entries */
		if (entry->nodisplay) {
			continue;
		}

		/* Search in name */
		char name_lower[256];
		snprintf(name_lower, sizeof(name_lower), "%s", entry->name);
		for (size_t i = 0; name_lower[i]; i++) {
			name_lower[i] = tolower(name_lower[i]);
		}

		if (strstr(name_lower, query_lower) != NULL) {
			results[count++] = entry;
		}
	}

	return count;
}

void
desktop_entry_destroy(struct cg_desktop_entry *entry)
{
	if (!entry) {
		return;
	}

	free(entry->name);
	free(entry->exec);
	free(entry->icon);
	free(entry->categories);
	free(entry->desktop_file);
	free(entry);
}
