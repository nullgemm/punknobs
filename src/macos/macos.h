#ifndef H_PUNKNOBS_BACKEND_MACOS
#define H_PUNKNOBS_BACKEND_MACOS

#import <ForceFeedback.h>
#import <Foundation/Foundation.h>
#import <IOHIDDevice.h>
#import <IOHIDManager.h>

struct macos_device_info
{
	intptr_t punknobs_id;
	char* manufacturer_name;
	char* product_name;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;

	// force feedback
	IOHIDDeviceRef device;
	io_service_t ff_service;
	FFDeviceObjectReference ff_device;
	FFEffectObjectReference* ff_effect_objs;
	int ff_effects_max;
	bool ff_available;
};

struct macos_input_info
{
	IOHIDDeviceRef device;
	IOHIDValueRef input_value;
};

struct macos_device_node
{
	struct macos_device_info info;
	struct macos_device_node* next;
};

struct macos_thread_data
{
	struct punknobs* punknobs;
	struct macos_backend* backend;
};

struct punknobs;
@class PunknobsThread;

struct macos_backend
{
	struct punknobs* punknobs;
	struct macos_device_node* devices;
	PunknobsThread* thread;
};

#endif
