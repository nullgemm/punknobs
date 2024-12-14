#include "include/punknobs.h"
#include "common/punknobs_private.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct punknobs* punknobs_init(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	// We allocate the context here and on the heap to avoid having to implement
	// a complex synchronization system just to reset a user-supplied structure
	// (this way this function is naturally thread-safe and reentrant).
	struct punknobs* context = malloc(sizeof (struct punknobs));

	// If the context allocation failed, we can't initialize the error system
	// and must therefore return NULL to communicate something went wrong.
	if (context == NULL)
	{
		return NULL;
	}

	// zero-initialize the context
	struct globuf zero = {0};
	*context = zero;

	// initialize everything with default values
	context->device_custom_data = NULL;
	context->device_callback = NULL;
	context->inputs_custom_data = NULL;
	context->inputs_callback = NULL;
	context->backend_config = *config
	context->backend_context = NULL;

	punknobs_error_init(context);

	// call the backend's init function
	context->backend_config.init(context, error);

	// error always set
	return context;
}

void punknobs_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	context->backend_config.clean(context, error);
	free(context);

	// error always set
}

void punknobs_start(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	context->backend_config.start(context, error);

	// error always set
}

void punknobs_window_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	context->backend_config.stop(context, error);

	// error always set
}

void punknobs_register(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	context->backend_config.register(context, id, error);

	// error always set
}

void punknobs_unregister(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	context->backend_config.unregister(context, id, error);

	// error always set
}

void punknobs_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	context->backend_config.reenumerate(context, error);

	// error always set
}

intptr_t punknobs_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_punknobs_id(context, device_info, error);
}

char* punknobs_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_name(context, device_info, error);
}

unsigned punknobs_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_vendor_id(context, device_info, error);
}

unsigned punknobs_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_product_id(context, device_info, error);
}

bool punknobs_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_registered(context, device_info, error);
}

bool punknobs_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_plugged(context, device_info, error);
}

void* punknobs_device_get_backend_data(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.device_get_backend_data(context, device_info, error);
}

intptr_t punknobs_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.input_get_punknobs_id(context, input_info, error);
}

void punknobs_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error)
{
	context->backend_config.input_get_time(context, input_info, sec, usec, error);

	// error always set
}

unsigned punknobs_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.input_get_type(context, input_info, error);
}

unsigned punknobs_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.input_get_code(context, input_info, error);
}

unsigned punknobs_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.input_get_value(context, input_info, error);
}

void* punknobs_input_get_backend_data(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	// error always set
	return context->backend_config.input_get_backend_data(context, input_info, error);
}
