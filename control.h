#ifndef CG_CONTROL_H
#define CG_CONTROL_H

#include "config.h"

#include <stdbool.h>
#include <wayland-server-core.h>

#include "server.h"

struct cg_control_server {
	struct cg_server *server;
	struct wl_event_source *event_source;
	int socket_fd;
	char *socket_path;
	struct wl_list clients; // cg_control_client::link
};

/* Create control server and listen on Unix domain socket */
struct cg_control_server *control_server_create(struct cg_server *server);

/* Destroy control server and cleanup resources */
void control_server_destroy(struct cg_control_server *control);

#endif
