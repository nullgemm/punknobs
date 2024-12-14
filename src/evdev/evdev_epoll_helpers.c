#define _XOPEN_SOURCE 700

#include "include/punknobs.h"

#include "common/punknobs_private.h"
#include "evdev/evdev_epoll.h"
#include "evdev/evdev_epoll_helpers.h"

#include <pthread.h>

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
