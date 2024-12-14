#define _XOPEN_SOURCE 700

#include "include/punknobs.h"
#include "include/punknobs_evdev_epoll.h"

#include "common/punknobs_private.h"
#include "evdev/evdev_epoll.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// *heavy sigh*
#if defined(_POSIX_MONOTONIC_CLOCK)
#define PUNKNOBS_NIX_EVDEV_CLOCK CLOCK_MONOTONIC
#else
#define PUNKNOBS_NIX_EVDEV_CLOCK CLOCK_REALTIME
#endif

// main API
void punknobs_evdev_epoll_init(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// allocate the backend
	struct evdev_epoll_backend* backend = malloc(sizeof (struct evdev_epoll_backend));

	if (backend == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// zero-initialize the backend
	struct evdev_epoll_backend zero = {0};
	*backend = zero;

	// reference the backend in the main context
	context->backend_context = backend;

	// initialize everything with default values
	backend->punknobs = context;
	backend->closed = false;

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// free the backend
	free(backend);

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_start(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// TODO

	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// TODO

	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	// TODO

	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	// TODO

	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// TODO

	punknobs_error_ok(error);
}

// device getters
intptr_t punknobs_evdev_epoll_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

char* punknobs_evdev_epoll_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->name;
}

unsigned punknobs_evdev_epoll_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->vendor_id;
}

unsigned punknobs_evdev_epoll_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->product_id;
}

bool punknobs_evdev_epoll_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->plugged;
}

bool punknobs_evdev_epoll_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->registered;
}

// input getters
intptr_t punknobs_evdev_epoll_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

void punknobs_evdev_epoll_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_input_info* info = input_info;

	*sec = info->input_event->input_event_sec;
	*usec = info->input_event->input_event_usec;

	punknobs_error_ok(error);
}

unsigned punknobs_evdev_epoll_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->type;
}

unsigned punknobs_evdev_epoll_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->code;
}

unsigned punknobs_evdev_epoll_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct punknobs* punknobs = backend->punknobs;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->value;
}

// configurator
void punknobs_prepare_init_evdev_epoll(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	config->data = NULL;

	config->init = punknobs_evdev_epoll_init;
	config->clean = punknobs_evdev_epoll_clean;
	config->start = punknobs_evdev_epoll_start;
	config->stop = punknobs_evdev_epoll_stop;
	config->register_add = punknobs_evdev_epoll_register_add;
	config->register_del = punknobs_evdev_epoll_register_del;
	config->reenumerate = punknobs_evdev_epoll_reenumerate;

	config->device_get_punknobs_id = punknobs_evdev_epoll_device_get_punknobs_id;
	config->device_get_name = punknobs_evdev_epoll_device_get_name;
	config->device_get_vendor_id = punknobs_evdev_epoll_device_get_vendor_id;
	config->device_get_product_id = punknobs_evdev_epoll_device_get_product_id;
	config->device_get_plugged = punknobs_evdev_epoll_device_get_plugged;
	config->device_get_registered = punknobs_evdev_epoll_device_get_registered;

	config->input_get_punknobs_id = punknobs_evdev_epoll_input_get_punknobs_id;
	config->input_get_time = punknobs_evdev_epoll_input_get_time;
	config->input_get_type = punknobs_evdev_epoll_input_get_type;
	config->input_get_code = punknobs_evdev_epoll_input_get_code;
	config->input_get_value = punknobs_evdev_epoll_input_get_value;

	punknobs_error_ok(error);
}
