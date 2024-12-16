#ifndef H_PUNKNOBS_WIN
#define H_PUNKNOBS_WIN

#include "punknobs.h"

enum punknobs_win_api
{
	PUNKNOBS_WIN_API_DIRECTINPUT = 0,
	PUNKNOBS_WIN_API_XINPUT,
	PUNKNOBS_WIN_API_COUNT,
};

struct punknobs_win_delays
{
	unsigned delay_device_refresh;
	unsigned delay_input_refresh;
};

void punknobs_prepare_init_win(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);

void punknobs_win_set_delays(
	struct punknobs* context,
	struct punknobs_win_delays* delays,
	struct punknobs_error_info* error);

enum punknobs_win_api punknobs_win_device_get_api(
	struct punknobs* context,
	void* device_info,
	struct punknobs_error_info* error);

enum punknobs_win_api punknobs_win_input_get_api(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

#endif
