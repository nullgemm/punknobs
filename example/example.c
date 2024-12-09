#include "punknobs.h"

#if defined(PUNKNOBS_EXAMPLE_EVDEV_EPOLL)
#include "punknobs_evdev_epoll.h"
#elif defined(PUNKNOBS_EXAMPLE_EVDEV_POLL)
#include "punknobs_evdev_poll.h"
#elif defined(PUNKNOBS_EXAMPLE_WIN)
#include "punknobs_win.h"
#elif defined(PUNKNOBS_EXAMPLE_MACOS)
#include "punknobs_macos.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct callbacks_data
{
	struct punknobs* punknobs;
	size_t ids_max;
	size_t ids_count;
	intptr_t* ids;
};

static void devices_callback(
	void* devices_custom_data,
	void* info,
	struct punknobs_error_info* error)
{

	// all good
	punknobs_error_ok(error);
}

static void inputs_callback(
	void* inputs_custom_data,
	void* info,
	struct punknobs_error_info* error)
{

	// all good
	punknobs_error_ok(error);
}

int main(int argc, char** argv)
{
	struct punknobs_error_info error = {0};
	struct punknobs_error_info error_early = {0};
	printf("starting the common punknobs example\n");

	// prepare function pointers
	struct punknobs_config_backend config = {0};

#if defined(PUNKNOBS_EXAMPLE_EVDEV_EPOLL)
	punknobs_prepare_init_evdev_epoll(&config, &error_early);
#elif defined(PUNKNOBS_EXAMPLE_EVDEV_POLL)
	punknobs_prepare_init_evdev_poll(&config, &error_early);
#elif defined(PUNKNOBS_EXAMPLE_WIN)
	punknobs_prepare_init_win(&config, &error_early);
#elif defined(PUNKNOBS_EXAMPLE_MACOS)
	punknobs_prepare_init_macos(&config, &error_early);
#endif

	// set function pointers and perform basic init
	struct punknobs* punknobs = punknobs_init(&config, &error);

	// Unless the context allocation failed it is always possible to access
	// error messages (even when the context initialization failed) so we can
	// always handle the backend initialization error first.

	// context allocation failed
	if (punknobs == NULL)
	{
		fprintf(stderr, "could not allocate the main punknobs context\n");
		return 1;
	}

	// Backend initialization failed. Since it happens before punknobs
	// initialization and errors are accessible even if it fails, we can handle
	// the errors in the right order regardless.
	if (punknobs_error_get_code(&error_early) != PUNKNOBS_ERROR_OK)
	{
		punknobs_error_log(punknobs, &error_early);
		punknobs_clean(punknobs, &error);
		return 1;
	}

	// The punknobs initialization had failed, make it known now if the backend
	// initialization that happened before went fine.
	if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
	{
		punknobs_error_log(punknobs, &error);
		punknobs_clean(punknobs, &error);
		return 1;
	}

	// TODO

	// free resources correctly
	punknobs_clean(punknobs, &error);

	if (punknobs_error_get_code(&error) != PUNKNOBS_ERROR_OK)
	{
		punknobs_error_log(punknobs, &error);
		return 1;
	}

	// all good
	return 0;
}
