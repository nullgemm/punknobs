#ifndef H_PUNKNOBS_EVDEV_EPOLL
#define H_PUNKNOBS_EVDEV_EPOLL

#include "punknobs.h"

void punknobs_prepare_init_evdev_epoll(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);

#endif
