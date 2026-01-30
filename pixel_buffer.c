#include "pixel_buffer.h"
#include <drm/drm_fourcc.h>
#include <stdlib.h>

void
pixel_buffer_destroy(struct pixel_buffer *buffer)
{
	if (!buffer) return;
	if (buffer->data) {
		free(buffer->data);
	}
	wlr_buffer_finish(&buffer->base);
	free(buffer);
}

bool
pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
				    uint32_t flags, void **data_out,
				    uint32_t *format, size_t *stride)
{
	struct pixel_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*data_out = buffer->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = buffer->width * 4;
	return true;
}

void
pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* Nothing to do */
}

const struct wlr_buffer_impl pixel_buffer_impl = {
	.destroy = (void*)pixel_buffer_destroy,
	.begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};
