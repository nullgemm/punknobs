#include "include/punknobs.h"
#include "include/punknobs_win.h"

#include "common/punknobs_private.h"
#include "win/win.h"
#include "win/win_helpers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// main API
void punknobs_win_init(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// allocate the backend
	struct win_backend* backend = malloc(sizeof (struct win_backend));

	if (backend == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// zero-initialize the backend
	struct win_backend zero = {0};
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

void punknobs_win_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// free the backend
	free(backend);

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_start(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO

	// all good
	punknobs_error_ok(error);
}

// device getters
intptr_t punknobs_win_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->punknobs_id;
}

char* punknobs_win_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->name;
}

unsigned punknobs_win_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->vendor_id;
}

unsigned punknobs_win_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->product_id;
}

bool punknobs_win_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->plugged;
}

bool punknobs_win_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->registered;
}

// input getters
intptr_t punknobs_win_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	punknobs_error_ok(error);
	return info->punknobs_id;
}

void punknobs_win_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	*sec = 0;
	*usec = 0;

	punknobs_error_ok(error);
}

unsigned punknobs_win_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	punknobs_error_ok(error);
	return 0;
}

unsigned punknobs_win_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	punknobs_error_ok(error);
	return 0;
}

unsigned punknobs_win_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	punknobs_error_ok(error);
	return 0;
}

// configurator
void punknobs_prepare_init_win(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	config->data = NULL;

	config->init = punknobs_win_init;
	config->clean = punknobs_win_clean;
	config->start = punknobs_win_start;
	config->stop = punknobs_win_stop;
	config->register_add = punknobs_win_register_add;
	config->register_del = punknobs_win_register_del;
	config->reenumerate = punknobs_win_reenumerate;

	config->device_get_punknobs_id = punknobs_win_device_get_punknobs_id;
	config->device_get_name = punknobs_win_device_get_name;
	config->device_get_vendor_id = punknobs_win_device_get_vendor_id;
	config->device_get_product_id = punknobs_win_device_get_product_id;
	config->device_get_plugged = punknobs_win_device_get_plugged;
	config->device_get_registered = punknobs_win_device_get_registered;

	config->input_get_punknobs_id = punknobs_win_input_get_punknobs_id;
	config->input_get_time = punknobs_win_input_get_time;
	config->input_get_type = punknobs_win_input_get_type;
	config->input_get_code = punknobs_win_input_get_code;
	config->input_get_value = punknobs_win_input_get_value;

	punknobs_error_ok(error);
}

void punknobs_win_set_delays(
	struct punknobs* context,
	struct punknobs_win_delays* delays,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	backend->delays = *delays;

	// all good
	punknobs_error_ok(error);
}

enum punknobs_win_api punknobs_win_device_get_api(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// TODO

	punknobs_error_ok(error);
	return info->api;
}

enum punknobs_win_api punknobs_win_input_get_api(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	// TODO

	punknobs_error_ok(error);
	return info->api;
}
