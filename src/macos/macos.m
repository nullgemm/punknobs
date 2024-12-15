#include "include/punknobs.h"
#include "include/punknobs_macos.h"

#include "common/punknobs_private.h"
#include "evdev/macos.h"
#include "evdev/macos_helpers.h"

// main API
void punknobs_macos_init(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	// allocate the backend
	struct macos_backend* backend = malloc(sizeof (struct macos_backend));

	if (backend == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// zero-initialize the backend
	struct macos_backend zero = {0};
	*backend = zero;

	// reference the backend in the main context
	context->backend_context = backend;

	// initialize everything with default values
	backend->punknobs = context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// free the backend
	free(backend);

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_start(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

// device getters
intptr_t punknobs_macos_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

char* punknobs_macos_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->name;
}

unsigned punknobs_macos_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->vendor_id;
}

unsigned punknobs_macos_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->product_id;
}

bool punknobs_macos_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->plugged;
}

bool punknobs_macos_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->registered;
}

// input getters
intptr_t punknobs_macos_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

void punknobs_macos_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	*sec = 
	*usec = 

	punknobs_error_ok(error);
}

unsigned punknobs_macos_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->type;
}

unsigned punknobs_macos_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->code;
}

unsigned punknobs_macos_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->value;
}

// configurator
void punknobs_prepare_init_macos(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	config->data = NULL;

	config->init = punknobs_macos_init;
	config->clean = punknobs_macos_clean;
	config->start = punknobs_macos_start;
	config->stop = punknobs_macos_stop;
	config->register_add = punknobs_macos_register_add;
	config->register_del = punknobs_macos_register_del;
	config->reenumerate = punknobs_macos_reenumerate;

	config->device_get_punknobs_id = punknobs_macos_device_get_punknobs_id;
	config->device_get_name = punknobs_macos_device_get_name;
	config->device_get_vendor_id = punknobs_macos_device_get_vendor_id;
	config->device_get_product_id = punknobs_macos_device_get_product_id;
	config->device_get_plugged = punknobs_macos_device_get_plugged;
	config->device_get_registered = punknobs_macos_device_get_registered;

	config->input_get_punknobs_id = punknobs_macos_input_get_punknobs_id;
	config->input_get_time = punknobs_macos_input_get_time;
	config->input_get_type = punknobs_macos_input_get_type;
	config->input_get_code = punknobs_macos_input_get_code;
	config->input_get_value = punknobs_macos_input_get_value;

	punknobs_error_ok(error);
}
