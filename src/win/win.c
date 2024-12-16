#include "include/punknobs.h"
#include "include/punknobs_win.h"

#include "common/punknobs_private.h"
#include "win/win.h"
#include "win/win_helpers.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <guiddef.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysinfoapi.h>
#include <windows.h>
#include <xinput.h>

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
	backend->delays.delay_device_refresh = 3000;
	backend->delays.delay_input_refresh = 8;
	backend->closed = false;
	backend->dinput = NULL;
	backend->new_enum_devices_dinput = NULL;
	backend->ref_enum_devices_dinput = NULL;
	backend->reg_devices_dinput = NULL;
	backend->new_enum_devices_xinput = NULL;
	backend->ref_enum_devices_xinput = NULL;
	backend->reg_devices_xinput = NULL;

	// main mutex
	backend->mutex_main = CreateMutexW(NULL, FALSE, NULL);

	if (backend->mutex_main == NULL)
	{
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_CREATE);
		return;
	}

	// enum mutex
	backend->mutex_enum = CreateMutexW(NULL, FALSE, NULL);

	if (backend->mutex_enum == NULL)
	{
		CloseHandle(backend->mutex_main);
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_CREATE);
		return;
	}

	// reg mutex
	backend->mutex_reg = CreateMutexW(NULL, FALSE, NULL);

	if (backend->mutex_reg == NULL)
	{
		CloseHandle(backend->mutex_enum);
		CloseHandle(backend->mutex_main);
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_CREATE);
		return;
	}

	// reference module
	backend->win_module = GetModuleHandleW(NULL);

	if (backend->win_module == NULL)
	{
		CloseHandle(backend->mutex_reg);
		CloseHandle(backend->mutex_enum);
		CloseHandle(backend->mutex_main);
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MODULE_GET);
		return;
	}

	// create directinput context
	HRESULT error_dinput =
		DirectInput8Create(
			backend->win_module,
			DIRECTINPUT_VERSION,
			&IID_IDirectInput8W,
			(void**) &(backend->dinput),
			NULL);

	if (error_dinput != DI_OK)
	{
		CloseHandle(backend->mutex_reg);
		CloseHandle(backend->mutex_enum);
		CloseHandle(backend->mutex_main);
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_DINPUT_GET);
		return;
	}

	// initialize device thread
	struct win_thread_device_loop_data thread_device_loop_data =
	{
		.context = NULL,
		.error = NULL,
	};

	backend->thread_device = NULL;
	backend->thread_device_loop_data = thread_device_loop_data;

	// initialize event thread
	struct win_thread_input_loop_data thread_input_loop_data =
	{
		.context = NULL,
		.error = NULL,
	};

	backend->thread_input = NULL;
	backend->thread_input_loop_data = thread_input_loop_data;

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	BOOL ok = FALSE;
	
	ok = CloseHandle(backend->mutex_main);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_DESTROY);
		return;
	}
	
	ok = CloseHandle(backend->mutex_enum);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_DESTROY);
		return;
	}
	
	ok = CloseHandle(backend->mutex_reg);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_DESTROY);
		return;
	}

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

	// start device thread
	struct win_thread_device_loop_data thread_device_loop_data =
	{
		.context = backend,
		.error = error,
	};

	backend->thread_device_loop_data =
		thread_device_loop_data;

	backend->thread_device =
		(HANDLE) _beginthreadex(
			NULL,
			0,
			device_loop,
			&(backend->thread_device_loop_data),
			0,
			NULL);

	if (backend->thread_device == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_START);
		return;
	}

	// start event thread
	struct win_thread_input_loop_data thread_input_loop_data =
	{
		.context = backend,
		.error = error,
	};

	backend->thread_input_loop_data =
		thread_input_loop_data;

	backend->thread_input =
		(HANDLE) _beginthreadex(
			NULL,
			0,
			input_loop,
			&(backend->thread_input_loop_data),
			0,
			NULL);

	if (backend->thread_input == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_START);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	BOOL ok = FALSE;

	// stop device thread
	ok = CloseHandle(backend->thread_device);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_CLOSE);
		return;
	}

	// stop event thread
	ok = CloseHandle(backend->thread_input);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_CLOSE);
		return;
	}

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

	switch (info->api)
	{
		case PUNKNOBS_WIN_API_DIRECTINPUT:
		{
			punknobs_error_ok(error);
			return (intptr_t) info->device_enum_node.dinput;
		}
		case PUNKNOBS_WIN_API_XINPUT:
		{
			punknobs_error_ok(error);
			return (intptr_t) info->device_enum_node.xinput;
		}
		default:
		{
			break;
		}
	}

	punknobs_error_throw(
		context,
		error,
		PUNKNOBS_ERROR_BACKEND_WIN_INVALID_API);

	return (intptr_t) NULL;
}

char* punknobs_win_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

	// duplicate name
	char* name = strdup(info->name);

	if (name == NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_ALLOC);
		return NULL;
	}

	punknobs_error_ok(error);
	return name;
}

unsigned punknobs_win_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_device_info* info = device_info;

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

	switch (info->api)
	{
		case PUNKNOBS_WIN_API_DIRECTINPUT:
		{
			punknobs_error_ok(error);
			return (intptr_t) info->device_enum_node.dinput;
		}
		case PUNKNOBS_WIN_API_XINPUT:
		{
			punknobs_error_ok(error);
			return (intptr_t) info->device_enum_node.xinput;
		}
		default:
		{
			break;
		}
	}

	punknobs_error_throw(
		context,
		error,
		PUNKNOBS_ERROR_BACKEND_WIN_INVALID_API);

	return (intptr_t) NULL;
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

	*sec = info->time / 1000;
	*usec = (info->time % 1000) * 1000;

	punknobs_error_ok(error);
}

unsigned punknobs_win_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->type;
}

unsigned punknobs_win_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->code;
}

unsigned punknobs_win_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	struct win_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->value;
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

	punknobs_error_ok(error);
	return info->api;
}
