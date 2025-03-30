#ifndef H_PUNKNOBS_BACKEND_WIN
#define H_PUNKNOBS_BACKEND_WIN

#include "include/punknobs.h"
#include "include/punknobs_win.h"

#include <dinput.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xinput.h>
#include <windows.h>

#define PUNKNOBS_DIRECTINPUT_MAX_SLOT 12

union win_device_enum_node
{
	struct win_device_enum_node_dinput* dinput;
	struct win_device_enum_node_xinput* xinput;
};

// events info
struct win_device_info
{
	char* name;
	unsigned vendor_id;
	unsigned product_id;
	bool plugged;
	bool registered;

	enum punknobs_win_api api;
	union win_device_enum_node device_enum_node;
};

struct win_input_info
{
	DWORD time;
	unsigned short type;
	unsigned short code;
	unsigned int value;

	enum punknobs_win_api api;
	union win_device_enum_node device_enum_node;
};

// enum nodes
struct win_device_enum_node_xinput
{
	DWORD id;
	struct win_device_reg_node_xinput* reg_entry;
	struct win_device_enum_node_xinput* next;
	XINPUT_VIBRATION effects[PUNKNOBS_DIRECTINPUT_MAX_SLOT];
	struct win_device_info info;
};

struct win_device_enum_node_dinput
{
	GUID guid;
	IDirectInputDevice8* device;
	struct win_device_reg_node_dinput* reg_entry;
	struct win_device_enum_node_dinput* next;
	LPDIRECTINPUTEFFECT effects[PUNKNOBS_DIRECTINPUT_MAX_SLOT];
	struct win_device_info info;
	bool xinput_compatible;
};

// reg nodes
struct win_device_reg_node_xinput
{
	XINPUT_STATE state;
	struct win_device_enum_node_xinput* enum_entry;
	struct win_device_reg_node_xinput* next;
};

struct win_device_reg_node_dinput
{
	DIJOYSTATE state;
	struct win_device_enum_node_dinput* enum_entry;
	struct win_device_reg_node_dinput* next;
};

// threads
struct win_thread_device_loop_data
{
	struct win_backend* context;
	struct punknobs_error_info* error;
};

struct win_thread_input_loop_data
{
	struct win_backend* context;
	struct punknobs_error_info* error;
};

// backend context
struct win_backend
{
	struct punknobs* punknobs;
	struct punknobs_win_delays delays;
	bool closed;

	IDirectInput8* dinput;
	HINSTANCE win_module;
	HANDLE reenumeration_handler;

	// new list of dinput enumerated devices
	struct win_device_enum_node_dinput* new_enum_devices_dinput;
	// reference list of dinput enumerated devices
	struct win_device_enum_node_dinput* ref_enum_devices_dinput;
	// list of registered dinput devices
	struct win_device_reg_node_dinput* reg_devices_dinput;

	// new list of xinput enumerated devices
	struct win_device_enum_node_xinput* new_enum_devices_xinput;
	// reference list of xinput enumerated devices
	struct win_device_enum_node_xinput* ref_enum_devices_xinput;
	// list of registered xinput devices
	struct win_device_reg_node_xinput* reg_devices_xinput;

	// mutexes
	HANDLE mutex_main;
	HANDLE mutex_enum;
	HANDLE mutex_reg;

	// device handling
	HANDLE thread_device;
	struct win_thread_device_loop_data thread_device_loop_data;

	// event handling
	HANDLE thread_input;
	struct win_thread_input_loop_data thread_input_loop_data;
};

#endif
