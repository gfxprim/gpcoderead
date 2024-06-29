//SPDX-License-Identifier: GPL-2.0-or-later

/*

    Copyright (C) 2023-2024 Cyril Hrubis <metan@ucw.cz>

 */

#include <zbar.h>
#include <gfxprim.h>

static zbar_image_scanner_t *scanner;
static gp_grabber *grabber;
static gp_widget *cam_view;
static gp_widget *result;

static zbar_image_t *zimg;

static enum gp_poll_event_ret grabber_event(gp_fd GP_UNUSED(*self))
{
	gp_pixmap *dst = cam_view->pixmap->pixmap;

	gp_grabber_poll(grabber);

	gp_pixmap center;

	size_t w = grabber->frame->w;
	size_t h = grabber->frame->h;

	gp_sub_pixmap(grabber->frame, &center, w/2 - 100, h/2 - 100, 200, 200);

	gp_pixmap *gray = gp_pixmap_convert_alloc(&center, GP_PIXEL_G8);

	zbar_image_set_size(zimg, gray->w, gray->h);
	zbar_image_set_data(zimg, gray->pixels, gray->w * gray->h, NULL);

	gp_rect_xywh(grabber->frame, w/2 - 100, h/2 - 100, 200, 200, 0xff0000);

	gp_coord xoff = ((gp_coord)gp_pixmap_w(dst) - (gp_coord)gp_pixmap_w(grabber->frame))/2;
	gp_coord yoff = ((gp_coord)gp_pixmap_h(dst) - (gp_coord)gp_pixmap_h(grabber->frame))/2;

	gp_blit_clipped(grabber->frame, 0, 0, w, h, dst, xoff, yoff);

	xoff = GP_MAX(0, xoff);
	yoff = GP_MAX(0, yoff);

	gp_widget_pixmap_redraw(cam_view, xoff, yoff, w, h);

	int n = zbar_scan_image(scanner, zimg);
	if (n != 0)
		printf("Found %i codes!\n", n);

	const zbar_symbol_t *symbol = zbar_image_first_symbol(zimg);
	for(; symbol; symbol = zbar_symbol_next(symbol)) {
		zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
		const char *data = zbar_symbol_get_data(symbol);
		printf("decoded %s symbol \"%s\"\n",
			zbar_get_symbol_name(typ), data);
		gp_widget_label_set(result, data);
	}

	gp_pixmap_free(gray);

	return GP_POLL_RET_OK;
}

static gp_fd grabber_fd = {
	.event = grabber_event,
	.events = GP_POLLIN,
};

static int cam_view_on_event(gp_widget_event *ev)
{
	gp_widget *self = ev->self;

	switch (ev->type) {
	case GP_WIDGET_EVENT_RESIZE:
		gp_pixmap_free(self->pixmap->pixmap);
		self->pixmap->pixmap = gp_pixmap_alloc(self->w, self->h, ev->ctx->pixel_type);
	/* fallthrough */
	case GP_WIDGET_EVENT_COLOR_SCHEME:
		gp_fill(self->pixmap->pixmap, ev->ctx->bg_color);
		gp_widget_pixmap_redraw_all(cam_view);
	break;
	}

	return 0;
}

int copy_to_clipboard(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (result)
		gp_widgets_clipboard_set(gp_widget_label_get(result), 0);

	return 0;
}

int main(int argc, char *argv[])
{
	gp_htable *uids;
	gp_widget *layout = gp_app_layout_load("gpcoderead", &uids);

	cam_view = gp_widget_by_uid(uids, "cam_view", GP_WIDGET_PIXMAP);
	gp_widget_on_event_set(cam_view, cam_view_on_event, NULL);
	gp_widget_events_unmask(cam_view, GP_WIDGET_EVENT_RESIZE | GP_WIDGET_EVENT_COLOR_SCHEME);
	result = gp_widget_by_uid(uids, "result", GP_WIDGET_LABEL);

	gp_htable_free(uids);

	scanner = zbar_image_scanner_create();
	if (!scanner) {
		//error!
		return 1;
	}

	zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

	zimg = zbar_image_create();
	zbar_image_set_format(zimg, zbar_fourcc('Y', '8', '0', '0'));

	grabber = gp_grabber_v4l2_init("/dev/video0", 640, 480);
	if (grabber) {
		grabber_fd.fd = grabber->fd;
		gp_widget_poll_add(&grabber_fd);
		gp_grabber_start(grabber);
	}

	gp_widgets_main_loop(layout, NULL, argc, argv);

	return 0;
}
