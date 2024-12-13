#ifndef H_PUNKNOBS_WIN
#define H_PUNKNOBS_WIN

#include "punknobs.h"

enum punknobs_win_api
{
	PUNKNOBS_WIN_API_DIRECTINPUT = 0,
	PUNKNOBS_WIN_API_XINPUT,
	PUNKNOBS_WIN_API_COUNT,
};

void punknobs_prepare_init_win(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);

enum punknobs_win_api punknobs_input_get_win_api(
	struct punknobs* context,
	void* backend_info,
	struct punknobs_error_info* error);

#endif
