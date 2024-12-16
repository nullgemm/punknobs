#ifndef H_PUNKNOBS_BACKEND_WIN
#define H_PUNKNOBS_BACKEND_WIN

#include "include/punknobs.h"

#include <stdint.h>
#include <stddef.h>

struct win_device_info
{
	intptr_t punknobs_id;
	char* name;
	char* path;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;
	bool removing;
};

struct win_input_info
{
	intptr_t punknobs_id;
	// TODO
};

struct win_device_node
{
	struct win_device_info info;
	struct win_device* next;
};

struct win_backend
{
	struct punknobs* punknobs;
	bool closed;

	struct win_device_node* devices;
};

#endif
