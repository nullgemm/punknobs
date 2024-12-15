#ifndef H_PUNKNOBS_BACKEND_MACOS_HELPERS
#define H_PUNKNOBS_BACKEND_MACOS_HELPERS

#include <stdbool.h>

#import <Foundation/Foundation.h>
#import <IOHIDDevice.h>
#import <IOHIDManager.h>

// macOS-friendly thread
struct punknobs;

@interface PunknobsThread: NSObject
	// thread and cond
	@property (nonatomic, retain) NSThread* thread;
	@property (nonatomic, retain) NSCondition* cond;
	@property bool running;
	// backend context
	@property struct punknobs* punknobs;

	// object methods
	- (instancetype) init;
	- (void) dealloc;
	- (void) start;
	- (void) stop;
@end

void macos_helper_input(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDValueRef value);

void macos_helper_device(
	void* punknobs,
	IOReturn result,
	void* sender,
	IOHIDDeviceRef device,
	bool plugged);

#endif
