#ifndef H_PUNKNOBS_BACKEND_MACOS
#define H_PUNKNOBS_BACKEND_MACOS

#include "include/punknobs.h"

struct macos_device_info
{
	intptr_t punknobs_id;
	char* manufacturer_name;
	char* product_name;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;
};

struct macos_input_info
{
	intptr_t punknobs_id;
	IOHIDValueRef input_value;
};

struct macos_device_node
{
	struct macos_device_info info;
	struct macos_device* next;
};

struct macos_thread_data
{
	struct punknobs* punknobs;
	struct macos_backend* backend;
};

struct macos_backend
{
	struct punknobs* punknobs;
	struct macos_device_node* devices;
};

#endif
