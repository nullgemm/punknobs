#ifndef H_PUNKNOBS
#define H_PUNKNOBS

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// # types
// ## general types
struct punknobs;

enum punknobs_error
{
	// generic
	PUNKNOBS_ERROR_OK = 0,
	PUNKNOBS_ERROR_NULL,
	PUNKNOBS_ERROR_ALLOC,
	PUNKNOBS_ERROR_BOUNDS,
	PUNKNOBS_ERROR_DOMAIN,
	PUNKNOBS_ERROR_FD,
	// evdev
	PUNKNOBS_ERROR_POSIX_STRDUP,
	PUNKNOBS_ERROR_POSIX_PIPE_CREATE,
	PUNKNOBS_ERROR_POSIX_FCNTL,
	PUNKNOBS_ERROR_POSIX_CLOSE,
	PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_INIT,
	PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_SETTYPE,
	PUNKNOBS_ERROR_POSIX_MUTEX_ATTR_DESTROY,
	PUNKNOBS_ERROR_POSIX_MUTEX_INIT,
	PUNKNOBS_ERROR_POSIX_MUTEX_DESTROY,
	PUNKNOBS_ERROR_POSIX_MUTEX_LOCK,
	PUNKNOBS_ERROR_POSIX_MUTEX_UNLOCK,
	PUNKNOBS_ERROR_POSIX_SEMAPHORE_WAIT,
	PUNKNOBS_ERROR_POSIX_SEMAPHORE_GET,
	PUNKNOBS_ERROR_POSIX_THREAD_ATTR_INIT,
	PUNKNOBS_ERROR_POSIX_THREAD_ATTR_JOINABLE,
	PUNKNOBS_ERROR_POSIX_THREAD_ATTR_DESTROY,
	PUNKNOBS_ERROR_POSIX_THREAD_CREATE,
	PUNKNOBS_ERROR_POSIX_THREAD_JOIN,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_POLL,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_DEVICE_NOT_FOUND,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPEN_EVENTFD,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_OPENDIR_ROOT,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_CREATE,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_ADD,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_EPOLL_DEL,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_INIT,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_ADD,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_DEL,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_READ,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_INOTIFY_EVENT_UNKNOWN,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEW,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_GET_HARDWARE_NAME,
	PUNKNOBS_ERROR_BACKEND_EVDEV_EPOLL_LIBEVDEV_NEXT_EVENT,
	// win
	PUNKNOBS_ERROR_BACKEND_WIN_MODULE_GET,
	PUNKNOBS_ERROR_BACKEND_WIN_DINPUT_GET,
	PUNKNOBS_ERROR_BACKEND_WIN_INVALID_API,
	PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_CREATE,
	PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_DESTROY,
	PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_LOCK,
	PUNKNOBS_ERROR_BACKEND_WIN_MUTEX_UNLOCK,
	PUNKNOBS_ERROR_BACKEND_WIN_EVENT_CREATE,
	PUNKNOBS_ERROR_BACKEND_WIN_EVENT_OPEN,
	PUNKNOBS_ERROR_BACKEND_WIN_EVENT_RESET,
	PUNKNOBS_ERROR_BACKEND_WIN_EVENT_DESTROY,
	PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_START,
	PUNKNOBS_ERROR_BACKEND_WIN_THREAD_DEVICE_CLOSE,
	PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_START,
	PUNKNOBS_ERROR_BACKEND_WIN_THREAD_INPUT_CLOSE,
	// macos
	PUNKNOBS_ERROR_BACKEND_MACOS_CSTRING,
	// special
	PUNKNOBS_ERROR_COUNT,
};

struct punknobs_error_info
{
	enum punknobs_error code;
	const char* file;
	unsigned line;
};

// ## haptics types
// reports types
enum punknobs_haptics_feature
{
	PUNKNOBS_HAPTICS_FEATURE_RUMBLE = 0,
	PUNKNOBS_HAPTICS_FEATURE_PERIODIC,
	PUNKNOBS_HAPTICS_FEATURE_CONSTANT,
	PUNKNOBS_HAPTICS_FEATURE_SPRING,
	PUNKNOBS_HAPTICS_FEATURE_FRICTION,
	PUNKNOBS_HAPTICS_FEATURE_DAMPER,
	PUNKNOBS_HAPTICS_FEATURE_INERTIA,
	PUNKNOBS_HAPTICS_FEATURE_RAMP,
	PUNKNOBS_HAPTICS_FEATURE_GAIN,
	PUNKNOBS_HAPTICS_FEATURE_AUTOCENTER,
	// special
	PUNKNOBS_HAPTICS_FEATURE_COUNT,
};

enum punknobs_haptics_waveform
{
	PUNKNOBS_HAPTICS_WAVEFORM_SQUARE = 0,
	PUNKNOBS_HAPTICS_WAVEFORM_TRIANGLE,
	PUNKNOBS_HAPTICS_WAVEFORM_SINE,
	PUNKNOBS_HAPTICS_WAVEFORM_SAW_UP,
	PUNKNOBS_HAPTICS_WAVEFORM_SAW_DOWN,
	PUNKNOBS_HAPTICS_WAVEFORM_CUSTOM,
	// special
	PUNKNOBS_HAPTICS_WAVEFORM_COUNT,
};

struct punknobs_haptics_features
{
	enum punknobs_haptics_feature* list;
	size_t count;
};

struct punknobs_haptics_waveforms
{
	enum punknobs_haptics_waveform* list;
	size_t count;
};

struct punknobs_haptics_envelope
{
	int attack_length;
	int attack_level;
	int fade_length;
	int fade_level;
};

// effects types
struct punknobs_haptics_effect_constant
{
	int level;
	struct punknobs_haptics_envelope envelope;
};

struct punknobs_haptics_effect_ramp
{
	int start_level;
	int end_level;
	struct punknobs_haptics_envelope envelope;
};

struct punknobs_haptics_effect_periodic
{
	int waveform;
	int period;
	int magnitude;
	int offset;
	int phase;

	struct punknobs_haptics_envelope envelope;

	uint32_t custom_len;
	uint16_t *custom_data;
};

struct punknobs_haptics_effect_condition
{
	int right_saturation;
	int left_saturation;
	int right_coeff;
	int left_coeff;
	int deadband;
	int center;
};

struct punknobs_haptics_effect_rumble
{
	int strong_magnitude;
	int weak_magnitude;
};

union punknobs_haptics_effect_config
{
	struct punknobs_haptics_effect_constant constant;
	struct punknobs_haptics_effect_ramp ramp;
	struct punknobs_haptics_effect_periodic periodic;
	struct punknobs_haptics_effect_condition condition[2];
	struct punknobs_haptics_effect_rumble rumble;
};

struct punknobs_haptics_effect_trigger
{
	int button;
	int interval;
};

struct punknobs_haptics_effect_replay
{
	int length;
	int delay;
};

struct punknobs_haptics_effect
{
	int type;
	int id;
	int direction;

	struct punknobs_haptics_effect_trigger trigger;
	struct punknobs_haptics_effect_replay replay;
	union punknobs_haptics_effect_config config;
};

// ## backend configuration structure
// depends on most of the above
struct punknobs_config_backend
{
	// custom data for initialization
	void* data;
	// function pointers for each cross-platform punknobs call
	// lifecycle
	void (*init)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*clean)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*start)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	void (*stop)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	// device registration
	void (*register_add)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_error_info* error);
	void (*register_del)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_error_info* error);
	void (*reenumerate)(
		struct punknobs* context,
		struct punknobs_error_info* error);
	// haptics management
	void (*haptics_get_features)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_haptics_features* features,
		struct punknobs_error_info* error);
	void (*haptics_get_waveforms)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_haptics_waveforms* waveforms,
		struct punknobs_error_info* error);
	int (*haptics_effect_max)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_error_info* error);
	int (*haptics_effect_set)(
		struct punknobs* context,
		intptr_t id,
		struct punknobs_haptics_effect* effect,
		struct punknobs_error_info* error);
	void (*haptics_effect_del)(
		struct punknobs* context,
		intptr_t id,
		int slot,
		struct punknobs_error_info* error);
	void (*haptics_gain_set)(
		struct punknobs* context,
		intptr_t id,
		int gain,
		struct punknobs_error_info* error);
	void (*haptics_autocenter_set)(
		struct punknobs* context,
		intptr_t id,
		int autocenter,
		struct punknobs_error_info* error);
	void (*haptics_effect_play)(
		struct punknobs* context,
		intptr_t id,
		int slot,
		int repeat,
		struct punknobs_error_info* error);
	void (*haptics_effect_stop)(
		struct punknobs* context,
		intptr_t id,
		int slot,
		struct punknobs_error_info* error);
	// device getters
	intptr_t (*device_get_punknobs_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	char* (*device_get_name)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	unsigned (*device_get_vendor_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	unsigned (*device_get_product_id)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	bool (*device_get_plugged)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	bool (*device_get_registered)(
		struct punknobs* context,
		void* device_info,
		struct punknobs_error_info* error);
	// input getters
	intptr_t (*input_get_punknobs_id)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	void (*input_get_time)(
		struct punknobs* context,
		void* input_info,
		unsigned* sec,
		unsigned* usec,
		struct punknobs_error_info* error);
	unsigned (*input_get_type)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	unsigned (*input_get_code)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
	unsigned (*input_get_value)(
		struct punknobs* context,
		void* input_info,
		struct punknobs_error_info* error);
};

struct punknobs_config_device_callback
{
	void* data;
	void (*handler)(
		void* devices_custom_data,
		void* info,
		struct punknobs_error_info* error);
};

struct punknobs_config_input_callback
{
	void* data;
	void (*handler)(
		void* inputs_custom_data,
		void* info,
		struct punknobs_error_info* error);
};

// # cross-platform, cross-backend
// ## lifecycle (N.B.: the event loop is always started on a separate thread)
// allocate base resources and make initial checks
struct punknobs* punknobs_init(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);
// free base resources
void punknobs_clean(
	struct punknobs* context,
	struct punknobs_error_info* error);

// set device callback
void punknobs_set_device_callback(
	struct punknobs* context,
	struct punknobs_config_device_callback* config_device,
	struct punknobs_error_info* error);
// set input callback
void punknobs_set_input_callback(
	struct punknobs* context,
	struct punknobs_config_input_callback* config_input,
	struct punknobs_error_info* error);

// start reporting device and input events
void punknobs_start(
	struct punknobs* context,
	struct punknobs_error_info* error);
// stop reporting device and input events
void punknobs_stop(
	struct punknobs* context,
	struct punknobs_error_info* error);

// ## device registration (can always be called)
// add device to input watch list
void punknobs_register_add(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error);
// remove device from input watch list
void punknobs_register_del(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error);
// re-list all plugged-in devices
void punknobs_reenumerate(
	struct punknobs* context,
	struct punknobs_error_info* error);

// ## haptics management
// get the haptic features supported by the device
void punknobs_haptics_get_features(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_features* features,
	struct punknobs_error_info* error);

// get the waveforms supported by the periodic effect (if applicable)
void punknobs_haptics_get_waveforms(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_waveforms* waveforms,
	struct punknobs_error_info* error);

// get haptics effect slot count for the target device
int punknobs_haptics_effect_max(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_error_info* error);

// set haptics effect in specified slot or adds it to a free slot
int punknobs_haptics_effect_set(
	struct punknobs* context,
	intptr_t id,
	struct punknobs_haptics_effect* effect,
	struct punknobs_error_info* error);

// removes haptics effect from specified slot
void punknobs_haptics_effect_del(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error);

// set gain value
void punknobs_haptics_gain_set(
	struct punknobs* context,
	intptr_t id,
	int gain,
	struct punknobs_error_info* error);

// set autocenter value
void punknobs_haptics_autocenter_set(
	struct punknobs* context,
	intptr_t id,
	int autocenter,
	struct punknobs_error_info* error);

// play effect in specified slot the given number of times
void punknobs_haptics_effect_play(
	struct punknobs* context,
	intptr_t id,
	int slot,
	int repeat,
	struct punknobs_error_info* error);

// stop effect in specified slot
void punknobs_haptics_effect_stop(
	struct punknobs* context,
	intptr_t id,
	int slot,
	struct punknobs_error_info* error);

// ## device getters
intptr_t punknobs_device_get_punknobs_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

char* punknobs_device_get_name(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

unsigned punknobs_device_get_vendor_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

unsigned punknobs_device_get_product_id(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

bool punknobs_device_get_registered(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

bool punknobs_device_get_plugged(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

// ## input getters
intptr_t punknobs_input_get_punknobs_id(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

void punknobs_input_get_time(
	struct punknobs* context,
	void* input_info,
	unsigned* sec,
	unsigned* usec,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_type(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_code(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

unsigned punknobs_input_get_value(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

// ## errors
void punknobs_error_log(
	struct punknobs* context,
	struct punknobs_error_info* error);

const char* punknobs_error_get_msg(
	struct punknobs* context,
	struct punknobs_error_info* error);

enum punknobs_error punknobs_error_get_code(
	struct punknobs_error_info* error);

const char* punknobs_error_get_file(
	struct punknobs_error_info* error);

unsigned punknobs_error_get_line(
	struct punknobs_error_info* error);

void punknobs_error_ok(
	struct punknobs_error_info* error);

#endif
