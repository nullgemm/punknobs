#define _XOPEN_SOURCE 700

#include "include/punknobs.h"

#include "common/punknobs_private.h"
#include "evdev/epoll/evdev_epoll.h"
#include "evdev/epoll/evdev_epoll_helpers.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


// *heavy sigh*
#if defined(_POSIX_MONOTONIC_CLOCK)
#define PUNKNOBS_EVDEV_EPOLL_CLOCK CLOCK_MONOTONIC
#else
#define PUNKNOBS_EVDEV_EPOLL_CLOCK CLOCK_REALTIME
#endif

// attempt at supporting BSDs
#ifndef NAME_MAX
#define NAME_MAX MAXNAMLEN
#endif

// settings for the input descriptors watcher
#define INPUT_EVENT_FILE_PATH "/dev/input"
#define INPUT_EVENT_SIZE ((sizeof (struct inotify_event)) + NAME_MAX + 1)
#define INPUT_EVENT_BUF_SIZE (INPUT_EVENT_SIZE * 15)
#define DEVICE_RETRY_TIMEOUT 2000


// local helpers

static bool is_event_fd(char* file_name)
{
	char* reference = "event";
	size_t len = strlen(reference);

	// can't be valid without enough characters for "event" and a number
	if (strlen(file_name) <= len)
	{
		return false;
	}

	// not starting with "event"
	if (strncmp(file_name, reference, len) != 0)
	{
		return false;
	}

	// not ending with a number
	file_name += len;

	do
	{
		if (isdigit(*file_name) == 0)
		{
			return false;
		}

		++file_name;
	}
	while (*file_name != '\0');

	// should be valid
	return true;
}

static char* get_full_path(char* file_name)
{
	char* file_path = INPUT_EVENT_FILE_PATH "/";
	size_t file_path_len = strlen(file_path);
	size_t file_name_len = strlen(file_name);
	char* path = malloc(file_path_len + file_name_len + 1);

	if (path != NULL)
	{
		memcpy(path, file_path, file_path_len);
		memcpy(path + file_path_len, file_name, file_name_len + 1);
	}

	return path;
}

static void run_device_callback(
	struct punknobs* context,
	char* full_path,
	intptr_t id,
	bool plugged,
	struct punknobs_error_info* error)
{
	struct evdev_epoll_backend* backend = context->backend_context;
	struct evdev_epoll_device_info info;
	struct evdev_epoll_device* device_del;
	int error_posix = 0;

	if (plugged == true)
	{
		// open event device descriptor
		int fd_tmp = open(full_path, O_RDONLY | O_NONBLOCK);

		if (fd_tmp == -1)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD);
			return;
		}

		// create libevdev context
		//
		// This is a one-shot connection to the device we open to get
		// all the information we can about it in order to provide
		// the developer with a complete picture of the gamepad.
		struct libevdev* libevdev_ctx = NULL;

		error_posix = libevdev_new_from_fd(fd_tmp, &libevdev_ctx);

		if (error_posix != 0)
		{
			close(fd_tmp);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEW);
			return;
		}

		// get device info
		const char* hardware_name = libevdev_get_name(libevdev_ctx);

		if (hardware_name == NULL)
		{
			libevdev_free(libevdev_ctx);
			close(fd_tmp);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_GET_HARDWARE_NAME);
			return;
		}

		char* hardware_name_copy = strdup(hardware_name);

		if (hardware_name_copy == NULL)
		{
			libevdev_free(libevdev_ctx);
			close(fd_tmp);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_STRDUP);
			return;
		}

		unsigned id_vendor = libevdev_get_id_vendor(libevdev_ctx);
		unsigned id_product = libevdev_get_id_product(libevdev_ctx);

		// free resources
		libevdev_free(libevdev_ctx);
		close(fd_tmp);

		// fill info
		char* full_path_copy = strdup(full_path);

		if (full_path_copy == NULL)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_STRDUP);
			return;
		}

		struct evdev_epoll_info* new_device =
			malloc(sizeof (struct evdev_epoll_info));

		if (new_device == NULL)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_ALLOC);
			return;
		}

		info.punknobs_id = (intptr_t) new_device;
		info.path = full_path_copy;
		info.name = hardware_name_copy;
		info.vendor_id = id_vendor;
		info.product_id = id_product;
		info.plugged = plugged;
		info.registered = false;
		info.removing = false;

		// allocate new plugged device node
		struct evdev_epoll_device* device_new = malloc(sizeof (struct evdev_epoll_device));

		if (device_new == NULL)
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_ALLOC);
			return;
		}

		// set device info
		device_new->info = info;

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

		// save plugged device
		device_new->next = backend->devices_plugged;
		backend->devices_plugged = device_new;

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
	}
	else
	{
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

		// search for device in plugged list
		device_del = backend->devices_plugged;
		struct evdev_epoll_device* device_prev = device_del;
		struct evdev_epoll_device* device_next = NULL;

		while (device_del != NULL)
		{
			device_next = device_del->next;

			if (device_del->info.punknobs_id == id)
			{
				device_del->info.removing = true;

				info.punknobs_id = id;
				info.path = device_del->info.path;
				info.name = device_del->info.name;
				info.vendor_id = device_del->info.vendor_id;
				info.product_id = device_del->info.product_id;
				info.plugged = plugged;
				info.registered = device_del->info.registered;
				info.removing = device_del->info.removing;

				if (device_prev == device_del)
				{
					backend->devices_plugged = device_next;
				}
				else
				{
					device_prev->next = device_next;
				}

				break;
			}

			device_prev = device_del;
			device_del = device_next;
		}

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
	}

	// call device callback
	//
	// The developer has a chance to ignore the device here, for
	// instance if only a specific kind of controller is to be
	// supported, in which case its inputs won't be reported.
	context->device_callback(context->device_custom_data, &info, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	if (plugged == false)
	{
		free(device_del->info.name);
		free(device_del);
	}

	// all good
	punknobs_error_ok(error);
}

static void add_pending(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	char* full_path,
	struct punknobs_error_info* error)
{
	bool found = false;
	size_t i = 0;

	while (i < backend->devices_pending_count)
	{
		if (strcmp(backend->devices_pending[i], full_path) == 0)
		{
			found = true;
			break;
		}

		++i;
	}

	if (found == false)
	{
		if (backend->devices_pending_count >= backend->devices_pending_max)
		{
			backend->devices_pending_max += DEVICE_PENDING_MULTIPLE;

			backend->devices_pending =
				realloc(
					backend->devices_pending,
					backend->devices_pending_max * (sizeof (char*)));

			if (backend->devices_pending == NULL)
			{
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_ALLOC);
				return;
			}
		}

		backend->timeout = DEVICE_RETRY_TIMEOUT;
		backend->devices_pending[backend->devices_pending_count] = strdup(full_path);
		++(backend->devices_pending_count);
	}

	// all good
	punknobs_error_ok(error);
}

static void device_handle(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	int error_posix = 0;
	ssize_t read_len = -1;
	char* event_buf = malloc(INPUT_EVENT_BUF_SIZE);

	if (event_buf == NULL)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_ALLOC);
		return;
	}

	// # loop over new devices
	read_len = read(backend->inotify_fd, event_buf, INPUT_EVENT_BUF_SIZE);

	// interrupted by signal or no data - not an error
	if ((read_len == -1) && ((errno == EINTR) || (errno == EAGAIN)))
	{
		free(event_buf);
		punknobs_error_ok(error);
		return;
	}

	// actual read error
	if (read_len == -1)
	{
		free(event_buf);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_READ);
		return;
	}

	// loop over inotify events
	struct inotify_event* event;
	char* event_cur = event_buf;
	char* event_max = event_buf + read_len;

	while (event_cur < event_max)
	{
		event = (struct inotify_event*) event_cur;

		// skip invalid events
		// skip events describing files that are not valid input devices
		if ((event->len <= 1) || (is_event_fd(event->name) == false))
		{
			// next event
			event_cur += (sizeof (struct inotify_event)) + event->len;
			continue;
		}

		backend->inotify_update = event;

		// # synchronize with the input loop
		// lock main mutex
		error_posix = pthread_mutex_lock(&(backend->mutex_main));

		if (error_posix != 0)
		{
			free(event_buf);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
			return;
		}

		// get full path
		struct inotify_event* event = backend->inotify_update;
		char* full_path = get_full_path(event->name);

		if (full_path == NULL)
		{
			pthread_mutex_unlock(&(backend->mutex_main));
			free(event_buf);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_ALLOC);
			return;
		}

		// skip events about devices about to be removed
		struct evdev_epoll_device* device_plugged = backend->devices_plugged;

		while (device_plugged != NULL)
		{
			if (strcmp(device_plugged->info.path, full_path) == 0)
			{
				break;
			}

			device_plugged = device_plugged->next;
		}

		if ((device_plugged != NULL) && (device_plugged->info.removing == true))
		{
			// next event
			event_cur += (sizeof (struct inotify_event)) + event->len;

			// unlock main mutex
			error_posix = pthread_mutex_unlock(&(backend->mutex_main));

			if (error_posix != 0)
			{
				free(event_buf);
				punknobs_error_throw(
					context,
					error,
					PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
				return;
			}

			continue;
		}

		// handle events
		intptr_t id = (intptr_t) NULL;
		bool plugged = false;
		bool skip = false;

		if ((event->mask & IN_CREATE) != 0)
		{
			id = (intptr_t) NULL;
			plugged = true;
		}
		else if ((event->mask & IN_ATTRIB) != 0)
		{
			bool found = false;

			// find id for given device fd
			struct evdev_epoll_info* input_loop_fds =
				backend->input_loop_fds->next;

			while (input_loop_fds != NULL)
			{
				if (strcmp(input_loop_fds->device_path, full_path) == 0)
				{
					found = true;
					break;
				}

				input_loop_fds = input_loop_fds->next;
			}

			if (found == false)
			{
				size_t i = 0;

				while (i < backend->devices_pending_count)
				{
					if (strcmp(backend->devices_pending[i], full_path) == 0)
					{
						found = true;
						break;
					}

					++i;
				}

				// unlock main mutex
				error_posix = pthread_mutex_unlock(&(backend->mutex_main));

				if (error_posix != 0)
				{
					free(event_buf);
					punknobs_error_throw(
						context,
						error,
						PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
					return;
				}

				// try running callback
				run_device_callback(context, full_path, (intptr_t) NULL, true, error);

				// lock main mutex
				error_posix = pthread_mutex_lock(&(backend->mutex_main));

				if (error_posix != 0)
				{
					free(event_buf);
					punknobs_error_throw(
						context,
						error,
						PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
					return;
				}

				// handle callback error, adding path as pending on fail
				if (punknobs_error_get_code(error) == PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD)
				{
					// already pending, simply reset timeout
					if (found == true)
					{
						backend->timeout = DEVICE_RETRY_TIMEOUT;
						punknobs_error_ok(error);
					}
					else
					{
						add_pending(context, backend, full_path, error);

						if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
						{
							free(event_buf);
							return;
						}
					}
				}
				else if (punknobs_error_get_code(error) == PUNKNOBS_ERROR_OK)
				{
					// succesfully watching an already pending device, remove it from list
					if (found == true)
					{
						free(backend->devices_pending[i]);
						backend->devices_pending_count -= 1;
						backend->devices_pending[i] = backend->devices_pending[backend->devices_pending_count];
					}
				}
			}

			skip = true;
		}
		else if ((event->mask & IN_DELETE) != 0)
		{
			// find id for given device fd
			struct evdev_epoll_info* input_loop_fds =
				backend->input_loop_fds->next;

			while (input_loop_fds != NULL)
			{
				if (strcmp(input_loop_fds->device_path, full_path) == 0)
				{
					break;
				}

				input_loop_fds = input_loop_fds->next;
			}

			id = (intptr_t) input_loop_fds;
			plugged = false;

			if (input_loop_fds == NULL)
			{
				skip = true;
			}
		}
		else
		{
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_EVENT_UNKNOWN);
		}

		// unlock main mutex
		error_posix = pthread_mutex_unlock(&(backend->mutex_main));

		if (error_posix != 0)
		{
			free(event_buf);
			punknobs_error_throw(
				context,
				error,
				PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
			return;
		}

		// handle errors from in between mutex lock/unlock above
		if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
		{
			free(event_buf);
			return;
		}

		// call user callback
		if (skip == false)
		{
			run_device_callback(context, full_path, id, plugged, error);
		}

		free(full_path);

		if (((skip != false)
		|| (plugged != true)
		|| (punknobs_error_get_code(error) != PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD))
		&& (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK))
		{
			free(event_buf);
			return;
		}

		if (punknobs_error_get_code(error) == PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD)
		{
			punknobs_error_ok(error);
		}

		// next event
		event_cur += (sizeof (struct inotify_event)) + event->len;
	}

	// all good
	free(event_buf);
	punknobs_error_ok(error);
}

static void handle_event(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct input_event* event,
	struct evdev_epoll_info* info,
	struct punknobs_error_info* error)
{
	// call user callback
	struct evdev_epoll_input_info out =
	{
		.punknobs_id = (intptr_t) info->epoll_event.data.ptr,
		.input_event = event,
	};

	context->inputs_callback(context->inputs_custom_data, &out, error);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// all good
	punknobs_error_ok(error);
}

static void modal_sync(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct input_event* event,
	struct evdev_epoll_info* info,
	struct punknobs_error_info* error)
{
	punknobs_error_ok(error);
	int error_posix = 0;

	do
	{
		error_posix =
			libevdev_next_event(
				info->evdev_context,
				LIBEVDEV_READ_FLAG_NORMAL,
				event);

		if (error_posix == LIBEVDEV_READ_STATUS_SYNC)
		{
			handle_event(context, backend, event, info, error);
		}
		else
		{
			// end of the sync stream
			break;
		}
	}
	while (punknobs_error_get_code(error) == PUNKNOBS_ERROR_OK);

	if (punknobs_error_get_code(error) != PUNKNOBS_ERROR_OK)
	{
		return;
	}

	// all good
	punknobs_error_ok(error);
}


// main helpers

void mutex_init(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	int error_posix = 0;
	pthread_mutexattr_t mutex_attr;

	// init pthread mutex attributes
	error_posix = pthread_mutexattr_init(&mutex_attr);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_INIT);
		return;
	}

	// set pthread mutex type (error checking for now)
	error_posix =
		pthread_mutexattr_settype(
			&mutex_attr,
			PTHREAD_MUTEX_ERRORCHECK);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_SETTYPE);
		return;
	}

	// init main pthread mutex
	error_posix =
		pthread_mutex_init(
			&(backend->mutex_main),
			&mutex_attr);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_INIT);
		return;
	}

	// destroy pthread mutex attributes
	error_posix = pthread_mutexattr_destroy(&mutex_attr);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_DESTROY);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void mutex_clean(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	int error_posix = 0;

	// destroy main pthread mutex
	error_posix = pthread_mutex_destroy(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_MUTEX_DESTROY);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void* callback_devices(void* data)
{
	struct evdev_epoll_thread_data* thread_data = data;
	struct punknobs* context = thread_data->punknobs;
	struct evdev_epoll_backend* backend = thread_data->backend;

	struct punknobs_error_info error;
	punknobs_error_ok(&error);

	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		pthread_exit(NULL);
		return NULL;
	}

	bool closed = backend->closed;

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		pthread_exit(NULL);
		return NULL;
	}

	// loop as long as the library is active
	int polled = -1;
	struct epoll_event events[DEVICE_LIST_MULTIPLE];
	struct timespec time = {0};
	int64_t time_current = 0;
	int64_t time_last = 0;
	int64_t time_rem = 0;
	int time_diff = 0;

	while (closed == false)
	{
		clock_gettime(PUNKNOBS_EVDEV_EPOLL_CLOCK, &time);
		time_current = time.tv_nsec;

		// Time_last can't be used uninitialized because timeout
		// is guaranteed to be -1 on the first loop, so it's ok.
		if (backend->timeout != -1)
		{
			time_diff = ((time_current - time_last) + time_rem) / 1000000;
			time_rem = (time_current - time_last) % 1000000;

			if (backend->timeout > time_diff)
			{
				backend->timeout -= time_diff;
			}
			else
			{
				backend->timeout = 0;
				time_rem = 0;
			}
		}

		// device refresh timer elapsed, retry immediately
		time_last = time_current;

		// poll event device fds
		polled =
			epoll_wait(
				backend->device_loop_epollfd,
				events,
				DEVICE_LIST_MULTIPLE,
				backend->timeout);

		if ((polled < 0) && (errno != EINTR))
		{
			punknobs_error_throw(
				context,
				&error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_POLL);
			pthread_exit(NULL);
			return NULL;
		}

		// loop over pending devices on timeout
		if ((polled == 0) || (backend->timeout == 0))
		{
			backend->timeout = -1;
			size_t i = 0;

			while (i < backend->devices_pending_count)
			{
				// For now only devices newly plugged in are added to the pending list
				// so we can simply use "true" as the value for the "plugged" param.
				run_device_callback(
					context,
					backend->devices_pending[i],
					(intptr_t) NULL,
					true,
					&error);

				enum punknobs_error code = punknobs_error_get_code(&error);

				if ((code != PUNKNOBS_ERROR_OK)
				&& (code != PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD))
				{
					pthread_exit(NULL);
					return NULL;
				}

				punknobs_error_ok(&error);
				free(backend->devices_pending[i]);

				++i;
			}

			backend->devices_pending_count = 0;
		}

		// process events
		void* info;

		for (int i = 0; i < polled; ++i)
		{
			info = events[i].data.ptr;

			if (info == &(backend->device_loop_fds[0]))
			{
				// process self-pipe events
				char buf;
				ssize_t pipe_error = read(backend->pipe_fds_device_loop[0], &buf, 1);

				if (pipe_error == 1)
				{
					// purge self-pipe
					while (read(backend->pipe_fds_device_loop[0], &buf, 1) > 0);

					// lock main mutex
					error_posix = pthread_mutex_lock(&(backend->mutex_main));

					if (error_posix != 0)
					{
						punknobs_error_throw(
							context,
							&error,
							PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
						pthread_exit(NULL);
						return NULL;
					}

					closed = backend->closed;

					// unlock main mutex
					error_posix = pthread_mutex_unlock(&(backend->mutex_main));

					if (error_posix != 0)
					{
						punknobs_error_throw(
							context,
							&error,
							PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
						pthread_exit(NULL);
						return NULL;
					}

					if (closed == false)
					{
						if (buf == 0)
						{
							error_posix = sem_trywait(&(backend->remove_count));

							if (error_posix != 0)
							{
								punknobs_error_throw(
									context,
									&error,
									PUNKNOBS_ERROR_POSIX_SEMAPHORE_WAIT);
								pthread_exit(NULL);
								return NULL;
							}

							int remove_count;
							error_posix = sem_getvalue(&(backend->remove_count), &remove_count);

							if (error_posix != 0)
							{
								punknobs_error_throw(
									context,
									&error,
									PUNKNOBS_ERROR_POSIX_SEMAPHORE_GET);
								pthread_exit(NULL);
								return NULL;
							}

							// we can now continue device deletion
							if (remove_count == 0)
							{
								// actually remove all shit
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
											&error,
											PUNKNOBS_ERROR_POSIX_CLOSE);
										pthread_exit(NULL);
										return NULL;
									}

									next = input_removed->next;
									free(input_removed);
									input_removed = next;
								}

								backend->input_removed = NULL;
							}
							else
							{
								char pipe_msg = 0;
								write(backend->pipe_fds_device_loop[1], &pipe_msg, 1);
							}
						}
						else if (buf == 1)
						{
							// lock main mutex
							error_posix = pthread_mutex_lock(&(backend->mutex_main));

							if (error_posix != 0)
							{
								punknobs_error_throw(
									context,
									&error,
									PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
								pthread_exit(NULL);
								return NULL;
							}

							// call device callback again for all plugged devices
							struct evdev_epoll_device* device_plugged = backend->devices_plugged;

							while (device_plugged != NULL)
							{
								context->device_callback(context->device_custom_data, &(device_plugged->info), &error);

								if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
								{
									pthread_exit(NULL);
									return NULL;
								}

								device_plugged = device_plugged->next;
							}

							// unlock main mutex
							error_posix = pthread_mutex_unlock(&(backend->mutex_main));

							if (error_posix != 0)
							{
								punknobs_error_throw(
									context,
									&error,
									PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
								pthread_exit(NULL);
								return NULL;
							}
						}
					}
				}
			}
			else
			{
				// handle devices
				device_handle(context, backend, &error);

				if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
				{
					pthread_exit(NULL);
					return NULL;
				}
			}
		}
	}

	pthread_exit(NULL);
	return NULL;
}

void* callback_inputs(void* data)
{
	struct evdev_epoll_thread_data* thread_data = data;
	struct punknobs* context = thread_data->punknobs;
	struct evdev_epoll_backend* backend = thread_data->backend;

	struct punknobs_error_info error;
	punknobs_error_ok(&error);

	int error_posix = 0;

	// lock main mutex
	error_posix = pthread_mutex_lock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
		pthread_exit(NULL);
		return NULL;
	}

	bool closed = backend->closed;

	// unlock main mutex
	error_posix = pthread_mutex_unlock(&(backend->mutex_main));

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			&error,
			PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
		pthread_exit(NULL);
		return NULL;
	}

	// loop as long as the library is active
	int polled = -1;
	struct epoll_event events[DEVICE_LIST_MULTIPLE];

	while (closed == false)
	{
		// poll event device fds
		polled =
			epoll_wait(
				backend->input_loop_epollfd,
				events,
				DEVICE_LIST_MULTIPLE,
				-1);

		if ((polled < 0) && (errno != EINTR))
		{
			punknobs_error_throw(
				context,
				&error,
				PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_POLL);
			pthread_exit(NULL);
			return NULL;
		}

		// process events
		struct input_event event;
		struct evdev_epoll_info* info;

		for (int i = 0; i < polled; ++i)
		{
			info = events[i].data.ptr;

			if (info == &(backend->input_loop_fds[0]))
			{
				// process self-pipe events
				char buf;

				ssize_t pipe_error =
					read(
						backend->pipe_fds_input_loop[0],
						&buf,
						1);

				if (pipe_error == 1)
				{
					// purge self-pipe
					while (read(backend->pipe_fds_input_loop[0], &buf, 1) > 0);

					// lock main mutex
					error_posix = pthread_mutex_lock(&(backend->mutex_main));

					if (error_posix != 0)
					{
						punknobs_error_throw(
							context,
							&error,
							PUNKNOBS_ERROR_POSIX_MUTEX_LOCK);
						pthread_exit(NULL);
						return NULL;
					}

					closed = backend->closed;

					// unlock main mutex
					error_posix = pthread_mutex_unlock(&(backend->mutex_main));

					if (error_posix != 0)
					{
						punknobs_error_throw(
							context,
							&error,
							PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK);
						pthread_exit(NULL);
						return NULL;
					}

					// tell the device loop it con continue deletion
					if (closed == false)
					{
						char pipe_msg = 0;
						write(backend->pipe_fds_device_loop[1], &pipe_msg, 1);
					}
				}
			}
			else
			{
				error_posix =
					libevdev_next_event(
						info->evdev_context,
						LIBEVDEV_READ_FLAG_NORMAL,
						&event);

				if (error_posix == LIBEVDEV_READ_STATUS_SUCCESS)
				{
					handle_event(context, backend, &event, info, &error);
				}
				else if (error_posix == LIBEVDEV_READ_STATUS_SYNC)
				{
					modal_sync(context, backend, &event, info, &error);
				}
				else if (error_posix == -EAGAIN)
				{
					punknobs_error_ok(&error);
				}
				else if (error_posix == -ENODEV)
				{
					// apparently this can happen...
					punknobs_error_ok(&error);
					break;
				}
				else
				{
					punknobs_error_throw(
						context,
						&error,
						PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEXT_EVENT);
				}

				if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
				{
					pthread_exit(NULL);
					return NULL;
				}
			}
		}
	}

	pthread_exit(NULL);
	return NULL;
}

void devices_init(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	// initialize inotify
	backend->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (backend->inotify_fd == -1)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_INIT);
		return;
	}

	// work around udev setting up device permissions too late by making extra
	// attempts on permission changes: https://stackoverflow.com/a/25672039
	backend->inotify_wd =
		inotify_add_watch(
			backend->inotify_fd,
			INPUT_EVENT_FILE_PATH,
			IN_CREATE | IN_DELETE | IN_ATTRIB);

	if (backend->inotify_wd == -1)
	{
		close(backend->inotify_fd);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_ADD);
		return;
	}

	// add inotify's fd to those we'll be watching in the device callback
	backend->device_loop_fds[1].data.ptr = &(backend->device_loop_fds[1]);
	backend->device_loop_fds[1].events = EPOLLIN;

	int error_posix =
		epoll_ctl(
			backend->device_loop_epollfd,
			EPOLL_CTL_ADD,
			backend->inotify_fd,
			&(backend->device_loop_fds[1]));

	if (error_posix != 0)
	{
		inotify_rm_watch(backend->inotify_fd, backend->inotify_wd);
		close(backend->inotify_fd);
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD);
		return;
	}

	// all good
	punknobs_error_ok(error);
}

void devices_init_list(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	// add initially connected devices to the list
	DIR* dir_root = opendir(INPUT_EVENT_FILE_PATH);

	if (dir_root == NULL)
	{
		// leave the inotify setup intact in case of failure here
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPENDIR_ROOT);
		return;
	}

	struct dirent* dir_info = readdir(dir_root);

	while (dir_info != NULL)
	{
		// listen to valid descriptors 
		if (is_event_fd(dir_info->d_name) == true)
		{
			// get full file path
			char* full_path = get_full_path(dir_info->d_name);

			if (full_path == NULL)
			{
				// leave the inotify setup intact in case of failure here
				closedir(dir_root);
				punknobs_error_throw(context, error, PUNKNOBS_ERROR_ALLOC);
				return;
			}

			// execute the device callback
			run_device_callback(context, full_path, (intptr_t) NULL, true, error);
			free(full_path);

			// keep trying when it fails
		}

		// always try all listed files
		dir_info = readdir(dir_root);
	}

	closedir(dir_root);

	// all good
	punknobs_error_ok(error);
}

void devices_clean(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)
{
	int error_posix = 0;

	// remove inotify watch on input folder
	error_posix = inotify_rm_watch(backend->inotify_fd, backend->inotify_wd);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_DEL);
		return;
	}

	// clean inotify
	error_posix = close(backend->inotify_fd);

	if (error_posix != 0)
	{
		punknobs_error_throw(
			context,
			error,
			PUNKNOBS_ERROR_POSIX_CLOSE);
		return;
	}

	// all good
	punknobs_error_ok(error);
}
