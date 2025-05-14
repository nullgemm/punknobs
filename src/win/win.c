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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysinfoapi.h>
#include <windows.h>
#include <xinput.h>

static BOOL CALLBACK effects_callback(LPCDIEFFECTINFO pdei, LPVOID pvRef)
{
	struct punknobs_haptics_features* features =
		(struct punknobs_haptics_features*) pvRef;

	// there was an issue, and DirectInput reported certain features twice or more...
	if (features->count == PUNKNOBS_HAPTICS_FEATURE_COUNT)
	{
		return DIENUM_STOP;
	}

	if ((pdei->dwEffType & DIEFT_PERIODIC) != 0)
	{
		size_t i = 0;

		// search for rumble/periodic in already reported features
		while (i < features->count)
		{
			if ((features->list[i] == PUNKNOBS_HAPTICS_FEATURE_RUMBLE)
			|| (features->list[i] == PUNKNOBS_HAPTICS_FEATURE_PERIODIC))
			{
				break;
			}

			++i;
		}

		// not found, let's add it (we must ignore extra requests for each waveform type)
		if (i == features->count)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_RUMBLE;
			features->count += 1;
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_PERIODIC;
			features->count += 1;
		}
	}
	if ((pdei->dwEffType & DIEFT_CONSTANTFORCE) != 0)
	{
		if (IsEqualGUID(&(pdei->guid), &GUID_ConstantForce) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_CONSTANT;
			features->count += 1;
		}
	}
	if ((pdei->dwEffType & DIEFT_CONDITION) != 0)
	{
		if (IsEqualGUID(&(pdei->guid), &GUID_Spring) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_SPRING;
			features->count += 1;
		}
		else if (IsEqualGUID(&(pdei->guid), &GUID_Friction) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_FRICTION;
			features->count += 1;
		}
		else if (IsEqualGUID(&(pdei->guid), &GUID_Damper) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_DAMPER;
			features->count += 1;
		}
		else if (IsEqualGUID(&(pdei->guid), &GUID_Inertia) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_INERTIA;
			features->count += 1;
		}
	}
	if ((pdei->dwEffType & DIEFT_RAMPFORCE) != 0)
	{
		if (IsEqualGUID(&(pdei->guid), &GUID_RampForce) == TRUE)
		{
			features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_RAMP;
			features->count += 1;
		}
	}

	return DIENUM_CONTINUE;
}

static BOOL CALLBACK waveforms_callback(LPCDIEFFECTINFO pdei, LPVOID pvRef)
{
	struct punknobs_haptics_waveforms* waveforms =
		(struct punknobs_haptics_waveforms*) pvRef;

	if ((pdei->dwEffType & DIEFT_PERIODIC) != 0)
	{
		if (IsEqualGUID(&(pdei->guid), &GUID_Square) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_SQUARE;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}

		if (IsEqualGUID(&(pdei->guid), &GUID_Triangle) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_TRIANGLE;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}

		if (IsEqualGUID(&(pdei->guid), &GUID_Sine) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_SINE;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}

		if (IsEqualGUID(&(pdei->guid), &GUID_SawtoothUp) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_SAW_UP;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}

		if (IsEqualGUID(&(pdei->guid), &GUID_SawtoothDown) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_SAW_DOWN;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}

		if (IsEqualGUID(&(pdei->guid), &GUID_CustomForce) == TRUE)
		{
			waveforms->list[waveforms->count] = PUNKNOBS_HAPTICS_WAVEFORM_CUSTOM;
			waveforms->count += 1;
			return DIENUM_CONTINUE;
		}
	}

	return DIENUM_CONTINUE;
}

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
	backend->window = NULL;

	// create reenumeration event
	backend->reenumeration_handler =
		CreateEventA(
			NULL,
			TRUE,
			FALSE,
			NULL);

	if (backend->reenumeration_handler == NULL)
	{
		free(backend);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_EVENT_CREATE);
		return;
	}

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

	ok = CloseHandle(backend->reenumeration_handler);

	if (ok == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_EVENT_DESTROY);
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

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_enum, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// clean reference dinput enumeration list
	struct win_device_enum_node_dinput* dinput_enum_node = backend->ref_enum_devices_dinput;
	struct win_device_enum_node_dinput* dinput_enum_next = NULL;

	while (dinput_enum_node != NULL)
	{
		dinput_enum_next = dinput_enum_node->next;
		dinput_enum_node->device->lpVtbl->Unacquire(dinput_enum_node->device);
		dinput_enum_node->device->lpVtbl->Release(dinput_enum_node->device);
		free(dinput_enum_node);
		dinput_enum_node = dinput_enum_next;
	}

	// clean reference xinput enumeration list
	struct win_device_enum_node_xinput* xinput_enum_node = backend->ref_enum_devices_xinput;
	struct win_device_enum_node_xinput* xinput_enum_next = NULL;

	while (xinput_enum_node != NULL)
	{
		xinput_enum_next = xinput_enum_node->next;
		free(xinput_enum_node);
		xinput_enum_node = xinput_enum_next;
	}

	// unlock mutex
	BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// lock mutex
	DWORD reg_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (reg_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// clean dinput registry
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;
	struct win_device_reg_node_dinput* dinput_reg_next = NULL;

	while (dinput_reg_node != NULL)
	{
		dinput_reg_next = dinput_reg_node->next;
		free(dinput_reg_node);
		dinput_reg_node = dinput_reg_next;
	}

	// clean xinput registry
	struct win_device_reg_node_xinput* xinput_reg_node = backend->reg_devices_xinput;
	struct win_device_reg_node_xinput* xinput_reg_next = NULL;

	while (xinput_reg_node != NULL)
	{
		xinput_reg_next = xinput_reg_node->next;
		free(xinput_reg_node);
		xinput_reg_node = xinput_reg_next;
	}

	// unlock mutex
	BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

	if (reg_unlock == 0)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_enum, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_enum_node_dinput* dinput_node = backend->new_enum_devices_dinput;

	while (dinput_node != NULL)
	{
		if (id == ((intptr_t) dinput_node))
		{
			break;
		}

		dinput_node = dinput_node->next;
	}

	if (dinput_node != NULL)
	{
		if ((dinput_node->info.registered != false) || (dinput_node->info.plugged != true))
		{
			// ignore invalid register requests
			BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

			if (enum_unlock == 0)
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
				return;
			}

			punknobs_error_ok(error);
			return;
		}

		struct win_device_reg_node_dinput* reg_device =
			malloc(sizeof (struct win_device_reg_node_dinput));

		if (reg_device == NULL)
		{
			ReleaseMutex(backend->mutex_enum);
			punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
			return;
		}

		// initialize effects to 0
		for (size_t i = 0; i < PUNKNOBS_DIRECTINPUT_MAX_SLOT; ++i)
		{
			reg_device->effects[i] = NULL;
		}

		dinput_node->device->lpVtbl->SetDataFormat(
			dinput_node->device, &c_dfDIJoystick);

		HRESULT result = dinput_node->device->lpVtbl->SetCooperativeLevel(
			dinput_node->device,
			backend->window,
			DISCL_EXCLUSIVE | DISCL_BACKGROUND);

		if (FAILED(result))
		{
			printf("%p %ld\n", (void*) backend->window, result);
		}

		switch (result)
		{
			case DIERR_INVALIDPARAM:
			{
				printf("INVALIDPARAM\n");
				break;
			}
			case DIERR_NOTINITIALIZED:
			{
				printf("NOTINITIALIZED\n");
				break;
			}
			case E_HANDLE:
			{
				printf("E_HANDLE\n");
				break;
			}
			default:
			{
				printf("OK\n");
				break;
			}
		}

		dinput_node->device->lpVtbl->Acquire(
			dinput_node->device);

		dinput_node->device->lpVtbl->SendForceFeedbackCommand(
			dinput_node->device,
			DISFFC_RESET);

		dinput_node->device->lpVtbl->SendForceFeedbackCommand(
			dinput_node->device,
			DISFFC_SETACTUATORSON);

		// save cross-pointers and info
		dinput_node->reg_entry = reg_device;
		reg_device->enum_entry = dinput_node;
		dinput_node->info.registered = true;
		memset(&(reg_device->state), 0, sizeof (DIJOYSTATE));

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// lock mutex
		DWORD reg_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

		if (reg_lock != WAIT_OBJECT_0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
			return;
		}

		// insert node
		reg_device->next = backend->reg_devices_dinput;
		backend->reg_devices_dinput = reg_device;

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_enum_node_xinput* xinput_node = backend->new_enum_devices_xinput;

	while (xinput_node != NULL)
	{
		if (id == ((intptr_t) xinput_node))
		{
			break;
		}

		xinput_node = xinput_node->next;
	}

	if (xinput_node != NULL)
	{
		if ((xinput_node->info.registered != false) || (xinput_node->info.plugged != true))
		{
			// ignore invalid register requests
			BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

			if (enum_unlock == 0)
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
				return;
			}

			punknobs_error_ok(error);
			return;
		}

		struct win_device_reg_node_xinput* reg_device =
			malloc(sizeof (struct win_device_reg_node_xinput));

		if (reg_device == NULL)
		{
			ReleaseMutex(backend->mutex_enum);
			punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
			return;
		}

		// save cross-pointers and info
		xinput_node->reg_entry = reg_device;
		reg_device->enum_entry = xinput_node;

		// initialize effects to 0
		for (size_t i = 0; i < PUNKNOBS_DIRECTINPUT_MAX_SLOT; ++i)
		{
			reg_device->effects[i].wLeftMotorSpeed = 0;
			reg_device->effects[i].wRightMotorSpeed = 0;
		}

		xinput_node->info.registered = true;
		memset(&(reg_device->state), 0, sizeof (XINPUT_STATE));

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// lock mutex
		DWORD reg_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

		if (reg_lock != WAIT_OBJECT_0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
			return;
		}

		// insert node
		reg_device->next = backend->reg_devices_xinput;
		backend->reg_devices_xinput = reg_device;

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// ignore missing devices
	punknobs_error_ok(error);
}

void punknobs_win_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// search for DirectInput devices
	DWORD reg_lock_dinput = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (reg_lock_dinput != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	struct win_device_reg_node_dinput* dinput_node = backend->reg_devices_dinput;
	struct win_device_reg_node_dinput* dinput_prev = dinput_node;

	while (dinput_node != NULL)
	{
		if (id == ((intptr_t) dinput_node->enum_entry))
		{
			if (dinput_prev == dinput_node)
			{
				backend->reg_devices_dinput = dinput_node->next;
			}
			else
			{
				dinput_prev->next = dinput_node->next;
			}

			dinput_node->enum_entry->reg_entry = NULL;
			dinput_node->enum_entry->info.registered = false;
			free(dinput_node);
			break;
		}

		dinput_prev = dinput_node;
		dinput_node = dinput_node->next;
	}

	BOOL reg_unlock_dinput = ReleaseMutex(backend->mutex_reg);

	if (reg_unlock_dinput == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	if (dinput_node != NULL)
	{
		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	DWORD reg_lock_xinput = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (reg_lock_xinput != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	struct win_device_reg_node_xinput* xinput_node = backend->reg_devices_xinput;
	struct win_device_reg_node_xinput* xinput_prev = xinput_node;

	while (xinput_node != NULL)
	{
		if (id == ((intptr_t) xinput_node->enum_entry))
		{
			if (xinput_prev == xinput_node)
			{
				backend->reg_devices_xinput = xinput_node->next;
			}
			else
			{
				xinput_prev->next = xinput_node->next;
			}

			xinput_node->enum_entry->reg_entry = NULL;
			xinput_node->enum_entry->info.registered = false;
			free(xinput_node);
			break;
		}

		xinput_prev = xinput_node;
		xinput_node = xinput_node->next;
	}

	// unlock mutex
	BOOL reg_unlock_xinput = ReleaseMutex(backend->mutex_reg);

	if (reg_unlock_xinput == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	if (xinput_node != NULL)
	{
		// all good
		punknobs_error_ok(error);
		return;
	}

	// ignore missing devices
	punknobs_error_ok(error);
}

void punknobs_win_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// signal device loop
	SetEvent(backend->reenumeration_handler);

	// all good
	punknobs_error_ok(error);
}

// haptics management
void punknobs_win_haptics_get_features(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_features* features,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_enum, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_enum_node_dinput* dinput_node = backend->new_enum_devices_dinput;

	while (dinput_node != NULL)
	{
		if (id == ((intptr_t) dinput_node))
		{
			break;
		}

		dinput_node = dinput_node->next;
	}

	if (dinput_node != NULL)
	{
		// allocate features list
		features->list =
			malloc(
				PUNKNOBS_HAPTICS_FEATURE_COUNT
				* (sizeof (enum punknobs_haptics_feature)));

		if (features->list == NULL)
		{
			punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
			return;
		}

		features->count = 0;

		// detect main features
		HRESULT result =
			dinput_node->device->lpVtbl->EnumEffects(
				dinput_node->device,
				effects_callback,
				features,
				DIEFT_ALL);

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_ENUM);
			return;
		}

		// register features we can't make sure are available
		features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_GAIN;
		features->count += 1;
		features->list[features->count] = PUNKNOBS_HAPTICS_FEATURE_AUTOCENTER;
		features->count += 1;

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_enum_node_xinput* xinput_node = backend->new_enum_devices_xinput;

	while (xinput_node != NULL)
	{
		if (id == ((intptr_t) xinput_node))
		{
			break;
		}

		xinput_node = xinput_node->next;
	}

	if (xinput_node != NULL)
	{
		// allocate features list
		features->list = malloc(sizeof (enum punknobs_haptics_feature));

		if (features->list == NULL)
		{
			punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
			return;
		}

		features->list[0] = PUNKNOBS_HAPTICS_FEATURE_RUMBLE;
		features->count = 1;
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_haptics_get_waveforms(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_waveforms* waveforms,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_enum, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_enum_node_dinput* dinput_node = backend->new_enum_devices_dinput;

	while (dinput_node != NULL)
	{
		if (id == ((intptr_t) dinput_node))
		{
			break;
		}

		dinput_node = dinput_node->next;
	}

	if (dinput_node != NULL)
	{
		// allocate waveforms list
		waveforms->list =
			malloc(
				PUNKNOBS_HAPTICS_WAVEFORM_COUNT
				* (sizeof (enum punknobs_haptics_waveform)));

		if (waveforms->list == NULL)
		{
			punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
			return;
		}

		waveforms->count = 0;

		// detect main waveforms
		HRESULT result =
			dinput_node->device->lpVtbl->EnumEffects(
				dinput_node->device,
				waveforms_callback,
				waveforms,
				DIEFT_ALL);

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_ENUM);
			return;
		}

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_enum_node_xinput* xinput_node = backend->new_enum_devices_xinput;

	while (xinput_node != NULL)
	{
		if (id == ((intptr_t) xinput_node))
		{
			break;
		}

		xinput_node = xinput_node->next;
	}

	if (xinput_node != NULL)
	{
		waveforms->list = NULL;
		waveforms->count = 0;
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

int punknobs_win_haptics_effect_max(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	int max = 0;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_enum, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return -1;
	}

	// search for DirectInput devices
	struct win_device_enum_node_dinput* dinput_node = backend->new_enum_devices_dinput;

	while (dinput_node != NULL)
	{
		if (id == ((intptr_t) dinput_node))
		{
			break;
		}

		dinput_node = dinput_node->next;
	}

	if (dinput_node != NULL)
	{
		max = PUNKNOBS_DIRECTINPUT_MAX_SLOT;

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return -1;
		}

		// all good
		punknobs_error_ok(error);
		return max;
	}

	// search for XInput devices
	struct win_device_enum_node_xinput* xinput_node = backend->new_enum_devices_xinput;

	while (xinput_node != NULL)
	{
		if (id == ((intptr_t) xinput_node))
		{
			break;
		}

		xinput_node = xinput_node->next;
	}

	if (xinput_node != NULL)
	{
		// XInput is shit so we emulate slots
		max = PUNKNOBS_DIRECTINPUT_MAX_SLOT;
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return -1;
	}

	// all good
	punknobs_error_ok(error);
	return max;
}

int punknobs_win_haptics_effect_set(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_effect* effect,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// TODO make all platforms just overwrite whatever is in the requested slot
	// (ie. remove slot auto-assign with '-1' when using evdev)
	int slot = effect->id;

	if ((slot < 0) || (slot >= PUNKNOBS_DIRECTINPUT_MAX_SLOT))
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID);
		return -1;
	}

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return -1;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		struct win_device_enum_node_dinput* dinput_node = dinput_reg_node->enum_entry;

		GUID type;
		LPVOID params;
		DWORD params_size;

		DIENVELOPE envelope =
		{
			.dwSize = sizeof (DIENVELOPE),
		};

		DICONSTANTFORCE constant;
		DIRAMPFORCE ramp;
		DIPERIODIC periodic;
		DICONDITION condition[2];

		// TODO support axes (objectids map, see IDirectInputDevice8::EnumObjects)
		DWORD axes = 2;
		DWORD axes_ids[2] = {DIJOFS_X, DIJOFS_Y};
		LONG directions[2] = {effect->direction, effect->direction};

		switch (effect->type)
		{
			case PUNKNOBS_HAPTICS_FEATURE_CONSTANT:
			{
				constant.lMagnitude = effect->config.constant.level * 100;

				type = GUID_ConstantForce;

				params_size = sizeof (DICONSTANTFORCE);
				params = (LPVOID) &constant;

				envelope.dwAttackLevel = effect->config.constant.envelope.attack_level * 100;
				envelope.dwAttackTime = effect->config.constant.envelope.attack_length * 1000;
				envelope.dwFadeLevel = effect->config.constant.envelope.fade_level * 100;
				envelope.dwFadeTime = effect->config.constant.envelope.fade_length * 1000;
				break;
			}
			case PUNKNOBS_HAPTICS_FEATURE_RAMP:
			{
				ramp.lStart = effect->config.ramp.start_level * 100;
				ramp.lEnd = effect->config.ramp.end_level* 100;

				type = GUID_RampForce;

				params_size = sizeof (DIRAMPFORCE);
				params = (LPVOID) &ramp;

				envelope.dwAttackLevel = effect->config.ramp.envelope.attack_level * 100;
				envelope.dwAttackTime = effect->config.ramp.envelope.attack_length * 1000;
				envelope.dwFadeLevel = effect->config.ramp.envelope.fade_level * 100;
				envelope.dwFadeTime = effect->config.ramp.envelope.fade_length * 1000;
				break;
			}
			case PUNKNOBS_HAPTICS_FEATURE_PERIODIC:
			{
				periodic.dwMagnitude = effect->config.periodic.magnitude * 100;
				periodic.lOffset = effect->config.periodic.offset;
				periodic.dwPhase = effect->config.periodic.phase;
				periodic.dwPeriod = effect->config.periodic.period * 1000;

				switch (effect->config.periodic.waveform)
				{
					case PUNKNOBS_HAPTICS_WAVEFORM_SQUARE:
					{
						type = GUID_Square;
						break;
					}
					case PUNKNOBS_HAPTICS_WAVEFORM_TRIANGLE:
					{
						type = GUID_Triangle;
						break;
					}
					case PUNKNOBS_HAPTICS_WAVEFORM_SINE:
					{
						type = GUID_Sine;
						break;
					}
					case PUNKNOBS_HAPTICS_WAVEFORM_SAW_UP:
					{
						type = GUID_SawtoothUp;
						break;
					}
					case PUNKNOBS_HAPTICS_WAVEFORM_SAW_DOWN:
					{
						type = GUID_SawtoothDown;
						break;
					}
					case PUNKNOBS_HAPTICS_WAVEFORM_CUSTOM:
					{
						type = GUID_CustomForce;
						break;
					}
					default:
					{
						break;
					}
				}

				params_size = sizeof (DIPERIODIC);
				params = (LPVOID) &periodic;

				envelope.dwAttackLevel = effect->config.periodic.envelope.attack_level * 100;
				envelope.dwAttackTime = effect->config.periodic.envelope.attack_length * 1000;
				envelope.dwFadeLevel = effect->config.periodic.envelope.fade_level * 100;
				envelope.dwFadeTime = effect->config.periodic.envelope.fade_length * 1000;
				break;
			}
			case PUNKNOBS_HAPTICS_FEATURE_SPRING:
			case PUNKNOBS_HAPTICS_FEATURE_FRICTION:
			case PUNKNOBS_HAPTICS_FEATURE_DAMPER: // ???
			case PUNKNOBS_HAPTICS_FEATURE_INERTIA: // ???
			{
				condition[0].lOffset = effect->config.condition[0].center * 100;
				condition[0].lPositiveCoefficient = effect->config.condition[0].left_saturation * 100;
				condition[0].lNegativeCoefficient = effect->config.condition[0].right_saturation * 100;
				condition[0].dwPositiveSaturation = effect->config.condition[0].left_coeff * 100;
				condition[0].dwNegativeSaturation = effect->config.condition[0].right_coeff * 100;
				condition[0].lDeadBand = effect->config.condition[0].deadband * 100;

				condition[1].lOffset = effect->config.condition[1].center * 100;
				condition[1].lPositiveCoefficient = effect->config.condition[1].left_saturation * 100;
				condition[1].lNegativeCoefficient = effect->config.condition[1].right_saturation * 100;
				condition[1].dwPositiveSaturation = effect->config.condition[1].left_coeff * 100;
				condition[1].dwNegativeSaturation = effect->config.condition[1].right_coeff * 100;
				condition[1].lDeadBand = effect->config.condition[1].deadband * 100;

				switch (effect->type)
				{
					case PUNKNOBS_HAPTICS_FEATURE_SPRING:
					{
						type = GUID_Spring;
						break;
					}
					case PUNKNOBS_HAPTICS_FEATURE_FRICTION:
					{
						type = GUID_Friction;
						break;
					}
					case PUNKNOBS_HAPTICS_FEATURE_DAMPER:
					{
						type = GUID_Damper;
						break;
					}
					case PUNKNOBS_HAPTICS_FEATURE_INERTIA:
					{
						type = GUID_Inertia;
						break;
					}
					default:
					{
						break;
					}
				}

				params_size = sizeof (DICONDITION);
				params = (LPVOID) &condition;
				break;
			}
			case PUNKNOBS_HAPTICS_FEATURE_RUMBLE:
			{
				int value_weak = effect->config.rumble.weak_magnitude * 100;
				int value_strong = effect->config.rumble.strong_magnitude * 100;

				if (value_weak > value_strong)
				{
					periodic.dwMagnitude = value_weak;
				}
				else
				{
					periodic.dwMagnitude = value_strong;
				}

				periodic.lOffset = 0;
				periodic.dwPhase = 0;
				periodic.dwPeriod = 50 * 1000;

				type = GUID_Sine;

				params_size = sizeof (DIPERIODIC);
				params = (LPVOID) &periodic;

				envelope.dwAttackLevel = 100 * 100;
				envelope.dwAttackTime = 0;
				envelope.dwFadeLevel = 100 * 100;
				envelope.dwFadeTime = 0;
				break;
			}
			default:
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_TYPE);
				return -1;
			}
		}

		// TODO support conditions with buttons etc. (objectids map, see IDirectInputDevice8::EnumObjects)
		DIEFFECT config =
		{
			.dwSize = sizeof (DIEFFECT),
			.dwFlags = DIEFF_POLAR | DIEFF_OBJECTOFFSETS,
			.dwDuration = INFINITE,
			.dwSamplePeriod = 0,
			.dwGain = DI_FFNOMINALMAX,
			.dwTriggerButton = DIEB_NOTRIGGER,
			.dwTriggerRepeatInterval = 0,
			.cAxes = axes,
			.rgdwAxes = axes_ids,
			.rglDirection = directions,
			.lpEnvelope = &envelope,
			.cbTypeSpecificParams = params_size,
			.lpvTypeSpecificParams = params,
			.dwStartDelay = 0,
		};

		HRESULT result;

		result =
			dinput_node->device->lpVtbl->CreateEffect(
				dinput_node->device,
				&type,
				&config,
				&(dinput_reg_node->effects[slot]),
				NULL);

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_CREATE);

			switch (result)
			{
				case DIERR_DEVICEFULL:
				{
					printf("DEVICEFULL\n");
					break;
				}
				case DIERR_DEVICENOTREG:
				{
					printf("DEVICENOTREG\n");
					break;
				}
				case DIERR_INVALIDPARAM:
				{
					printf("INVALIDPARAM\n");
					break;
				}
				case DIERR_NOTINITIALIZED:
				{
					printf("NOTINITIALIZED\n");
					break;
				}
				default:
				{
					printf("DINPUT SHIT %ld\n", result);
					break;
				}
			}

			return -1;
		}

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return -1;
		}

		// all good
		punknobs_error_ok(error);
		return slot;
	}

	// search for XInput devices
	struct win_device_reg_node_xinput* xinput_reg_node = backend->reg_devices_xinput;

	while (xinput_reg_node != NULL)
	{
		if (id == ((intptr_t) xinput_reg_node->enum_entry))
		{
			break;
		}

		xinput_reg_node = xinput_reg_node->next;
	}

	if (xinput_reg_node != NULL)
	{
		if (effect->type == PUNKNOBS_HAPTICS_FEATURE_RUMBLE)
		{
			xinput_reg_node->effects[slot].wLeftMotorSpeed = effect->config.rumble.strong_magnitude;
			xinput_reg_node->effects[slot].wRightMotorSpeed = effect->config.rumble.weak_magnitude;
		}
		else
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_TYPE);
			return -1;
		}
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return -1;
	}

	// all good
	punknobs_error_ok(error);
	return slot;
}

void punknobs_win_haptics_effect_del(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	if ((slot < 0) || (slot >= PUNKNOBS_DIRECTINPUT_MAX_SLOT))
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID);
		return;
	}

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		if (slot >= PUNKNOBS_DIRECTINPUT_MAX_SLOT)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID);
			return;
		}

		// remove from device
		HRESULT result =
			dinput_reg_node->effects[slot]->lpVtbl->Unload(
				dinput_reg_node->effects[slot]);

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_STOP);
			return;
		}

		// release COM object
		dinput_reg_node->effects[slot]->lpVtbl->Release(
			dinput_reg_node->effects[slot]);

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_reg_node_xinput* xinput_reg_node = backend->reg_devices_xinput;

	while (xinput_reg_node != NULL)
	{
		if (id == ((intptr_t) xinput_reg_node->enum_entry))
		{
			break;
		}

		xinput_reg_node = xinput_reg_node->next;
	}

	if (xinput_reg_node != NULL)
	{
		struct win_device_enum_node_xinput* xinput_node = xinput_reg_node->enum_entry;

		xinput_reg_node->effects[slot].wLeftMotorSpeed = 0;
		xinput_reg_node->effects[slot].wRightMotorSpeed = 0;

		DWORD ok =
			XInputSetState(
				xinput_node->id,
				&(xinput_reg_node->effects[slot]));

		if (ok != ERROR_SUCCESS)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_STOP);
			return;
		}
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_haptics_gain_set(
	struct punknobs* context,
	intptr_t id,
	int gain,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	int max = 0;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		struct win_device_enum_node_dinput* dinput_node = dinput_reg_node->enum_entry;

		DIPROPDWORD property =
		{
			.diph =
			{
				.dwSize = sizeof (DIPROPDWORD),
				.dwHeaderSize = sizeof (DIPROPHEADER),
				.dwObj = 0,
				.dwHow = DIPH_DEVICE,
			},
			.dwData = gain * 100,
		};

		HRESULT result =
			dinput_node->device->lpVtbl->SetProperty(
				dinput_node->device,
				DIPROP_FFGAIN,
				&(property.diph));

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_GAIN_SET);
			return;
		}
	}

	// ignore XInput
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_haptics_autocenter_set(
	struct punknobs* context,
	intptr_t id,
	int autocenter,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;
	int max = 0;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		struct win_device_enum_node_dinput* dinput_node = dinput_reg_node->enum_entry;

		DIPROPDWORD property =
		{
			.diph =
			{
				.dwSize = sizeof (DIPROPDWORD),
				.dwHeaderSize = sizeof (DIPROPHEADER),
				.dwObj = 0,
				.dwHow = DIPH_DEVICE,
			},
			.dwData = (autocenter > 0) ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF,
		};

		HRESULT result =
			dinput_node->device->lpVtbl->SetProperty(
				dinput_node->device,
				DIPROP_AUTOCENTER,
				&(property.diph));

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_AUTOCENTER_SET);
			return;
		}
	}

	// ignore XInput
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_haptics_effect_play(
	struct punknobs* context,
	intptr_t id,
	int slot,
	int repeat,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	if ((slot < 0) || (slot >= PUNKNOBS_DIRECTINPUT_MAX_SLOT))
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID);
		return;
	}

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		HRESULT result =
			dinput_reg_node->effects[slot]->lpVtbl->Start(
				dinput_reg_node->effects[slot],
				INFINITE,
				DIES_NODOWNLOAD);

		if (result != DI_OK)
		{
			switch(result)
			{
				case DIERR_INCOMPLETEEFFECT:
				{
					printf("DIERR_INCOMPLETEEFFECT\n");
					break;
				}
				case DIERR_INVALIDPARAM:
				{
					printf("DIERR_INVALIDPARAM\n");
					break;
				}
				case DIERR_NOTEXCLUSIVEACQUIRED:
				{
					printf("DIERR_NOTEXCLUSIVEACQUIRED\n");
					break;
				}
				case DIERR_NOTINITIALIZED:
				{
					printf("DIERR_NOTINITIALIZED\n");
					break;
				}
				case DIERR_UNSUPPORTED:
				{
					printf("DIERR_UNSUPPORTED\n");
					break;
				}
				default:
				{
					break;
				}
			}
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_PLAY);
			return;
		}

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_reg_node_xinput* xinput_reg_node = backend->reg_devices_xinput;

	while (xinput_reg_node != NULL)
	{
		if (id == ((intptr_t) xinput_reg_node->enum_entry))
		{
			break;
		}

		xinput_reg_node = xinput_reg_node->next;
	}

	if (xinput_reg_node != NULL)
	{
		struct win_device_enum_node_xinput* xinput_node = xinput_reg_node->enum_entry;

		DWORD ok =
			XInputSetState(
				xinput_node->id,
				&(xinput_reg_node->effects[slot]));

		if (ok != ERROR_SUCCESS)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_PLAY);
			return;
		}
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_win_haptics_effect_stop(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct win_backend* backend = context->backend_context;

	// lock mutex
	DWORD enum_lock = WaitForSingleObject(backend->mutex_reg, INFINITE);

	if (enum_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// search for DirectInput devices
	struct win_device_reg_node_dinput* dinput_reg_node = backend->reg_devices_dinput;

	while (dinput_reg_node != NULL)
	{
		if (id == ((intptr_t) dinput_reg_node->enum_entry))
		{
			break;
		}

		dinput_reg_node = dinput_reg_node->next;
	}

	if (dinput_reg_node != NULL)
	{
		if (slot >= PUNKNOBS_DIRECTINPUT_MAX_SLOT)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID);
			return;
		}

		HRESULT result =
			dinput_reg_node->effects[slot]->lpVtbl->Stop(
				dinput_reg_node->effects[slot]);

		if (result != DI_OK)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_STOP);
			return;
		}

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			return;
		}

		// all good
		punknobs_error_ok(error);
		return;
	}

	// search for XInput devices
	struct win_device_reg_node_xinput* xinput_reg_node = backend->reg_devices_xinput;

	while (xinput_reg_node != NULL)
	{
		if (id == ((intptr_t) xinput_reg_node->enum_entry))
		{
			break;
		}

		xinput_reg_node = xinput_reg_node->next;
	}

	if (xinput_reg_node != NULL)
	{
		struct win_device_enum_node_xinput* xinput_node = xinput_reg_node->enum_entry;

		XINPUT_VIBRATION vibration =
		{
			.wLeftMotorSpeed = 0,
			.wRightMotorSpeed = 0,
		};

		DWORD ok =
			XInputSetState(
				xinput_node->id,
				&vibration);

		if (ok != ERROR_SUCCESS)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_STOP);
			return;
		}
	}

	// ignore invalid register requests
	BOOL enum_unlock = ReleaseMutex(backend->mutex_reg);

	if (enum_unlock == 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

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

	config->haptics_get_features = punknobs_win_haptics_get_features;
	config->haptics_get_waveforms = punknobs_win_haptics_get_waveforms;
	config->haptics_effect_max = punknobs_win_haptics_effect_max;
	config->haptics_effect_set = punknobs_win_haptics_effect_set;
	config->haptics_effect_del = punknobs_win_haptics_effect_del;
	config->haptics_gain_set = punknobs_win_haptics_gain_set;
	config->haptics_autocenter_set = punknobs_win_haptics_autocenter_set;
	config->haptics_effect_play = punknobs_win_haptics_effect_play;
	config->haptics_effect_stop = punknobs_win_haptics_effect_stop;

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

void punknobs_win_set_window(
	struct punknobs* context,
	void* window)
{
	struct win_backend* backend = context->backend_context;
	backend->window = (HWND) window;
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
