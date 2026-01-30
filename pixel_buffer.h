#ifndef PIXEL_BUFFER_H
#define PIXEL_BUFFER_H

#include <wlr/interfaces/wlr_buffer.h>
#include <stdbool.h>
#include <stddef.h>

/* Custom buffer that wraps pixel data for wlroots scene graph */
struct pixel_buffer {
	struct wlr_buffer base;
	uint32_t *data;
	int width;
	int height;
	size_t size;
};

/**
 * Destroy a pixel buffer and release its resources.
 */
void pixel_buffer_destroy(struct pixel_buffer *buffer);

/**
 * Begin accessing pixel buffer data.
 * Implements wlr_buffer begin_data_ptr_access interface.
 */
bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
				    uint32_t flags, void **data_out,
				    uint32_t *format, size_t *stride);

/**
 * End accessing pixel buffer data.
 * Implements wlr_buffer end_data_ptr_access interface.
 */
void pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer);

/* wlr_buffer implementation for pixel_buffer */
extern const struct wlr_buffer_impl pixel_buffer_impl;

#endif
