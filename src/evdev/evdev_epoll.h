#ifndef H_PUNKNOBS_BACKEND_EVDEV_EPOLL
#define H_PUNKNOBS_BACKEND_EVDEV_EPOLL

#include "include/punknobs.h"

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/epoll.h>

struct evdev_epoll_device_info
{
	intptr_t punknobs_id;
	char* name;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;
};

struct evdev_epoll_input_info
{
	intptr_t punknobs_id;
	struct input_event* input_event;
};

struct evdev_epoll_backend
{
	struct punknobs* punknobs;
	bool closed;
};

#endif
