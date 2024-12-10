#include "include/punknobs.h"
#include "include/punknobs_evdev_epoll.h"

#include "common/punknobs_private.h"
#include "evdev/evdev_epoll.h"

void punknobs_prepare_init_evdev_epoll(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error)
{
	config->data = NULL;
	config->init = punknobs_evdev_epoll_init;
	config->clean = punknobs_evdev_epoll_clean;
	config->start = punknobs_evdev_epoll_start;
	config->stop = punknobs_evdev_epoll_stop;
	config->register = punknobs_evdev_epoll_register;
	config->unregister = punknobs_evdev_epoll_unregister;
	config->device_get_punknobs_id = punknobs_evdev_epoll_device_get_punknobs_id;
	config->input_get_punknobs_id = punknobs_evdev_epoll_input_get_punknobs_id;

	// TODO

	punknobs_error_ok(error);
}
