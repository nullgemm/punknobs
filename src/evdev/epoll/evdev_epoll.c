#define _XOPEN_SOURCE 700

#include "include/punknobs.h"
#include "include/punknobs_evdev_epoll.h"

#include "common/punknobs_private.h"
#include "evdev/epoll/evdev_epoll.h"
#include "evdev/epoll/evdev_epoll_helpers.h"

#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>
#include <pthread.h>

// main API
void punknobs_evdev_epoll_init(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	int error_posix = 0;

	// allocate the backend
	struct evdev_epoll_backend* backend = malloc(sizeof (struct evdev_epoll_backend));

	if (backend == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// zero-initialize the backend
	struct evdev_epoll_backend zero = {0};
	*backend = zero;

	// reference the backend in the main context
	context->backend_context = backend;

	// initialize everything with default values
	backend->punknobs = context;
	backend->timeout = -1;
	backend->closed = false;

	backend->device_loop_epollfd = -1;
	backend->inotify_update = NULL;
	backend->inotify_fd = -1;
	backend->inotify_wd = -1;

	backend->input_loop_epollfd = -1;
	backend->input_removed = NULL;
	backend->input_loop_fds = NULL;
	backend->input_loop_last = NULL;

	backend->devices_plugged = NULL;
	backend->devices_pending = NULL;
	backend->devices_pending_count = 0;
	backend->devices_pending_max = 0;

	// create pending devices array
	backend->devices_pending_count = 0;
	backend->devices_pending_max = DEVICE_PENDING_MULTIPLE;

	backend->devices_pending =
		malloc(backend->devices_pending_max * (sizeof (char*)));

	if (backend->devices_pending == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// create a self-pipe to be able to interrupt polling in the input loop
	error_posix = pipe(backend->pipe_fds_input_loop);

	if (error_posix != 0)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_POSIX_PIPE_CREATE);
		return;
	}

	// set non-block on pipe fds
	int flags_input_loop = fcntl(backend->pipe_fds_input_loop[0], F_GETFL);

	if (flags_input_loop == -1)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_POSIX_FCNTL);
		return;
	}

	error_posix =
		fcntl(
			backend->pipe_fds_input_loop[0],
			F_SETFL,
			flags_input_loop | O_NONBLOCK);

	if (error_posix == -1)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_POSIX_FCNTL);
		return;
	}

	// create input epoll instances
	backend->input_loop_epollfd = epoll_create(DEVICE_LIST_MULTIPLE);

	if (backend->input_loop_epollfd == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_CREATE);
		return;
	}

	// allocate input info linked list first item structure
	backend->input_loop_fds =
		malloc(sizeof (struct evdev_epoll_info));

	if (backend->input_loop_fds == NULL)
	{
		punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// save pipe fd in watched input devices fds
	backend->input_loop_fds->epoll_event.data.ptr = backend->input_loop_fds;
	backend->input_loop_fds->epoll_event.events = EPOLLIN;
	backend->input_loop_fds->evdev_context = NULL;
	backend->input_loop_fds->device_path = NULL;
	backend->input_loop_fds->device_fd = backend->pipe_fds_input_loop[0];
	backend->input_loop_fds->next = NULL;
	backend->input_loop_last = backend->input_loop_fds;

	error_posix =
		epoll_ctl(
			backend->input_loop_epollfd,
			EPOLL_CTL_ADD,
			backend->input_loop_fds->device_fd,
			&(backend->input_loop_fds->epoll_event));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD);
		return;
	}

	// create a self-pipe to be able to interrupt polling in the device loop
	error_posix = pipe(backend->pipe_fds_device_loop);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_PIPE_CREATE);
		return;
	}

	// set non-block on pipe fds
	int flags_device_loop = fcntl(backend->pipe_fds_device_loop[0], F_GETFL);

	if (flags_device_loop == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_FCNTL);
		return;
	}

	error_posix =
		fcntl(
			backend->pipe_fds_device_loop[0],
			F_SETFL,
			flags_device_loop | O_NONBLOCK);

	if (error_posix == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_FCNTL);
		return;
	}

	// create device epoll instances
	backend->device_loop_epollfd = epoll_create(2);

	if (backend->device_loop_epollfd == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_CREATE);
		return;
	}

	// save pipe fd in watched input devices fds
	backend->device_loop_fds[0].data.ptr = &(backend->device_loop_fds[0]);
	backend->device_loop_fds[0].events = EPOLLIN;

	error_posix =
		epoll_ctl(
			backend->device_loop_epollfd,
			EPOLL_CTL_ADD,
			backend->pipe_fds_device_loop[0],
			&(backend->device_loop_fds[0]));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD);
		return;
	}

	// create pthread mutexes
	mutex_init(context, backend, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// create semaphore
	sem_init(&(backend->remove_count), 0, 0);

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_clean(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	int error_posix = 0;

	// close self-pipe fds
	close(backend->pipe_fds_input_loop[1]);
	close(backend->pipe_fds_input_loop[0]);
	close(backend->pipe_fds_device_loop[1]);
	close(backend->pipe_fds_device_loop[0]);

	// clean device resources
	struct evdev_epoll_info* input_loop_fds = NULL;

	// free pipe fd
	if (backend->input_loop_fds != NULL)
	{
		input_loop_fds = backend->input_loop_fds->next;
		free(backend->input_loop_fds);
	}

	// free input fds
	while (input_loop_fds != NULL)
	{
		libevdev_free(input_loop_fds->evdev_context);
		error_posix = close(input_loop_fds->device_fd);
		free(input_loop_fds->device_path);

		if (error_posix != 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_CLOSE);
			return;
		}

		struct evdev_epoll_info* prev = input_loop_fds;
		input_loop_fds = input_loop_fds->next;
		free(prev);
	}

	// free removed input fds
	struct evdev_epoll_info* next = NULL;
	struct evdev_epoll_info* input_removed = backend->input_removed;

	while (input_removed != NULL)
	{
		libevdev_free(input_removed->evdev_context);
		error_posix = close(input_removed->device_fd);
		free(input_removed->device_path);

		if (error_posix != 0)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_CLOSE);
			return;
		}

		next = input_removed->next;
		free(input_removed);
		input_removed = next;
	}

	// free pending devices
	for (size_t i = 0; i < backend->devices_pending_count; ++i)
	{
		free(backend->devices_pending[i]);
	}

	free(backend->devices_pending);

	// free plugged devices
	struct evdev_epoll_device* device_plugged = backend->devices_plugged;
	struct evdev_epoll_device* device_next = NULL;

	while (device_plugged != NULL)
	{
		device_next = device_plugged->next;
		free(device_plugged->info.name);
		free(device_plugged->info.path);
		free(device_plugged);
		device_plugged = device_next;
	}

	// destroy pthread mutexes
	mutex_clean(context, backend, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// destroy semaphore
	sem_destroy(&(backend->remove_count));

	// free the backend
	free(backend);

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_start(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	int error_posix = 0;

	// prepare initial device list
	devices_init(context, backend, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// create pthread attributes
	pthread_attr_t attr;

	error_posix = pthread_attr_init(&attr);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_ATTR_INIT);
		return;
	}

	error_posix = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_ATTR_JOINABLE);
		return;
	}

	struct evdev_epoll_thread_data thread_data =
	{
		.punknobs = context,
		.backend = backend,
	};

	backend->thread_data = thread_data;

	// start the device loop in a new thread
	error_posix =
		pthread_create(
			&(backend->device_thread),
			&attr,
			callback_devices,
			&(backend->thread_data));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_CREATE);
		return;
	}

	// start the input loop in a new thread
	error_posix =
		pthread_create(
			&(backend->input_thread),
			&attr,
			callback_inputs,
			&(backend->thread_data));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_CREATE);
		return;
	}

	// destroy the attributes
	error_posix = pthread_attr_destroy(&attr);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_ATTR_DESTROY);
		return;
	}

	// prepare initial device list
	devices_init_list(context, backend, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_stop(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
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

	// request that threads close
	backend->closed = true;

	// ping input loop and device loop self-pipes
	char pipe_msg = 0;
	write(backend->pipe_fds_input_loop[1], &pipe_msg, 1);
	write(backend->pipe_fds_device_loop[1], &pipe_msg, 1);

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

	// wait for input thread to end
	error_posix = pthread_join(backend->input_thread, NULL);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_JOIN);
		return;
	}

	// wait for device thread to end
	error_posix = pthread_join(backend->device_thread, NULL);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_THREAD_JOIN);
		return;
	}

	// clean inotify
	devices_clean(context, backend, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
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

	// search for device in the list
	struct evdev_epoll_device* device = backend->devices_plugged;

	while (device != NULL)
	{
		if ((device->info.registered == false)
		&& (device->info.punknobs_id == id))
		{
			break;
		}

		device = device->next;
	}

	// stop here if we could not find the device
	if (device == NULL)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_DEVICE_NOT_FOUND);
		return;
	}

	// watch the device
	struct evdev_epoll_device_info* info = &(device->info);

	// open event device descriptor
	int fd = open(info->path, O_RDONLY | O_NONBLOCK);

	if (fd == -1)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD);
		return;
	}

	// create libevdev context
	struct libevdev* libevdev_ctx = NULL;
	error_posix = libevdev_new_from_fd(fd, &libevdev_ctx);

	if (error_posix != 0)
	{
		close(fd);
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEW);
		return;
	}

	char* path_copy = strdup(info->path);

	if (path_copy == NULL)
	{
		close(fd);
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_STRDUP);
		return;
	}

	// add to watch list
	struct evdev_epoll_info* new_device =
		(struct evdev_epoll_info*) info->punknobs_id;

	// configure epoll
	new_device->epoll_event.data.ptr = new_device;
	new_device->epoll_event.events = EPOLLIN;
	new_device->evdev_context = libevdev_ctx;
	new_device->device_path = path_copy;
	new_device->device_fd = fd;
	new_device->next = backend->input_loop_last->next;
	backend->input_loop_last->next = new_device;
	backend->input_loop_last = new_device;

	error_posix =
		epoll_ctl(
			backend->input_loop_epollfd,
			EPOLL_CTL_ADD,
			fd,
			&(new_device->epoll_event));

	if (error_posix != 0)
	{
		libevdev_free(libevdev_ctx);
		close(fd);
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD);
		return;
	}

	info->registered = true;

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

void punknobs_evdev_epoll_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
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
	struct evdev_epoll_info* prev = backend->input_loop_fds;
	struct evdev_epoll_info* input_loop_fds = backend->input_loop_fds->next;

	while (input_loop_fds != NULL)
	{
		if (((intptr_t) input_loop_fds) == id)
		{
			break;
		}

		prev = input_loop_fds;
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

	error_posix =
		epoll_ctl(
			backend->input_loop_epollfd,
			EPOLL_CTL_DEL,
			input_loop_fds->device_fd,
			NULL);

	if (error_posix != 0)
	{
		pthread_mutex_unlock(&(backend->mutex_main));
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_DEL);
	}

	// save reference to device being removed
	if (input_loop_fds == backend->input_loop_last)
	{
		backend->input_loop_last = prev;
	}

	prev->next = input_loop_fds->next;
	input_loop_fds->next = backend->input_removed;
	backend->input_removed = input_loop_fds;
	sem_post(&(backend->remove_count));

	// search for device in the list
	struct evdev_epoll_device* device = backend->devices_plugged;

	while (device != NULL)
	{
		if ((device->info.registered == true)
		&& (device->info.punknobs_id == id))
		{
			break;
		}

		device = device->next;
	}

	// set device to unregistered
	if (device != NULL)
	{
		device->info.registered = false;
	}

	// signal input loop to have it flush events about the device being removed
	char pipe_msg = 0;
	write(backend->pipe_fds_input_loop[1], &pipe_msg, 1);

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

void punknobs_evdev_epoll_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	char pipe_msg = 1;
	write(backend->pipe_fds_device_loop[1], &pipe_msg, 1);

	// all good
	punknobs_error_ok(error);
}

// haptics management
void punknobs_evdev_epoll_haptics_get_features(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_features* features,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_haptics_get_waveforms(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_waveforms* waveforms,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

int punknobs_evdev_epoll_haptics_effect_set(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_effect* effect,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
	return -1;
}

void punknobs_evdev_epoll_haptics_effect_del(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_haptics_gain_set(
	struct punknobs* context,
	intptr_t id,
	int gain,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_haptics_autocenter_set(
	struct punknobs* context,
	intptr_t id,
	int autocenter,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_haptics_effect_play(
	struct punknobs* context,
	intptr_t id,
	int slot,
	int repeat,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

void punknobs_evdev_epoll_haptics_effect_stop(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;

	// all good
	punknobs_error_ok(error);
}

// device getters
intptr_t punknobs_evdev_epoll_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

char* punknobs_evdev_epoll_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->name;
}

unsigned punknobs_evdev_epoll_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->vendor_id;
}

unsigned punknobs_evdev_epoll_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->product_id;
}

bool punknobs_evdev_epoll_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->plugged;
}

bool punknobs_evdev_epoll_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info* info = device_info;

	punknobs_error_ok(error);
	return info->registered;
}

// input getters
intptr_t punknobs_evdev_epoll_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->punknobs_id;
}

void punknobs_evdev_epoll_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_input_info* info = input_info;

	*sec = info->input_event->input_event_sec;
	*usec = info->input_event->input_event_usec;

	punknobs_error_ok(error);
}

unsigned punknobs_evdev_epoll_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->type;
}

unsigned punknobs_evdev_epoll_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->code;
}

unsigned punknobs_evdev_epoll_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_input_info* info = input_info;

	punknobs_error_ok(error);
	return info->input_event->value;
}

// configurator
void punknobs_prepare_init_evdev_epoll(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	config->data = NULL;

	config->init = punknobs_evdev_epoll_init;
	config->clean = punknobs_evdev_epoll_clean;
	config->start = punknobs_evdev_epoll_start;
	config->stop = punknobs_evdev_epoll_stop;
	config->register_add = punknobs_evdev_epoll_register_add;
	config->register_del = punknobs_evdev_epoll_register_del;
	config->reenumerate = punknobs_evdev_epoll_reenumerate;

	config->haptics_get_features = punknobs_evdev_epoll_haptics_get_features;
	config->haptics_get_waveforms = punknobs_evdev_epoll_haptics_get_waveforms;
	config->haptics_effect_set = punknobs_evdev_epoll_haptics_effect_set;
	config->haptics_effect_del = punknobs_evdev_epoll_haptics_effect_del;
	config->haptics_gain_set = punknobs_evdev_epoll_haptics_gain_set;
	config->haptics_autocenter_set = punknobs_evdev_epoll_haptics_autocenter_set;
	config->haptics_effect_play = punknobs_evdev_epoll_haptics_effect_play;
	config->haptics_effect_stop = punknobs_evdev_epoll_haptics_effect_stop;

	config->device_get_punknobs_id = punknobs_evdev_epoll_device_get_punknobs_id;
	config->device_get_name = punknobs_evdev_epoll_device_get_name;
	config->device_get_vendor_id = punknobs_evdev_epoll_device_get_vendor_id;
	config->device_get_product_id = punknobs_evdev_epoll_device_get_product_id;
	config->device_get_plugged = punknobs_evdev_epoll_device_get_plugged;
	config->device_get_registered = punknobs_evdev_epoll_device_get_registered;

	config->input_get_punknobs_id = punknobs_evdev_epoll_input_get_punknobs_id;
	config->input_get_time = punknobs_evdev_epoll_input_get_time;
	config->input_get_type = punknobs_evdev_epoll_input_get_type;
	config->input_get_code = punknobs_evdev_epoll_input_get_code;
	config->input_get_value = punknobs_evdev_epoll_input_get_value;

	punknobs_error_ok(error);
}
