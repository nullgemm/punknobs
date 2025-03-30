#include "include/punknobs.h"
#include "common/punknobs_private.h"
#include "common/punknobs_error.h"

#include <stdbool.h>

#if defined(PUNKNOBS_ERROR_LOG_MANUAL) || defined(PUNKNOBS_ERROR_LOG_THROW)
	#include <stdio.h>
#endif

#ifdef PUNKNOBS_ERROR_ABORT
	#include <stdlib.h>
#endif

void punknobs_error_init(
	struct punknobs* context)
{
#ifndef PUNKNOBS_ERROR_SKIP
	char** log = context->error_messages;

	log[PUNKNOBS_ERROR_OK] =
		"out-of-bound error message";
	log[PUNKNOBS_ERROR_NULL] =
		"null pointer";
	log[PUNKNOBS_ERROR_ALLOC] =
		"failed malloc";
	log[PUNKNOBS_ERROR_BOUNDS] =
		"out-of-bounds index";
	log[PUNKNOBS_ERROR_DOMAIN] =
		"invalid domain";
	// evdev
	log[PUNKNOBS_ERROR_POSIX_STRDUP] =
		"failed strdup";
	log[PUNKNOBS_ERROR_POSIX_PIPE_CREATE] =
		"failed pipe creation";
	log[PUNKNOBS_ERROR_POSIX_FCNTL] =
		"failed fcntl";
	log[PUNKNOBS_ERROR_POSIX_CLOSE] =
		"failed closing file descriptor";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_INIT] =
		"failed initializing mutex attributes";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_SETTYPE] =
		"failed setting mutex attributes type";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_DESTROY] =
		"failed destroying mutex attributes";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_INIT] =
		"failed initializing mutex";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_DESTROY] =
		"failed destroying mutex";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_LOCK] =
		"failed locking mutex";
	log[PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK] =
		"failed unlocking mutex";
	log[PUNKNOBS_ERROR_POSIX_SEMAPHORE_WAIT] =
		"failed waiting for semaphore";
	log[PUNKNOBS_ERROR_POSIX_SEMAPHORE_GET] =
		"failed getting semaphore value";
	log[PUNKNOBS_ERROR_POSIX_THREAD_ATTR_INIT] =
		"failed initializing thread attributes";
	log[PUNKNOBS_ERROR_POSIX_THREAD_ATTR_JOINABLE] =
		"failed setting joinable thread attribute";
	log[PUNKNOBS_ERROR_POSIX_THREAD_ATTR_DESTROY] =
		"failed destroying thread attributes";
	log[PUNKNOBS_ERROR_POSIX_THREAD_CREATE] =
		"failed creating thread";
	log[PUNKNOBS_ERROR_POSIX_THREAD_JOIN] =
		"failed joining thread";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_POLL] =
		"failed polling";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_DEVICE_NOT_FOUND] =
		"could not find target device";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD] =
		"could not open event file descriptor";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPENDIR_ROOT] =
		"could not open event directory";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_CREATE] =
		"failed creating epoll structure";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD] =
		"failed adding descriptor to epoll";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_DEL] =
		"failed removing descriptor from epoll";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_INIT] =
		"could not initialize inotify";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_ADD] =
		"could not add inotify watch";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_DEL] =
		"could not remove inotify watch";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_READ] =
		"could not read inotify event";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_EVENT_UNKNOWN] =
		"unknown inotify event";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEW] =
		"could not create evdev context";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_GET_HARDWARE_NAME] =
		"could not get device hardware name";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEXT_EVENT] =
		"could not get next evdev event";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_IOCTL_FEATURES] =
		"could not get force feedback features";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_IOCTL_WAVEFORMS] =
		"could not get periodic force feedback waveforms";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_IOCTL_EFFECTS_MAX] =
		"could not get force feedback maximum effects count";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_IOCTL_EFFECTS_DEL] =
		"could not delete force feedback effect";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_GAIN_SET] =
		"could not set force feedback gain";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_AUTOCENTER_SET] =
		"could not set force feedback autocenter";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_PLAY] =
		"could not play force feedback effect";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_STOP] =
		"could not stop force feedback effect";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_TYPE] =
		"invalid force feedback effect type";
	log[PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EFFECT_SET] =
		"could not set force feedback effect";
	// win
	log[PUNKNOBS_ERROR_BACKEND_WIN_MODULE_GET] =
		"could not get application module";
	log[PUNKNOBS_ERROR_BACKEND_WIN_DINPUT_GET] =
		"could not get DirectInput context";
	log[PUNKNOBS_ERROR_BACKEND_WIN_INVALID_API] =
		"invalid input API";
	log[PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_CREATE] =
		"could not create mutex";
	log[PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_DESTROY] =
		"could not destroy mutex";
	log[PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK] =
		"could not lock mutex";
	log[PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK] =
		"could not unlock mutex";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EVENT_CREATE] =
		"could not create event";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EVENT_OPEN] =
		"could not open event";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EVENT_RESET] =
		"could not reset event";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EVENT_DESTROY] =
		"could not destroy event";
	log[PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_START] =
		"could not start device thread";
	log[PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_CLOSE] =
		"could not stop device thread";
	log[PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_START] =
		"could not start input thread";
	log[PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_CLOSE] =
		"could not stop input thread";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_CREATE] =
		"could not create effect";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_SLOT_INVALID] =
		"invalid effect slot";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_STOP] =
		"could not stop effect";
	log[PUNKNOBS_ERROR_BACKEND_WIN_EFFECT_PLAY] =
		"could not start effect";
	// macos
	log[PUNKNOBS_ERROR_BACKEND_MACOS_CSTRING] =
		"could not convert string";
#endif
}

void punknobs_error_log(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
#ifndef PUNKNOBS_ERROR_SKIP
	#ifdef PUNKNOBS_ERROR_LOG_MANUAL
		#ifdef PUNKNOBS_ERROR_LOG_DEBUG
			fprintf(
				stderr,
				"error in %s line %u: ",
				error->file,
				error->line);
		#endif

		if (error->code < PUNKNOBS_ERROR_COUNT)
		{
			fprintf(stderr, "%s\n", context->error_messages[error->code]);
		}
		else
		{
			fprintf(stderr, "%s\n", context->error_messages[0]);
		}
	#endif
#endif
}

const char* punknobs_error_get_msg(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	if (error->code < PUNKNOBS_ERROR_COUNT)
	{
		return context->error_messages[error->code];
	}
	else
	{
		return context->error_messages[0];
	}
}

enum punknobs_error punknobs_error_get_code(
	struct punknobs_error_info* error)
{
	return error->code;
}

const char* punknobs_error_get_file(
	struct punknobs_error_info* error)
{
	return error->file;
}

unsigned punknobs_error_get_line(
	struct punknobs_error_info* error)
{
	return error->line;
}

void punknobs_error_ok(
	struct punknobs_error_info* error)
{
	error->code = PUNKNOBS_ERROR_OK;
	error->file = "";
	error->line = 0;
}

void punknobs_error_throw_extra(
	struct punknobs* context,
	struct punknobs_error_info* error,
	enum punknobs_error code,
	const char* file,
	unsigned line)
{
#ifndef PUNKNOBS_ERROR_SKIP
	error->code = code;
	error->file = file;
	error->line = line;

	#ifdef PUNKNOBS_ERROR_LOG_THROW
		#ifdef PUNKNOBS_ERROR_LOG_DEBUG
			fprintf(
				stderr,
				"error in %s line %u: ",
				file,
				line);
		#endif

		if (error->code < PUNKNOBS_ERROR_COUNT)
		{
			fprintf(stderr, "%s\n", context->error_messages[error->code]);
		}
		else
		{
			fprintf(stderr, "%s\n", context->error_messages[0]);
		}
	#endif

	#ifdef PUNKNOBS_ERROR_ABORT
		abort();
	#endif
#endif
}
