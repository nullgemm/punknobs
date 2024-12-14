#ifndef H_PUNKNOBS_PRIVATE
#define H_PUNKNOBS_PRIVATE

#include "include/punknobs.h"
#include "common/punknobs_error.h"

struct punknobs_device_backend
{
	void* backend_data;
	intptr_t punknobs_id;
	char* name;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;
};

struct punknobs_input_backend
{
	void* backend_data;
	intptr_t punknobs_id;
	unsigned sec;
	unsigned usec;
	unsigned type;
	unsigned code;
	unsigned value;
};

struct punknobs
{
	// device callback
	void* device_custom_data;

	void (*device_callback)(
		void* device_custom_data,
		void* info,
		struct punknobs_error_info* error);

	// input callback
	void* inputs_custom_data;

	void (*inputs_callback)(
		void* inputs_custom_data,
		void* info,
		struct punknobs_error_info* error);

	// backends
	struct punknobs_config_backend backend_config;
	void* backend_context;

	// error handling
	char* error_messages[PUNKNOBS_ERROR_COUNT];
};

#endif
