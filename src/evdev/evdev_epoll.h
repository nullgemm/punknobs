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

#define DEVICE_LIST_MULTIPLE 16
#define DEVICE_PENDING_MULTIPLE 8

struct evdev_epoll_device_info
{
	intptr_t punknobs_id;
	char* name;
	char* path;
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

struct evdev_epoll_info
{
	struct epoll_event epoll_event;
	struct libevdev* evdev_context;
	char* device_path;
	int device_fd;

	struct evdev_epoll_info* next;
};

struct evdev_epoll_device
{
	struct evdev_epoll_device_info info;

	struct evdev_epoll_device* next;
};

struct evdev_epoll_thread_data
{
	struct punknobs* punknobs;
	struct evdev_epoll_backend* backend;
};

struct evdev_epoll_backend
{
	struct punknobs* punknobs;
	int timeout;
	bool closed;

	// device and input threads
	struct evdev_epoll_thread_data thread_data;
	pthread_t device_thread;
	pthread_t input_thread;
	pthread_mutex_t mutex_main;

	// device folder watcher
	int pipe_fds_device_loop[2];
	int device_loop_epollfd;

	struct epoll_event device_loop_fds[2];
	struct inotify_event* inotify_update;
	int inotify_fd;
	int inotify_wd;

	// input devices watcher
	int pipe_fds_input_loop[2];
	int input_loop_epollfd;

	struct evdev_epoll_info* input_removed;
	sem_t remove_count;
	struct evdev_epoll_info* input_loop_fds;
	struct evdev_epoll_info* input_loop_last;

	// connected devices
	struct evdev_epoll_device* devices_plugged;

	// pending devices
	char** devices_pending;
	size_t devices_pending_count;
	size_t devices_pending_max;
};

#endif
