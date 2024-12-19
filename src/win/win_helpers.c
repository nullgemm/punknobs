#include "include/punknobs.h"

#include "common/punknobs_private.h"
#include "win/win.h"
#include "win/win_helpers.h"

#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PUNKNOBS_XUSB_HARDWARE_NAME "XUSB Controller"

static BOOL CALLBACK device_enum_callback(LPCDIDEVICEINSTANCE instance, LPVOID data)
{
	struct win_backend* backend = (struct win_backend*) data;
	struct punknobs* punknobs = backend->punknobs;
	struct punknobs_error_info error;

	// allocate new enum device list node
	struct win_device_enum_node_dinput* enum_device =
		malloc(sizeof (struct win_device_enum_node_dinput));

	if (enum_device == NULL)
	{
		punknobs_error_throw(
			punknobs,
			&error,
			PUNKNOBS_ERROR_ALLOC);
		return DIENUM_CONTINUE;
	}

	// lock mutex
	DWORD main_lock = WaitForSingleObject(backend->mutex_main, INFINITE);

	if (main_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			punknobs,
			&error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return DIENUM_CONTINUE;
	}

	// fill node values
	enum_device->guid = instance->guidInstance;
	enum_device->reg_entry = NULL;
	enum_device->next = backend->new_enum_devices_dinput;

	// add node to linked list
	backend->new_enum_devices_dinput = enum_device;

	// unlock mutex
	BOOL main_unlock = ReleaseMutex(backend->mutex_main);

	if (main_unlock == 0)
	{
		punknobs_error_throw(
			punknobs,
			&error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return DIENUM_CONTINUE;
	}

	// continue
	return DIENUM_CONTINUE;
}

static void update_closed(struct win_backend* backend, struct punknobs_error_info* error, bool* closed)
{
	struct punknobs* punknobs = backend->punknobs;

	// lock mutex
	DWORD main_lock = WaitForSingleObject(backend->mutex_main, INFINITE);

	if (main_lock != WAIT_OBJECT_0)
	{
		punknobs_error_throw(
			punknobs,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
		return;
	}

	// set closed variable
	*closed = backend->closed;

	// unlock mutex
	BOOL main_unlock = ReleaseMutex(backend->mutex_main);

	if (main_unlock == 0)
	{
		punknobs_error_throw(
			punknobs,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

static DWORD elapsed(DWORD current, DWORD last)
{
	if (current < last)
	{
		return MAXDWORD32 - last + current + 1;
	}

	return current - last;
}

// device thread
unsigned __stdcall device_loop(void* data)
{
	struct win_thread_device_loop_data* thread_data = data;
	struct win_backend* backend = thread_data->context;
	struct punknobs_error_info* error = thread_data->error;
	struct punknobs* punknobs = backend->punknobs;

	// reference module
	HINSTANCE win_module =
		GetModuleHandleW(NULL);

	if (win_module == NULL)
	{
		punknobs_error_throw(
			punknobs,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_MODULE_GET);
		_endthreadex(0);
		return 1;
	}

	// create directinput context
	IDirectInput8* dinput;

	HRESULT error_dinput =
		DirectInput8Create(
			win_module,
			DIRECTINPUT_VERSION,
			&IID_IDirectInput8W,
			(void**) &dinput,
			NULL);

	if (error_dinput != DI_OK)
	{
		punknobs_error_throw(
			punknobs,
			error,
			PUNKNOBS_ERROR_BACKEND_WIN_DINPUT_GET);
		_endthreadex(0);
		return 1;
	}

	// set closed variable
	bool closed = false;
	update_closed(backend, error, &closed);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		_endthreadex(0);
		return 1;
	}

	// # Refresh devices regularly
	DWORD timer = 0;
	DWORD millis = 0;
	DWORD last_timer = 0;

	while (closed == false)
	{
		// get monotonic clock's current time
		millis = GetTickCount();
		// use it to compute last loop's duration
		timer = elapsed(millis, last_timer);
		// sleep until the configured delay elapsed
		if (timer < backend->delays.delay_device_refresh)
		{
			Sleep(backend->delays.delay_device_refresh - timer);
		}
		// update loop's time reference with current time
		last_timer = GetTickCount();

		// ## List DirectInput devices
		backend->dinput->lpVtbl->EnumDevices(
			backend->dinput,
			DI8DEVCLASS_GAMECTRL,
			device_enum_callback,
			(void*) backend,
			DIEDFL_ALLDEVICES);

		// process device list
		struct win_device_enum_node_dinput* ref_ptr = backend->ref_enum_devices_dinput;
		struct win_device_enum_node_dinput* new_part_start = backend->new_enum_devices_dinput;
		struct win_device_enum_node_dinput* new_prev_start = new_part_start;
		struct win_device_enum_node_dinput* new_part = NULL;
		struct win_device_enum_node_dinput* new_prev = NULL;

		// ## Unregister removed devices
		while (ref_ptr != NULL)
		{
			// start comparing after already matched devices
			new_prev = new_prev_start;
			new_part = new_part_start;

			// seach for device in reference enumeration list
			while (new_part != NULL)
			{
				// device still present, move to the top of the list
				if (IsEqualGUID(&(ref_ptr->guid), &(new_part->guid)) == TRUE)
				{
					// lock mutex
					DWORD enum_lock =
						WaitForSingleObject(
							backend->mutex_enum,
							INFINITE);

					if (enum_lock != WAIT_OBJECT_0)
					{
						punknobs_error_throw(
							punknobs,
							error,
							PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
						_endthreadex(0);
						return 1;
					}

					// copy reg entry pointer
					if (ref_ptr->reg_entry != NULL)
					{
						new_part->reg_entry = ref_ptr->reg_entry;
					}

					// unlock mutex
					BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

					if (enum_unlock == 0)
					{
						punknobs_error_throw(
							punknobs,
							error,
							PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
						_endthreadex(0);
						return 1;
					}

					// we matched the first item of the new list
					// move the start position for our search down
					if (new_part == new_part_start)
					{
						if (new_part == backend->new_enum_devices_dinput)
						{
							new_prev_start = new_part_start;
						}

						new_part_start = new_part_start->next;
					}
					// we did not match on the first item but it is the first time
					// we match so the current item must become our previous start
					else if (new_prev_start == new_part_start)
					{
						new_prev_start = new_part;
					}

					// move the current item  to the top of the list
					if (new_part != backend->new_enum_devices_dinput)
					{
						new_prev->next = new_part->next;
						new_part->next = backend->new_enum_devices_dinput;
						backend->new_enum_devices_dinput = new_part;
					}

					// continue with next device removal check
					break;
				}

				// try next new enumeration device
				new_prev = new_part;
				new_part = new_part->next;
			}

			// lock mutex
			DWORD enum_lock =
				WaitForSingleObject(
					backend->mutex_enum,
					INFINITE);

			if (enum_lock != WAIT_OBJECT_0)
			{
				punknobs_error_throw(
					punknobs,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
				_endthreadex(0);
				return 1;
			}

			// device was removed, call unregistration callback
			if ((new_part == NULL) && (ref_ptr->reg_entry != NULL))
			{
				ref_ptr->info.plugged = false;

				// call user callback
				punknobs->device_callback(
					punknobs->device_custom_data,
					&(ref_ptr->info),
					error);

				// continue even in case of error
				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			// unlock mutex
			BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

			if (enum_unlock == 0)
			{
				punknobs_error_throw(
					punknobs,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
				_endthreadex(0);
				return 1;
			}

			// test next reference enumeration device
			ref_ptr = ref_ptr->next;
		}

		// ## Register newly plugged devices
		new_part = new_part_start;

		while (new_part != NULL)
		{
			// create new device
			IDirectInputDevice8* device;

			backend->dinput->lpVtbl->CreateDevice(
				backend->dinput,
				&(new_part->guid),
				&device,
				NULL);

			// prepare device
			device->lpVtbl->SetCooperativeLevel(
				device,
				GetActiveWindow(), 
				DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

			device->lpVtbl->SetDataFormat(
				device,
				&c_dfDIJoystick);

			device->lpVtbl->Acquire(
				device);

			// set node info
			new_part->device = device;

			// check for xbox controller
			DIPROPGUIDANDPATH prop_guidpath =
			{
				.diph =
				{
					.dwSize = sizeof (DIPROPGUIDANDPATH),
					.dwHeaderSize = sizeof (DIPROPHEADER),
					.dwObj = 0,
					.dwHow = DIPH_DEVICE,
				},
			};

			HRESULT prop_result =
				device->lpVtbl->GetProperty(
					device,
					DIPROP_GUIDANDPATH,
					&(prop_guidpath.diph));

			if ((prop_result != DI_OK) && (prop_result != S_FALSE))
			{
				// ignore
				new_part = new_part->next;
				continue;
			}

			char* hid_path = utf16_to_utf8(prop_guidpath.wszPath, true);

			if (hid_path == NULL)
			{
				// ignore
				punknobs_error_throw(punknobs, error, PUNKNOBS_ERROR_ALLOC);
				new_part = new_part->next;
				continue;
			}

			if (strstr(hid_path, "IG_") != NULL)
			{
				// skip xbox controllers
				new_part = new_part->next;
				continue;
			}

			// get direct input device info
			DIDEVICEINSTANCE dinput_info =
			{
				.dwSize = sizeof (DIDEVICEINSTANCE),
			};

			device->lpVtbl->GetDeviceInfo(device, &dinput_info);

			// prepare info
			char* name =
				utf16_to_utf8(
					dinput_info.tszProductName,
					false);

			if (name == NULL)
			{
				// ignore
				punknobs_error_throw(punknobs, error, PUNKNOBS_ERROR_ALLOC);
				new_part = new_part->next;
				continue;
			}

			DIPROPDWORD prop_vidpid =
			{
				.diph =
				{
					.dwSize = sizeof (DIPROPDWORD),
					.dwHeaderSize = sizeof (DIPROPHEADER),
					.dwObj = 0,
					.dwHow = DIPH_DEVICE,
				},
			};

			prop_result =
				device->lpVtbl->GetProperty(
					device,
					DIPROP_VIDPID,
					&(prop_vidpid.diph));

			if ((prop_result != DI_OK) && (prop_result != S_FALSE))
			{
				// ignore
				free(name);
				new_part = new_part->next;
				continue;
			}

			struct win_device_info info =
			{
				.name = name,
				.vendor_id = LOWORD(prop_vidpid.dwData),
				.product_id = HIWORD(prop_vidpid.dwData),
				.plugged = true,
				.registered = false,
				.api = PUNKNOBS_WIN_API_DIRECTINPUT,
				.device_enum_node.dinput = new_part,
			};

			new_part->info = info;

			// call user callback
			punknobs->device_callback(
				punknobs->device_custom_data,
				&info,
				error);

			// if the device was registered, do not release device
			if (new_part->reg_entry == NULL)
			{
				device->lpVtbl->Unacquire(device);
				device->lpVtbl->Release(device);
			}

			// we can free temporary strings in all cases
			free(name);

			// continue even in case of error
			if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
			{
				// ignore
			}

			// continue with next new device
			new_part = new_part->next;
		}

		// ## List XInput devices
		DWORD dwResult;

		for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
		{
			XINPUT_STATE state; // leave it dirty as we only need the return value
			dwResult = XInputGetState(i, &state);

			if (dwResult == ERROR_SUCCESS)
			{
				// allocate new enum device list node
				struct win_device_enum_node_xinput* enum_device =
					malloc(sizeof (struct win_device_enum_node_xinput));

				if (enum_device == NULL)
				{
					// ignore
					punknobs_error_throw(punknobs, error, PUNKNOBS_ERROR_ALLOC);
					continue;
				}

				// fill node values
				enum_device->id = i;
				enum_device->reg_entry = NULL;
				enum_device->next = backend->new_enum_devices_xinput;

				// add node to linked list
				backend->new_enum_devices_xinput = enum_device;
			}
		}

		// ## Register newly plugged devices
		struct win_device_enum_node_xinput* xinput_ref_ptr =
			backend->ref_enum_devices_xinput;
		struct win_device_enum_node_xinput* xinput_new_part_start =
			backend->new_enum_devices_xinput;
		struct win_device_enum_node_xinput* xinput_new_prev_start =
			xinput_new_part_start;
		struct win_device_enum_node_xinput* xinput_new_part =
			NULL;
		struct win_device_enum_node_xinput* xinput_new_prev =
			NULL;

		while (xinput_ref_ptr != NULL)
		{
			// start comparing after already matched devices
			xinput_new_prev = xinput_new_prev_start;
			xinput_new_part = xinput_new_part_start;

			// seach for device in reference enumeration list
			while (xinput_new_part != NULL)
			{
				// device still present, move to the top of the list
				if (xinput_ref_ptr->id == xinput_new_part->id)
				{
					// lock mutex
					DWORD enum_lock =
						WaitForSingleObject(
							backend->mutex_enum,
							INFINITE);

					if (enum_lock != WAIT_OBJECT_0)
					{
						punknobs_error_throw(
							punknobs,
							error,
							PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
						_endthreadex(0);
						return 1;
					}

					// copy reg entry pointer
					if (xinput_ref_ptr->reg_entry != NULL)
					{
						xinput_new_part->reg_entry =
							xinput_ref_ptr->reg_entry;
					}

					// unlock mutex
					BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

					if (enum_unlock == 0)
					{
						punknobs_error_throw(
							punknobs,
							error,
							PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
						_endthreadex(0);
						return 1;
					}

					// we matched the first item of the new list
					// move the start position for our search down
					if (xinput_new_part == xinput_new_part_start)
					{
						if (xinput_new_part == backend->new_enum_devices_xinput)
						{
							xinput_new_prev_start = xinput_new_part_start;
						}

						xinput_new_part_start = xinput_new_part_start->next;
					}
					// we did not match on the first item but it is the first time
					// we match so the current item must become our previous start
					else if (xinput_new_prev_start == xinput_new_part_start)
					{
						xinput_new_prev_start = xinput_new_part;
					}

					// move the current item to the top of the list
					if (xinput_new_part != backend->new_enum_devices_xinput)
					{
						xinput_new_prev->next = xinput_new_part->next;
						xinput_new_part->next = backend->new_enum_devices_xinput;
						backend->new_enum_devices_xinput = xinput_new_part;
					}

					// continue with next device removal check
					break;
				}

				// try next new enumeration device
				xinput_new_prev = xinput_new_part;
				xinput_new_part = xinput_new_part->next;
			}

			// lock mutex
			DWORD enum_lock =
				WaitForSingleObject(
					backend->mutex_enum,
					INFINITE);

			if (enum_lock != WAIT_OBJECT_0)
			{
				punknobs_error_throw(
					punknobs,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
				_endthreadex(0);
				return 1;
			}

			// device was removed, call unregistration callback
			if ((xinput_new_part == NULL) && (xinput_ref_ptr->reg_entry != NULL))
			{
				xinput_ref_ptr->info.plugged = false;

				// call user callback
				punknobs->device_callback(
					punknobs->device_custom_data,
					&(xinput_ref_ptr->info),
					error);

				// continue even in case of error
				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			// unlock mutex
			BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

			if (enum_unlock == 0)
			{
				punknobs_error_throw(
					punknobs,
					error,
					PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
				_endthreadex(0);
				return 1;
			}

			// test next reference enumeration device
			xinput_ref_ptr = xinput_ref_ptr->next;
		}

		// ## Register xinput devices
		xinput_new_part = xinput_new_part_start;

		while (xinput_new_part != NULL)
		{
			struct win_device_info info =
			{
				.name = PUNKNOBS_XUSB_HARDWARE_NAME,
				.vendor_id = (unsigned) -1,
				.product_id = (unsigned) -1,
				.plugged = false,
				.registered = false,
				.api = PUNKNOBS_WIN_API_XINPUT,
				.device_enum_node.xinput = xinput_new_part,
			};

			xinput_new_part->info = info;

			// call user callback
			punknobs->device_callback(
				punknobs->device_custom_data,
				&(xinput_new_part->info),
				error);

			// nothing to do if we didn't register
			if (xinput_new_part->reg_entry == NULL)
			{
				// ignore
			}

			// continue even in case of error
			if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
			{
				// ignore
				xinput_new_part = xinput_new_part->next;
				continue;
			}

			xinput_new_part = xinput_new_part->next;
		}

		// ## Save new enumeration list as reference enumeration list
		struct win_device_enum_node_dinput* tmp_node =
			backend->ref_enum_devices_dinput;
		struct win_device_enum_node_dinput* tmp_next =
			NULL;
		struct win_device_enum_node_xinput* tmp_xinput_node =
			backend->ref_enum_devices_xinput;
		struct win_device_enum_node_xinput* tmp_xinput_next =
			NULL;

		// lock mutex
		DWORD enum_lock =
			WaitForSingleObject(
				backend->mutex_enum,
				INFINITE);

		if (enum_lock != WAIT_OBJECT_0)
		{
			punknobs_error_throw(
				punknobs,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
			_endthreadex(0);
			return 1;
		}

		// replace reference enumeration list
		backend->ref_enum_devices_dinput = backend->new_enum_devices_dinput;
		backend->ref_enum_devices_xinput = backend->new_enum_devices_xinput;

		// free old reference enumeration list
		while (tmp_node != NULL)
		{
			tmp_next = tmp_node->next;
			free(tmp_node);
			tmp_node = tmp_next;
		}

		while (tmp_xinput_node != NULL)
		{
			tmp_xinput_next = tmp_xinput_node->next;
			free(tmp_xinput_node);
			tmp_xinput_node = tmp_xinput_next;
		}

		// copy enum entry pointer
		new_part = backend->new_enum_devices_dinput;
			
		while (new_part != new_part_start)
		{
			if (new_part->reg_entry != NULL)
			{
				new_part->reg_entry->enum_entry = new_part;
			}

			new_part = new_part->next;
		}

		xinput_new_part = backend->new_enum_devices_xinput;
			
		while (xinput_new_part != xinput_new_part_start)
		{
			if (xinput_new_part->reg_entry != NULL)
			{
				xinput_new_part->reg_entry->enum_entry = xinput_new_part;
			}

			xinput_new_part = xinput_new_part->next;
		}

		// reset new enumeration list pointer
		backend->new_enum_devices_dinput = NULL;
		backend->new_enum_devices_xinput = NULL;

		// unlock mutex
		BOOL enum_unlock = ReleaseMutex(backend->mutex_enum);

		if (enum_unlock == 0)
		{
			punknobs_error_throw(
				punknobs,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			_endthreadex(0);
			return 1;
		}

		// update closed
		update_closed(
			backend,
			error,
			&closed);

		if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
		{
			_endthreadex(0);
			return 1;
		}
	}

	_endthreadex(0);
	return 0;
}

// input thread
unsigned __stdcall input_loop(void* data)
{
	struct win_thread_device_loop_data* thread_data = data;
	struct win_backend* backend = thread_data->context;
	struct punknobs_error_info* error = thread_data->error;
	struct punknobs* punknobs = backend->punknobs;

	// set closed variable
	bool closed = false;
	update_closed(backend, error, &closed);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		_endthreadex(0);
		return 1;
	}

	// # Refresh devices regularly
	DIJOYSTATE state;
	HRESULT error_state;
	DWORD timer = 0;
	DWORD millis = 0;
	DWORD last_timer = 0;

	while (closed == false)
	{
		// get monotonic clock's current time
		millis = GetTickCount();
		// use it to compute last loop's duration
		timer = elapsed(millis, last_timer);
		// sleep until the configured delay elapsed
		if (timer < backend->delays.delay_input_refresh)
		{
			Sleep(backend->delays.delay_input_refresh - timer);
		}
		// update loop's time reference with current time
		last_timer = GetTickCount();

		// lock mutex
		DWORD reg_lock =
			WaitForSingleObject(
				backend->mutex_reg,
				INFINITE);

		if (reg_lock != WAIT_OBJECT_0)
		{
			punknobs_error_throw(
				punknobs,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK);
			_endthreadex(0);
			return 1;
		}

		// poll devices and call the user's input callback
		struct win_device_reg_node_dinput* node = backend->reg_devices_dinput;
		struct win_input_info info = {0};

		while (node != NULL)
		{
			// get device state
			error_state =
				node->enum_entry->device->lpVtbl->GetDeviceState(
					node->enum_entry->device,
					sizeof (DIJOYSTATE),
					&state);

			if (error_state != DI_OK)
			{
				// ignore
				node = node->next;
				continue;
			}

			// report inputs
			info.time = last_timer;
			info.api = PUNKNOBS_WIN_API_DIRECTINPUT;
			info.device_enum_node.dinput = node->enum_entry;

			if (node->state.lX != state.lX)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_X;
				info.value = state.lX;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (node->state.lY != state.lY)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_Y;
				info.value = state.lY;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (node->state.lZ != state.lZ)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_Z;
				info.value = state.lZ;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (node->state.lRx != state.lRx)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_RX;
				info.value = state.lRx;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (node->state.lRy != state.lRy)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_RY;
				info.value = state.lRy;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (node->state.lRz != state.lRz)
			{
				info.type = PUNKNOBS_DIRECTINPUT_TYPE_AXIS;
				info.code = PUNKNOBS_DIRECTINPUT_CODE_RZ;
				info.value = state.lRz;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			for (size_t i = 0; i < 2; ++i)
			{
				if (node->state.rglSlider[i] != state.rglSlider[i])
				{
					info.type = PUNKNOBS_DIRECTINPUT_TYPE_SLIDER;
					info.code = PUNKNOBS_DIRECTINPUT_CODE_SLIDER_1 + i;
					info.value = state.rglSlider[i];

					punknobs->inputs_callback(
						punknobs->inputs_custom_data,
						&info,
						error);

					if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
					{
						// ignore
					}
				}
			}

			for (size_t i = 0; i < 4; ++i)
			{
				if (node->state.rgdwPOV[i] != state.rgdwPOV[i])
				{
					info.type = PUNKNOBS_DIRECTINPUT_TYPE_HAT;
					info.code = PUNKNOBS_DIRECTINPUT_CODE_HAT_1 + i;
					info.value = state.rgdwPOV[i];

					punknobs->inputs_callback(
						punknobs->inputs_custom_data,
						&info,
						error);

					if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
					{
						// ignore
					}
				}
			}

			for (size_t i = 0; i < 32; ++i)
			{
				if (node->state.rgbButtons[i] != state.rgbButtons[i])
				{
					info.type = PUNKNOBS_DIRECTINPUT_TYPE_BUTTON;
					info.code = PUNKNOBS_DIRECTINPUT_CODE_BUTTON_1 + i;
					info.value = state.rgbButtons[i];

					punknobs->inputs_callback(
						punknobs->inputs_custom_data,
						&info,
						error);

					if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
					{
						// ignore
					}
				}
			}

			// save state
			node->state = state;
			node = node->next;
		}

		// poll xinput
		struct win_device_reg_node_xinput* xinput_node = backend->reg_devices_xinput;
		struct win_input_info xinput_info = {0};

		DWORD dwResult;
		DWORD xinput_id;

		while (xinput_node != NULL)
		{
			XINPUT_STATE state;
			memset(&state, 0, sizeof (XINPUT_STATE));

			xinput_id = xinput_node->enum_entry->id;
			dwResult = XInputGetState(xinput_id, &state);

			xinput_info.time = last_timer;
			xinput_info.api = PUNKNOBS_WIN_API_XINPUT;
			xinput_info.device_enum_node.xinput = xinput_node->enum_entry;

			// continue on error
			if (dwResult != ERROR_SUCCESS)
			{
				xinput_node = xinput_node->next;
				continue;
			}

			// continue if no change
			if (state.dwPacketNumber == xinput_node->state.dwPacketNumber)
			{
				xinput_node = xinput_node->next;
				continue;
			}

			// button codes
			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_DPAD_UP;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_DPAD_DOWN;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data, 
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_DPAD_LEFT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_DPAD_RIGHT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_START))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_START;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_START) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_BACK;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_STICK_LEFT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_STICK_RIGHT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_SHOULDER_LEFT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_SHOULDER_RIGHT;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_A))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_A;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_B))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_B;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_X)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_X))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_X;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
			!= (xinput_node->state.Gamepad.wButtons & XINPUT_GAMEPAD_Y))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_Y;
				xinput_info.value =
					(state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

#if 0
			if ((state.Gamepad.wButtons & 0x400)
			!= (xinput_node->state.Gamepad.wButtons & 0x400))
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_BUTTON;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_BUTTON_GUIDE;
				xinput_info.value =
					(state.Gamepad.wButtons & 0x400) ?
					PUNKNOBS_XINPUT_VALUE_BUTTON_PRESSED :
					PUNKNOBS_XINPUT_VALUE_BUTTON_RELEASED;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}
#endif

			if (state.Gamepad.bLeftTrigger != xinput_node->state.Gamepad.bLeftTrigger)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_TRIGGER_LEFT;
				xinput_info.value = state.Gamepad.bLeftTrigger;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (state.Gamepad.bRightTrigger != xinput_node->state.Gamepad.bRightTrigger)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_TRIGGER_RIGHT;
				xinput_info.value = state.Gamepad.bRightTrigger;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (state.Gamepad.sThumbLX != xinput_node->state.Gamepad.sThumbLX)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_STICK_LEFT_HORIZONTAL;
				xinput_info.value = state.Gamepad.sThumbLX;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (state.Gamepad.sThumbLY != xinput_node->state.Gamepad.sThumbLY)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_STICK_LEFT_VERTICAL;
				xinput_info.value = state.Gamepad.sThumbLY;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (state.Gamepad.sThumbRX != xinput_node->state.Gamepad.sThumbRX)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_STICK_RIGHT_HORIZONTAL;
				xinput_info.value = state.Gamepad.sThumbRX;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			if (state.Gamepad.sThumbRY != xinput_node->state.Gamepad.sThumbRY)
			{
				xinput_info.type = PUNKNOBS_XINPUT_TYPE_AXIS;
				xinput_info.code = PUNKNOBS_XINPUT_CODE_AXIS_STICK_RIGHT_VERTICAL;
				xinput_info.value = state.Gamepad.sThumbRY;

				punknobs->inputs_callback(
					punknobs->inputs_custom_data,
					&xinput_info,
					error);

				if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
				{
					// ignore
				}
			}

			// save state
			xinput_node->state = state;
			xinput_node = xinput_node->next;
		}

		// unlock mutex
		BOOL reg_unlock = ReleaseMutex(backend->mutex_reg);

		if (reg_unlock == 0)
		{
			punknobs_error_throw(
				punknobs,
				error,
				PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK);
			_endthreadex(0);
			return 1;
		}

		// update closed
		update_closed(backend, error, &closed);

		if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
		{
			_endthreadex(0);
			return 1;
		}
	}

	_endthreadex(0);
	return 0;
}

// utf16 to utf8 conversion
char* utf16_to_utf8(WCHAR* win_string, bool capitalize)
{
	// early exit if utf16 input is NULL
	if (win_string == NULL)
	{
		// input error
		return NULL;
	}

	// utf8 buffer
	size_t utf8_alloc_step = 32 * 4; // allocation increment for the utf8 buffer in bytes
	size_t utf8_alloc_size = 1; // allocation size of the utf8 buffer in bytes
	char* utf8 = NULL; // utf8 buffer

	// initialize loop variables
	uint32_t utf16 = '\0'; // current utf16 character
	uint32_t tmp = 0; // temporary space for quad-byte utf16 reconstruction
	char* utf8_string = NULL; // current utf8 codepoint in the utf8 buffer 
	size_t utf8_size = 0; // length of the converted utf8 string in bytes

	// only exit the loop *after* we have converted NUL
	do
	{
		// get two bytes of utf16 text
		utf16 = *win_string;

		// increase memory allocation when we are running out of space
		if ((utf8_alloc_size - utf8_size) < 5)
		{
			utf8_alloc_size += utf8_alloc_step;
			utf8 = realloc(utf8, utf8_alloc_size);

			if (utf8 == NULL)
			{
				// alloc error
				return NULL;
			}
		}

		// recompute the current utf8 codepoint pointer here
		// in case we reallocated the underlying buffer
		utf8_string = utf8 + utf8_size;

		// quad-byte utf16 UCS reconstruction
		if ((utf16 & 0xFC00) == 0xD800)
		{
			// the first six bits of the first two bytes
			// start like 0xD8, try loading two more bytes
			// to decode the ucs value from these four bytes
			tmp = (0x3FF & utf16) << 10;
			++win_string;
			utf16 = *win_string;

			if ((utf16 & 0xFC00) == 0xDC00)
			{
				// the first six bits of the next two bytes
				// start like 0xDC, decode the ucs value from
				// all four bytes before starting utf8 encoding
				utf16 = ((0x3FF & utf16) | tmp) + 0x10000;
			}

			// if the first six bits of the next two bytes
			// did not start like 0xDC, we will fall back
			// to handling them as double-byte utf16,
			// ignoring the invalid first two bytes
		}

		// utf8 conversion from ucs
		if (utf16 < 0x80)
		{
			utf8_string[0] = capitalize && (islower(utf16) > 0) ? (utf16 - 32) : utf16;
			utf8_size += 1;
		}
		else if (utf16 < 0x800)
		{
			utf8_string[0] = 0xC0 | (0x1F & (utf16 >> 6));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 2;
		}
		else if (utf16 < 0x10000)
		{
			utf8_string[0] = 0xE0 | (0x0F & (utf16 >> 12));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 6));
			utf8_string[2] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 3;
		}
		else
		{
			utf8_string[0] = 0xF0 | (0x07 & (utf16 >> 18));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 12));
			utf8_string[2] = 0x80 | (0x3F & (utf16 >> 6));
			utf8_string[3] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 4;
		}

		// prepare getting the next two bytes of utf16 text
		++win_string;
	}
	while (utf16 != '\0');

	// shrink the utf8 buffer to the actual length of the converted string
	utf8 = realloc(utf8, utf8_size);

	if (utf8 == NULL)
	{
		// alloc error
		return NULL;
	}

	// all good
	return utf8;
}
