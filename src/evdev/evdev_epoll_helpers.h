#ifndef H_PUNKNOBS_BACKEND_EVDEV_EPOLL_HELPERS
#define H_PUNKNOBS_BACKEND_EVDEV_EPOLL_HELPERS

void mutex_init(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error);

void mutex_clean(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error);

void* callback_devices(void* data);

void* callback_inputs(void* data);

void devices_init(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error)

void devices_init_list(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error);

void devices_clean(
	struct punknobs* context,
	struct evdev_epoll_backend* backend,
	struct punknobs_error_info* error);

#endif
