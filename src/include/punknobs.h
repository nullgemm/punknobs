#ifndef H_PUNKNOBS
#define H_PUNKNOBS

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// # types
// ## general types
struct punknobs;

enum punknobs_error
{
	// generic
	PUNKNOBS_ERROR_OK = 0,
	PUNKNOBS_ERROR_NULL,
	PUNKNOBS_ERROR_ALLOC,
	PUNKNOBS_ERROR_BOUNDS,
	PUNKNOBS_ERROR_DOMAIN,
	PUNKNOBS_ERROR_FD,
	// evdev
	// TODO
	// win
	// TODO
	// macos
	// TODO
	// special
	PUNKNOBS_ERROR_COUNT,
}

// ## backend configuration structure
// depends on most of the above
struct punknobs_config_backend
{
	// custom data for initialization
	void* data;
	// function pointers for each cross-platform punknobs call
	// lifecycle
	void (*init)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*clean)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*start)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*stop)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	// device registration
	void (*register)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_error_info* error);
	void (*unregister)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_error_info* error);
	void (*reenumerate)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	// device getters
	intptr_t (*device_get_punknobs_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	char* (*device_get_name)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	unsigned (*device_get_vendor_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	unsigned (*device_get_product_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	bool (*device_get_plugged)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	bool (*device_get_registered)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	void* (*device_get_backend_data)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	// input getters
	intptr_t (*input_get_punknobs_id)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	void (*input_get_time)(
		struct punknobs* context,
		void* input_info,
		unsigned* sec,
		unsigned* usec,
		struct punknobs_error_info* error);
	unsigned (*input_get_type)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	unsigned (*input_get_code)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	unsigned (*input_get_value)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	void* (*input_get_backend_data)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
};

// # cross-platform, cross-backend
// ## lifecycle (N.B.: the event loop is always started on a separate thread)
// allocate base resources and make initial checks
struct punknobs* punknobs_init(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);
// free base resources
void punknobs_clean(
	struct punknobs* context,
	struct punknobs_error_info* error);

// start reporting device and input events
void punknobs_start(
	struct punknobs* context,
	struct punknobs_error_info* error);
// stop reporting device and input events
void punknobs_window_stop(
	struct punknobs* context,
	struct punknobs_error_info* error);

// ## device registration (can always be called)
// add device to input watch list
void punknobs_register(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error);
// remove device from input watch list
void punknobs_unregister(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error);
// re-list all plugged-in devices
void punknobs_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error);

// ## device getters
intptr_t punknobs_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

char* punknobs_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

unsigned punknobs_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

unsigned punknobs_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

bool punknobs_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

bool punknobs_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

void* punknobs_device_get_backend_data(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

// ## input getters
intptr_t punknobs_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

void punknobs_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

void* punknobs_input_get_backend_data(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

// ## errors
void punknobs_error_log(
	struct punknobs* context,
	struct punknobs_error_info* error);

const char* punknobs_error_get_msg(
	struct punknobs* context,
	struct punknobs_error_info* error);

enum punknobs_error punknobs_error_get_code(
	struct punknobs_error_info* error);

const char* punknobs_error_get_file(
	struct punknobs_error_info* error);

unsigned punknobs_error_get_line(
	struct punknobs_error_info* error);

void punknobs_error_ok(
	struct punknobs_error_info* error);

#endif
