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
		// TODO
		struct punknobs_error_info* error);
	void (*unregister)(
		struct punknobs* context,
		// TODO
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
	// TODO
	struct punknobs_error_info* error);
// remove device from input watch list
void punknobs_unregister(
	struct punknobs* context,
	// TODO
	struct punknobs_error_info* error);

// ## device getters
// TODO

// ## input getters
// TODO

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
