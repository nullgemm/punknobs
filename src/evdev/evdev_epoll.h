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

struct evdev_epoll_backend
{
};

#endif
