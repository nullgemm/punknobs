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

uint32_t lut_features[PUNKNOBS_HAPTICS_FEATURE_COUNT] =
{
	[PUNKNOBS_HAPTICS_FEATURE_CONSTANT] = FFCAP_ET_CONSTANTFORCE,
	[PUNKNOBS_HAPTICS_FEATURE_SPRING] = FFCAP_ET_SPRING,
	[PUNKNOBS_HAPTICS_FEATURE_FRICTION] = FFCAP_ET_FRICTION,
	[PUNKNOBS_HAPTICS_FEATURE_DAMPER] = FFCAP_ET_DAMPER,
	[PUNKNOBS_HAPTICS_FEATURE_INERTIA] = FFCAP_ET_INERTIA,
	[PUNKNOBS_HAPTICS_FEATURE_RAMP] = FFCAP_ET_RAMPFORCE,
};

uint32_t lut_waveforms[PUNKNOBS_HAPTICS_WAVEFORM_COUNT] =
{
	[PUNKNOBS_HAPTICS_WAVEFORM_SQUARE] = FFCAP_ET_SQUARE,
	[PUNKNOBS_HAPTICS_WAVEFORM_TRIANGLE] = FFCAP_ET_TRIANGLE,
	[PUNKNOBS_HAPTICS_WAVEFORM_SINE] = FFCAP_ET_SINE,
	[PUNKNOBS_HAPTICS_WAVEFORM_SAW_UP] = FFCAP_ET_SAWTOOTHUP,
	[PUNKNOBS_HAPTICS_WAVEFORM_SAW_DOWN] = FFCAP_ET_SAWTOOTHDOWN,
	[PUNKNOBS_HAPTICS_WAVEFORM_CUSTOM] = FFCAP_ET_CUSTOMFORCE,
};

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
	HRESULT error_ff = FF_OK;

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

			// get service id from IOHIDDevice
			device->info.ff_service =
				IOHIDDeviceGetService((IOHIDDeviceRef) id);

			if (service == MACH_PORT_NULL)
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_MACOS_IOSERVICE);
				return;
			}

			// create FFDeviceObject from service id
			error_ff =
				FFCreateDevice(
					device->info.ff_service,
					&(device->info.ff_device));

			if (error_ff != FF_OK)
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
				return;
			}

			// get features
			FFCAPABILITIES ff_features;

			error_ff =
				FFDeviceGetForceFeedbackCapabilities(
					device->info.ff_device,
					&ff_features);

			if (error_ff != FF_OK)
			{
				FFReleaseDevice(device);
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_MACOS_FFGETCAPABILITIES);
				return;
			}

			// allocate effects array
			device->info.ff_effect_objs =
				malloc(
					ff_features.storageCapacity
					* (sizeof (FFEffectObjectReference)));

			if (device->info.ff_effect_objs == NULL)
			{
				FFReleaseDevice(device);
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_ALLOC);
				return;
			}

			// set critical force feedback info
			device->info.ff_effects_max = ff_features.storageCapacity;
			device->info.ff_available = true;
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
	HRESULT error_ff = FF_OK;

	while (device != NULL)
	{
		if ((device->info.punknobs_id == id)
		&& (device->info.registered == true))
		{
			if (device->info.ff_available == true)
			{
				// release force feedback device
				FFReleaseDevice(device->info.ff_device);

				// free effects array
				if (device->info.ff_effect_objs == NULL)
				{
					free(device->info.ff_effect_objs);
				}

				// reset force feedback service
				device->info.ff_service = MACH_PORT_NULL;
				device->info.ff_effects_max = 0;
				device->info.ff_effect_objs = NULL;
				device->info.ff_available = false;
			}

			// unregister input device callback
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

// haptics management
void punknobs_macos_haptics_get_features(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_features* features,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	HRESULT error_ff = FF_OK;

	// get service id from IOHIDDevice
	io_service_t service = IOHIDDeviceGetService((IOHIDDeviceRef) id);

	if (service == MACH_PORT_NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_IOSERVICE);
		return;
	}

	// create FFDeviceObject from service id
	FFDeviceObjectReference device;
	error_ff = FFCreateDevice(service, &device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
		return;
	}

	// get features
	FFCAPABILITIES ff_features;
	error_ff = FFDeviceGetForceFeedbackCapabilities(device, &ff_features);

	if (error_ff != FF_OK)
	{
		FFReleaseDevice(device);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFGETCAPABILITIES);
		return;
	}

	// allocate features list
	features->list =
		malloc(
			PUNKNOBS_HAPTICS_FEATURE_COUNT
			* (sizeof (enum punknobs_haptics_feature)));

	if (features->list == NULL)
	{
		FFReleaseDevice(device);
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// process direct features list
	size_t count = 0;

	for (int i = 0; i < PUNKNOBS_HAPTICS_FEATURE_COUNT; ++i)
	{
		if (ff_features.supportedEffects & lut_features[i])
		{
			features->list[count] = i;
			++count;
		}
	}

	// check rumble/periodic
	int i = 0;

	while (i < PUNKNOBS_HAPTICS_WAVEFORM_COUNT)
	{
		if (ff_features.supportedEffects & lut_waveforms[i])
		{
			features->list[count] = PUNKNOBS_HAPTICS_FEATURE_RUMBLE;
			++count;

			features->list[count] = PUNKNOBS_HAPTICS_FEATURE_PERIODIC;
			++count;

			break;
		}

		++i;
	}

	// check gain/autocenter
	uint32_t value;

	error_ff =
		FFDeviceGetForceFeedbackProperty(
			device,
			FFPROP_FFGAIN,
			&value,
			sizeof(value));

	if (error_ff == FF_OK)
	{
		features->list[count] = PUNKNOBS_HAPTICS_FEATURE_GAIN;
		++count;
	}

	error_ff =
		FFDeviceGetForceFeedbackProperty(
			device,
			FFPROP_AUTOCENTER,
			&value,
			sizeof(value));

	if (error_ff == FF_OK)
	{
		features->list[count] = PUNKNOBS_HAPTICS_FEATURE_AUTOCENTER;
		++count;
	}

	// set final feature count
	features->count = count;

	// release force feedback device
	error_ff = FFReleaseDevice(device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFRELEASEDEVICE);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_haptics_get_waveforms(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_waveforms* waveforms,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	HRESULT error_ff = FF_OK;

	// get service id from IOHIDDevice
	io_service_t service = IOHIDDeviceGetService((IOHIDDeviceRef) id);

	if (service == MACH_PORT_NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_IOSERVICE);
		return;
	}

	// create FFDeviceObject from service id
	FFDeviceObjectReference device;
	error_ff = FFCreateDevice(service, &device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
		return;
	}

	// get features
	FFCAPABILITIES ff_features;
	error_ff = FFDeviceGetForceFeedbackCapabilities(device, &ff_features);

	if (error_ff != FF_OK)
	{
		FFReleaseDevice(device);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFGETCAPABILITIES);
		return;
	}

	// allocate features list
	waveforms->list =
		malloc(
			PUNKNOBS_HAPTICS_WAVEFORM_COUNT
			* (sizeof (enum punknobs_haptics_feature)));

	if (waveforms->list == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// process features list
	size_t count = 0;

	for (int i = 0; i < PUNKNOBS_HAPTICS_WAVEFORM_COUNT; ++i)
	{
		if (ff_features.supportedEffects & lut_waveforms[i])
		{
			waveforms->list[count] = i;
			++count;
		}
	}

	waveforms->count = count;

	// release force feedback device
	error_ff = FFReleaseDevice(device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFRELEASEDEVICE);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

int punknobs_macos_haptics_effect_max(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	HRESULT error_ff = FF_OK;

	// get service id from IOHIDDevice
	io_service_t service = IOHIDDeviceGetService((IOHIDDeviceRef) id);

	if (service == MACH_PORT_NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_IOSERVICE);
		return;
	}

	// create FFDeviceObject from service id
	FFDeviceObjectReference device;
	error_ff = FFCreateDevice(service, &device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
		return;
	}

	// get features
	FFCAPABILITIES ff_features;
	error_ff = FFDeviceGetForceFeedbackCapabilities(device, &ff_features);

	if (error_ff != FF_OK)
	{
		FFReleaseDevice(device);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFGETCAPABILITIES);
		return;
	}

	int max = ff_features.storageCapacity;

	// release force feedback device
	error_ff = FFReleaseDevice(device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFRELEASEDEVICE);
		return;
	}

	// all good
	punknobs_error_ok(error);
	return max;
}

int punknobs_macos_haptics_effect_set(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_effect* effect,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	HRESULT error_ff = FF_OK;

	// get service id from IOHIDDevice
	io_service_t service = IOHIDDeviceGetService((IOHIDDeviceRef) id);

	if (service == MACH_PORT_NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_IOSERVICE);
		return -1;
	}

	// create FFDeviceObject from service id
	FFDeviceObjectReference device;
	error_ff = FFCreateDevice(service, &device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
		return -1;
	}

	// create effect
	CFUUIDRef uuid;

	switch (effect->type)
	{
		case PUNKNOBS_HAPTICS_FEATURE_CONSTANT:
		{
			uuid = kFFEffectType_ConstantForce_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_RAMP:
		{
			uuid = kFFEffectType_RampForce_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_PERIODIC:
		{
			switch (effect->config.periodic.waveform)
			{
				case PUNKNOBS_HAPTICS_WAVEFORM_SQUARE:
				{
					uuid = kFFEffectType_Square_ID;
					break;
				}
				case PUNKNOBS_HAPTICS_WAVEFORM_TRIANGLE:
				{
					uuid = kFFEffectType_Triangle_ID;
					break;
				}
				case PUNKNOBS_HAPTICS_WAVEFORM_SINE:
				{
					uuid = kFFEffectType_Sine_ID;
					break;
				}
				case PUNKNOBS_HAPTICS_WAVEFORM_SAW_UP:
				{
					uuid = kFFEffectType_SawtoothUp_ID;
					break;
				}
				case PUNKNOBS_HAPTICS_WAVEFORM_SAW_DOWN:
				{
					uuid = kFFEffectType_SawtoothDown_ID;
					break;
				}
				case PUNKNOBS_HAPTICS_WAVEFORM_CUSTOM:
				{
					uuid = kFFEffectType_CustomForce_ID;
					break;
				}
				default:
				{
					punknobs_error_throw(
						context,
						error,
						PUNKNOBS_ERROR_BACKEND_MACOS_WAVEFORM_TYPE);
					return -1;
				}
			}
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_SPRING:
		{
			uuid = kFFEffectType_Spring_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_FRICTION:
		{
			uuid = kFFEffectType_Friction_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_DAMPER:
		{
			uuid = kFFEffectType_Damper_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_INERTIA:
			uuid = kFFEffectType_Inertia_ID;
			break;
		}
		case PUNKNOBS_HAPTICS_FEATURE_RUMBLE:
		{
			uuid = kFFEffectType_Sine_ID;
			break;
		}
		default:
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_MACOS_EFFECT_TYPE);
			return -1;
		}
	}

	FFEFFECT effect =
	{
		.cAxes = ,
		.cbTypeSpecificParams = ,
		.dwDuration = ,
		.dwFlags = ,
		.dwGain = ,
		.dwSamplePeriod = ,
		.dwSize = ,
		.dwStartDelay = ,
		.dwTriggerButton = ,
		.dwTriggerRepeatInterval = ,
		.lpEnvelope = ,
		.lpvTypeSpecificParams = ,
		.rgdwAxes = ,
		.rglDirection = ,
	};

	//TODO add to linked list in device (this way the API still makes sense as the use *has* to be careful with the max effect count)
	//TODO or malloc using max when the device is created, and malloc here to store the pointer in this array (so we can tell if some are NULL)
	FFEffectObjectReference effect_obj;

	error_ff = FFDeviceCreateEffect(device, uuid, &effect);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFDEVICECREATEEFFECT);
		return -1;
	}

	// upload effect
	error_ff = FFEffectDownload(effect_obj);

	if (error_ff != FF_OK)
	{
		FFDeviceReleaseEffect(device, effect_obj);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFEFFECTDOWNLOAD);
		return -1;
	}

	// release force feedback device
	error_ff = FFReleaseDevice(device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFRELEASEDEVICE);
		return -1;
	}

	// all good
	punknobs_error_ok(error);
	return;
}

void punknobs_macos_haptics_effect_del(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	HRESULT error_ff = FF_OK;

	// get service id from IOHIDDevice
	io_service_t service = IOHIDDeviceGetService((IOHIDDeviceRef) id);

	if (service == MACH_PORT_NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_IOSERVICE);
		return -1;
	}

	// create FFDeviceObject from service id
	FFDeviceObjectReference device;
	error_ff = FFCreateDevice(service, &device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFCREATEDEVICE);
		return -1;
	}

	// TODO
	// release device effect
	FFEffectObjectReference* effect_obj = ;
	error_ff = FFDeviceReleaseEffect(device, *effect_obj);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFDEVICERELEASEEFFECT);
		return;
	}

	// release force feedback device
	error_ff = FFReleaseDevice(device);

	if (error_ff != FF_OK)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_MACOS_FFRELEASEDEVICE);
		return -1;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_haptics_gain_set(
	struct punknobs* context,
	intptr_t id,
	int gain,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		return;
	}

	// find ptr for given device fd
	struct evdev_epoll_info* input_loop_fds = backend->input_loop_fds->next;

	while (input_loop_fds != NULL)
	{
		if (((intptr_t) input_loop_fds) == id)
		{
			break;
		}

		input_loop_fds = input_loop_fds->next;
	}

	if (input_loop_fds == NULL)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_DOMAIN);
		return;
	}

	// set gain
	struct input_event event =
	{
		.type = EV_FF,
		.code = FF_GAIN,
		.value = 0xFFFFUL * gain / 100,
	};

	error_posix =
		write(
			input_loop_fds->device_fd,
			(void*) &event,
			sizeof (struct input_event));

	if (error_posix == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_GAIN_SET);
		return;
	}

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_haptics_autocenter_set(
	struct punknobs* context,
	intptr_t id,
	int autocenter,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		return;
	}

	// find ptr for given device fd
	struct evdev_epoll_info* input_loop_fds = backend->input_loop_fds->next;

	while (input_loop_fds != NULL)
	{
		if (((intptr_t) input_loop_fds) == id)
		{
			break;
		}

		input_loop_fds = input_loop_fds->next;
	}

	if (input_loop_fds == NULL)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_DOMAIN);
		return;
	}

	// set gain
	struct input_event event =
	{
		.type = EV_FF,
		.code = FF_AUTOCENTER,
		.value = 0xFFFFUL * autocenter / 100,
	};

	error_posix =
		write(
			input_loop_fds->device_fd,
			(void*) &event,
			sizeof (struct input_event));

	if (error_posix == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_AUTOCENTER_SET);
		return;
	}

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_haptics_effect_play(
	struct punknobs* context,
	intptr_t id,
	int slot,
	int repeat,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		return;
	}

	// find ptr for given device fd
	struct evdev_epoll_info* input_loop_fds = backend->input_loop_fds->next;

	while (input_loop_fds != NULL)
	{
		if (((intptr_t) input_loop_fds) == id)
		{
			break;
		}

		input_loop_fds = input_loop_fds->next;
	}

	if (input_loop_fds == NULL)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_DOMAIN);
		return;
	}

	// set gain
	struct input_event event =
	{
		.type = EV_FF,
		.code = slot,
		.value = repeat,
	};

	error_posix =
		write(
			input_loop_fds->device_fd,
			(void*) &event,
			sizeof (struct input_event));

	if (error_posix == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_PLAY);
		return;
	}

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_macos_haptics_effect_stop(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct macos_backend* backend = context->backend_context;
	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		return;
	}

	// find ptr for given device fd
	struct evdev_epoll_info* input_loop_fds = backend->input_loop_fds->next;

	while (input_loop_fds != NULL)
	{
		if (((intptr_t) input_loop_fds) == id)
		{
			break;
		}

		input_loop_fds = input_loop_fds->next;
	}

	if (input_loop_fds == NULL)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_DOMAIN);
		return;
	}

	// set gain
	struct input_event event =
	{
		.type = EV_FF,
		.code = slot,
		.value = 0,
	};

	error_posix =
		write(
			input_loop_fds->device_fd,
			(void*) &event,
			sizeof (struct input_event));

	if (error_posix == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_STOP);
		return;
	}

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		return;
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

	config->haptics_get_features = punknobs_macos_haptics_get_features;
	config->haptics_get_waveforms = punknobs_macos_haptics_get_waveforms;
	config->haptics_effect_max = punknobs_macos_haptics_effect_max;
	config->haptics_effect_set = punknobs_macos_haptics_effect_set;
	config->haptics_effect_del = punknobs_macos_haptics_effect_del;
	config->haptics_gain_set = punknobs_macos_haptics_gain_set;
	config->haptics_autocenter_set = punknobs_macos_haptics_autocenter_set;
	config->haptics_effect_play = punknobs_macos_haptics_effect_play;
	config->haptics_effect_stop = punknobs_macos_haptics_effect_stop;

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
