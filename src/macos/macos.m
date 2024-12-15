#include "include/punknobs.h"
#include "include/punknobs_macos.h"

#include "common/punknobs_private.h"
#include "macos/macos.h"
#include "macos/macos_helpers.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#import <Foundation/Foundation.h>
#import <IOHIDDevice.h>
#import <IOHIDManager.h>

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
	backend->devices = NULL;
	backend->thread = [PunknobsThread new];
	[backend->thread setPunknobs: context];

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	// release thread
	[backend->thread release];

	// release the list of plugged devices
	struct macos_device_node* device = backend->devices;
	struct macos_device_node* device_tmp = NULL;

	while (device != NULL)
	{
		device_tmp = device->next;
		free(device);
		device = device_tmp;
	}

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

	[backend->thread start];

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;

	[backend->thread stop];

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_node* device = backend->devices;

	while (device != NULL)
	{
		if ((device->info.punknobs_id == id)
		&& (device->info.registered == false)
		&& (device->info.plugged == true))
		{
			IOHIDDeviceRegisterInputValueCallback(
				(IOHIDDeviceRef) id,
				macos_helper_input,
				context);

			device->info.registered = true;

			break;
		}

		device = device->next;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_node* device = backend->devices;

	while (device != NULL)
	{
		if ((device->info.punknobs_id == id)
		&& (device->info.registered == true))
		{
			IOHIDDeviceRegisterInputValueCallback(
				(IOHIDDeviceRef) id,
				NULL,
				NULL);

			device->info.registered = false;

			break;
		}

		device = device->next;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_device_node* device = backend->devices;

	while (device != NULL)
	{
		if (device->info.plugged == true)
		{
			struct macos_device_info info =
			{
				.punknobs_id = (intptr_t) device,
				.manufacturer_name = device->info.manufacturer_name,
				.product_name = device->info.product_name,
				.vendor_id = device->info.vendor_id,
				.product_id = device->info.product_id,
				.plugged = device->info.plugged,
				.registered = device->info.registered,
			};

			context->device_callback(
				context->device_custom_data,
				&info,
				error);

			if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
			{
				return;
			}
		}

		device = device->next;
	}

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

	size_t manufacturer_len = strlen(info->manufacturer_name);
	size_t product_len = strlen(info->product_name);

	// allocate name buffer
	char* name = malloc(manufacturer_len + product_len + 2);

	if (name == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return NULL;
	}

	// fill name buffer
	char* ptr = name;
	// copy manufacturer name
	strncpy(ptr, info->manufacturer_name, manufacturer_len);
	ptr += manufacturer_len;
	// append space
	*ptr = ' ';
	ptr += 1;
	// append product name
	strncpy(ptr, info->product_name, product_len);
	ptr += product_len;
	// append NUL
	*ptr = '\0';

	// all good
	punknobs_error_ok(error);
	return name;
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

	uint64_t timestamp = IOHIDValueGetTimeStamp(info->input_value);

	*sec = timestamp / 1000000000;
	*usec = (timestamp % 1000000000) / 1000;

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
	return IOHIDElementGetType(IOHIDValueGetElement(info->input_value));
}

unsigned punknobs_macos_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return IOHIDElementGetUsage(IOHIDValueGetElement(info->input_value));
}

unsigned punknobs_macos_input_get_page(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return IOHIDElementGetUsagePage(IOHIDValueGetElement(info->input_value));
}

unsigned punknobs_macos_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	struct macos_input_info* info = input_info;

	punknobs_error_ok(error);
	return IOHIDValueGetIntegerValue(info->input_value);
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
