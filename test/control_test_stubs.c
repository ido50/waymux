/*
 * Stubs for control server testing
 *
 * These stubs allow the control server to be tested without the full
 * WayMux server implementation.
 */

#include "tab.h"
#include "view.h"

/* Tab stubs */
int
tab_count(struct cg_server *server)
{
	(void)server;
	return 0;
}

void
tab_activate(struct cg_tab *tab)
{
	(void)tab;
}

void
tab_destroy(struct cg_tab *tab)
{
	(void)tab;
}

/* View stubs */
char *
view_get_title(struct cg_view *view)
{
	(void)view;
	return NULL;
}

/* Launcher stub - this should be in launcher.c, but we need to stub it here for testing */
void launcher_show(struct cg_launcher *launcher) {
	(void)launcher;
}
