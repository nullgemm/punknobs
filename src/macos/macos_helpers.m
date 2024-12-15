#include "include/punknobs.h"

#include "common/punknobs_private.h"
#include "macos/macos.h"
#include "macos/macos_helpers.h"

#import <Foundation/Foundation.h>
#import <IOHIDDevice.h>
#import <IOHIDManager.h>

// # Some helpers used by the thread class below
void punknobs_backend_macos_helper_device(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDDeviceRef device,
	bool plugged)
{
	struct punknobs* context = punknobs;
	struct punknobs_backend_macos* backend = context->backend;
	struct punknobs_error_info error;

	// get device vendor/product ids
    CFNumberRef vendor_ref =
		(CFNumberRef) IOHIDDeviceGetProperty(
				device,
				CFSTR(kIOHIDVendorIDKey));

    CFNumberRef product_ref =
		(CFNumberRef) IOHIDDeviceGetProperty(
			device,
			CFSTR(kIOHIDProductIDKey));

    int vendor = ((NSNumber*) vendor_ref).intValue;
    int product = ((NSNumber*) product_ref).intValue;

	// get manufacturer and product strings
	CFStringRef manufacturer_name_ref =
		(CFStringRef) IOHIDDeviceGetProperty(
			device,
			CFSTR(kIOHIDManufacturerKey));

	CFStringRef product_name_ref =
		(CFStringRef) IOHIDDeviceGetProperty(
			device,
			CFSTR(kIOHIDProductKey));

	// get string lengths
	CFIndex manufacturer_name_glyphs =
		CFStringGetLength(
			manufacturer_name_ref);

	CFIndex product_name_glyphs =
		CFStringGetLength(
			product_name_ref);

	CFIndex manufacturer_name_len =
		CFStringGetMaximumSizeForEncoding(
			manufacturer_name_glyphs,
			kCFStringEncodingUTF8) + 1;

	CFIndex product_name_len =
		CFStringGetMaximumSizeForEncoding(
			product_name_glyphs,
			kCFStringEncodingUTF8) + 1;

	// allocate buffers for strings
	char* manufacturer_name = malloc(manufacturer_name_len);

	if (manufacturer_name == NULL)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_ALLOC);
		return;
	}

	char* product_name = malloc(product_name_len);

	if (product_name == NULL)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// get strings from CFStringRefs
	bool error_cf;

	error_cf =
		CFStringGetCString(
			manufacturer_name_ref,
			manufacturer_name,
			manufacturer_name_len,
			kCFStringEncodingUTF8);

	if (error_cf != true)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_BACKEND_MACOS_CSTRING);
		return;
	}

	error_cf =
		CFStringGetCString(
			product_name_ref,
			product_name,
			product_name_len,
			kCFStringEncodingUTF8);

	if (error_cf != true)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_BACKEND_MACOS_CSTRING);
		return;
	}

	struct punknobs_backend_macos_device_info info;

	if (plugged == true)
	{
		// run callback
		info.punknobs_id = (intptr_t) device;
		info.manufacturer_name = manufacturer_name;
		info.product_name = product_name;
		info.id_vendor = vendor;
		info.id_product = product;
		info.plugged = plugged;
		info.registered = false;

		// save device in list
		struct macos_device_node* device = malloc(sizeof (struct macos_device_node));

		if (device == NULL)
		{
			punknobs_error_throw(
				context,
				&error,
				PUNKNOBS_ERROR_ALLOC);
			return;
		}

		device.info = info;
		device->next = backend->devices;
		backend->devices = device;
	}
	else
	{
		struct macos_device_node* device = backend->devices;
		struct macos_device_node* device_prev = device;
		struct macos_device_node* device_next = NULL;

		while (device != NULL)
		{
			device_next = device->next;

			if (device->info.punknobs_id == ((intptr_t) device))
			{
				info.punknobs_id = (intptr_t) device;
				info.manufacturer_name = device->info.manufacturer_name;
				info.product_name = device->info.product_name;
				info.id_vendor = device->info.id_vendor;
				info.id_product = device->info.id_product;
				info.plugged = device->info.plugged;
				info.registered = device->info.registered;

				if (device_prev == device)
				{
					backend->devices = device_next;
				}
				else
				{
					device_prev->next = device_next;
				}

				free(device);

				break;
			}

			device_prev = device;
			device = device->next;
		}
	}

	// execute callback
	context->device_callback(
		context->device_custom_data,
		&info,
		&error);

	if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}
}

void device_added(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDDeviceRef device)
{
	macos_helper_device(punknobs, result, sender, device, true);
}

void device_removed(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDDeviceRef device)
{
	macos_helper_device(punknobs, result, sender, device, false);
}

// find dictionary info about device type
CFDictionaryRef dictionary(uint32_t page, uint32_t usage)
{
    CFNumberRef page_number =
		CFNumberCreate(
			kCFAllocatorDefault,
			kCFNumberIntType,
			&page);

    CFNumberRef usage_number =
		CFNumberCreate(
			kCFAllocatorDefault,
			kCFNumberIntType,
			&usage);

    CFStringRef keys[] =
	{
		CFSTR(kIOHIDDeviceUsagePageKey),
		CFSTR(kIOHIDDeviceUsageKey),
	};

    CFNumberRef values[] =
	{
		page_number,
		usage_number,
	};

    return CFDictionaryCreate(
		kCFAllocatorDefault,
		(const void**) keys,
		(const void**) values,
		2,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
}

// # Thread class implementation
@implementation PunknobsThread
// ## Getters and setters
// for the thread and cond
@synthesize thread;
@synthesize cond;
@synthesize running;
// for the punknobs context 
@synthesize punknobs;

// ## Private helpers
// stop attached CFRunLoop
- (void) stopHelper
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}

// signal attached CFRunLoop was stopped
- (void) stopNotificationHelper
{
	[cond lock];
	running = false;
	[cond signal];
	[cond unlock];
}

// ## Object methods
- (instancetype) init
{
	running = false;
	self = [super init];

	if (self == nil)
	{
		return nil;
	}

	cond = [NSCondition new];
	return self;
}

- (void) dealloc
{
	[self stop];
	[cond release];
	[super dealloc];
}

- (void) start
{
	// create thread
	thread =
		[[NSThread alloc]
			initWithTarget:self
			selector:@selector(loop:)
			object:nil];

	// start thread
	[thread start];

	// wait for thread to start
	[cond lock];

	while (running == false)
	{
		[cond wait];
	}

	[cond unlock];
}

- (void) stop
{
	// ignore calls when the internal thread is inactive
	if (thread == nil)
	{
		return;
	}

	// prepare notifications
	NSNotificationCenter* notifications =
		[NSNotificationCenter defaultCenter];

	[notifications
		addObserver:self
		selector:@selector(stopNotificationHelper)
		name:NSThreadWillExitNotification
		object:thread];

	// wait for thread to exit
	[cond lock];

	[self
		performSelector:@selector(stopHelper)
		onThread:thread
		withObject:nil
		waitUntilDone:NO];

	while (running == true)
	{
		[cond wait];
	}

	[cond unlock];

	// reset thread
	thread = nil;

	// reset notifications
	[notifications
		removeObserver:self
		name:NSThreadWillExitNotification
		object:thread];

	// release notifications
	[notifications release];
}

- (void) loop: (id)object
{
	// prepare device manager
	IOHIDManagerRef manager =
		IOHIDManagerCreate(
			kCFAllocatorDefault,
			kIOHIDOptionsTypeNone);

	CFDictionaryRef joystickDictionary =
		dictionary(
			kHIDPage_GenericDesktop,
			kHIDUsage_GD_Joystick);

	CFDictionaryRef gamepadDictionary =
		dictionary(
			kHIDPage_GenericDesktop,
			kHIDUsage_GD_GamePad);

	CFDictionaryRef devices[] =
		{
			joystickDictionary,
			gamepadDictionary,
		};

	CFArrayRef matching_array =
		CFArrayCreate(
			kCFAllocatorDefault,
			(const void**) devices,
			2,
			&kCFTypeArrayCallBacks);

	IOHIDManagerSetDeviceMatchingMultiple(
		manager,
		matching_array);

	// register device events callbacks
	IOHIDManagerRegisterDeviceMatchingCallback(
		manager,
		device_added,
		[self punknobs]);

	IOHIDManagerRegisterDeviceRemovalCallback(
		manager,
		device_removed,
		[self punknobs]);

	// register IOHID to CFRunLoop
	IOHIDManagerScheduleWithRunLoop(
		manager,
		CFRunLoopGetCurrent(),
		kCFRunLoopDefaultMode);

	IOHIDManagerOpen(
		manager,
		kIOHIDOptionsTypeNone);

	// confirm thread has started
	[cond lock];
	running = true;
	[cond signal];
	[cond unlock];

	// block while spinning a CFRunLoop
	CFRunLoopRun();

	// unregister device events callbacks
	IOHIDManagerRegisterDeviceMatchingCallback(
		manager,
		NULL,
		NULL);

	IOHIDManagerRegisterDeviceRemovalCallback(
		manager,
		NULL,
		NULL);

	// unregister IOHID from CFRunLoop
    IOHIDManagerUnscheduleFromRunLoop(
		manager,
		CFRunLoopGetCurrent(),
		kCFRunLoopDefaultMode);

    IOHIDManagerClose(
		manager,
		kIOHIDOptionsTypeNone);
}
@end

void macos_helper_input(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDValueRef value)
{
	struct punknobs* context = punknobs;
	struct punknobs_backend_macos* backend = context->backend;
	struct punknobs_error_info error;

	IOHIDElementRef element = IOHIDValueGetElement(value);
	IOHIDElementType type = IOHIDElementGetType(element);

	switch (type)
	{
		case kIOHIDElementTypeInput_Button:
		case kIOHIDElementTypeInput_Axis:
		case kIOHIDElementTypeInput_ScanCodes:
		case kIOHIDElementTypeInput_Misc:
		{
			struct macos_input_info out =
			{
				.punknobs_id = (intptr_t) IOHIDElementGetDevice(element),
				.input_value = value,
			};

			// execute callback
			punknobs->inputs_callback(
				punknobs->inputs_custom_data,
				&out,
				&error);

			if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
			{
				return;
			}

			break;
		}
		case kIOHIDElementTypeOutput:
		case kIOHIDElementTypeFeature:
		case kIOHIDElementTypeCollection:
		case kIOHIDElementTypeInput_NULL:
		default:
		{
			// ignore
			break;
		}
	}

	// all good
	punknobs_error_ok(&error);
}
