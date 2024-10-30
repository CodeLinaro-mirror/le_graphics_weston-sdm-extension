/*
*    This file provide a module to deal with deep sleep.
*    Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
*    SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "config.h"

#include <stdlib.h>
#include <systemd/sd-bus.h>>
#include <wayland-server.h>
#include <poll.h>
#include <errno.h>
#include <libweston/zalloc.h>
#include <libweston/libweston.h>
#include "weston-shared/helpers.h"

#ifndef EOK
#define EOK 0
#endif

#ifndef E_NOK
#define E_NOK -1
#endif

#define DS_SD_BUS_TYPE_BOOLEAN "b"


struct dbus_context {
	/* Systemd bus pointer */
	sd_bus          *sBus;

	/* dbus fd events revents for I/O operations */
	struct pollfd   sDbusFd;
};

struct  dbus_notifier {
	struct wl_event_source *dbus_source;
	struct wl_listener compositor_destroy_listener;
	struct dbus_context dContext;
	struct weston_compositor *compositor;
};

static int weston_compositor_dbus_process(sd_bus_message *sMessage, void *userdata, sd_bus_error *sError);

static void
weston_compositor_put_dbus(struct dbus_context *context)
{
	if (!context) {
		weston_log(" Null context \n");
		return;
	}

	context->sDbusFd.fd = -1;
	if (context->sBus) {
		sd_bus_unref(context->sBus);
		context->sBus = NULL;
	}
}

static int32_t
weston_compositor_get_dbus(struct dbus_context *context)
{
	int32_t ret = EOK;

	if (!context) {
		weston_log(" Null context \n");
		return E_NOK;
	}

	/* sBUs should be NULL before get bus */
	if (context->sBus) {
		weston_log("sBus should be NULL before get bus\n");
		return E_NOK;
	}

	ret = sd_bus_default_system(&context->sBus);
	if (ret < 0) {
		weston_log("sd_bus_default system fail %s\n", strerror(0 - ret));
		return ret;
	}

	context->sDbusFd.fd = sd_bus_get_fd(context->sBus);
	if (context->sDbusFd.fd < 0) {
		weston_log("sd_bus_get_fd fails %s\n", strerror(0 - context->sDbusFd.fd));
		weston_compositor_put_dbus(context);
		return E_NOK;
	}

	context->sDbusFd.events = (short int) POLLIN,
	context->sDbusFd.revents = 0;
	ret = EOK;

	return ret;
}

static int32_t
weston_compositor_dbus_register_cb(struct dbus_notifier *notifier)
{
	int32_t ret = EOK;

	ret = sd_bus_match_signal(notifier->dContext.sBus, NULL,
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager",
		"PrepareForSleep", weston_compositor_dbus_process, (void *)notifier);
	if (ret < 0)
		weston_log("failed to match PrepareForSleep signal %s\n", strerror(0-ret));
	else
		ret = EOK;

	return ret;
}

static void
weston_compositor_destroy_dbus_listener(struct wl_listener *listener, void *data)
{
	struct dbus_notifier *notifier;

	notifier = container_of(listener, struct dbus_notifier, compositor_destroy_listener);
	if (notifier->dbus_source)
		wl_event_source_remove(notifier->dbus_source);
	weston_compositor_put_dbus(&notifier->dContext);
	wl_list_remove(&notifier->compositor_destroy_listener.link);
	free(notifier);
}

static int
weston_compositor_dbus_process(sd_bus_message *sMessage, void *userdata, sd_bus_error *sError)
{
	int suspend;
	int ret = EOK;
	struct dbus_notifier *notifier = NULL;

	if (!sMessage || !userdata) {
		weston_log("NULL param received \n");
		return E_NOK;
	}

	weston_log("Match triggered! destination=%s interface=%s member=%s\n",
			sd_bus_message_get_destination(sMessage),
			sd_bus_message_get_interface(sMessage),
			sd_bus_message_get_member(sMessage));

	ret = sd_bus_message_read(sMessage, DS_SD_BUS_TYPE_BOOLEAN, &suspend);
	if (ret < 0) {
		weston_log("failed to read message: %s\n", strerror(0 - ret));
		return ret;
	}

	notifier = (struct dbus_notifier *)userdata;
	/* suspend > 0 means that the system is about to suspend */
	if (suspend) {
		weston_compositor_sleep(notifier->compositor);
		weston_log("Weston goto sleep \n");
	} else {
		weston_compositor_wake(notifier->compositor);
		weston_log("Weston wake up \n");
	}

	return ret;
}

static void
dbus_ds_handler(int fd, uint32_t mask, void* data)
{
	struct dbus_notifier *notifier = data;
	int ret = EOK;

	if (mask & POLLIN) {
		do {
			/* sd_bus_process only processes one incoming message per call.
			   There may be more than one dbus incoming messages on the bus though.
			   Call sd_bus_process until it reports r < 0 (error) or r == 0 (no progress was made) */
			ret  = sd_bus_process(notifier->dContext.sBus, NULL);
		} while (ret > 0); /* r > 0 means sd_bus_process processed an incoming message */

		if (ret < 0)
			weston_log("failed to process bus: %s\n", strerror(0 - ret));
	}
}

WL_EXPORT int
wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
	struct wl_event_loop *loop;
	struct dbus_notifier *dbus_notifier;
	int32_t ret = EOK;

	dbus_notifier = zalloc(sizeof *dbus_notifier);
	if (!dbus_notifier)
		return E_NOK;

	if (!weston_compositor_add_destroy_listener_once(compositor,
			&dbus_notifier->compositor_destroy_listener,
			weston_compositor_destroy_dbus_listener)) {
		weston_log("weston_compositor_add_destroy_listener_once fail \n");
		goto err_out;
	}

	ret = weston_compositor_get_dbus(&dbus_notifier->dContext);
	if (ret) {
		weston_log("weston_compositor_get_dbus fail \n");
		goto err_get_bus;
	}

	ret = weston_compositor_dbus_register_cb(dbus_notifier);
	if (ret) {
		weston_log("weston_compositor_dbus_register_cb fail \n");
		goto err_register;
	}

	loop = wl_display_get_event_loop(compositor->wl_display);
	if (!loop) {
		weston_log("No loop get \n");
		goto err_get_loop;
	}

	dbus_notifier->compositor = compositor;
	dbus_notifier->dbus_source =
			wl_event_loop_add_fd(loop, dbus_notifier->dContext.sDbusFd.fd,
										WL_EVENT_READABLE, dbus_ds_handler, dbus_notifier);

	return ret;

err_get_loop:
err_register:
	weston_compositor_put_dbus(&dbus_notifier->dContext);
err_get_bus:
	wl_list_remove(&dbus_notifier->compositor_destroy_listener.link);
err_out:
	free(dbus_notifier);
	return E_NOK;
}
