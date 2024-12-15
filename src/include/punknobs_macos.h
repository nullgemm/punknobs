#ifndef H_PUNKNOBS_MACOS
#define H_PUNKNOBS_MACOS

#include "punknobs.h"

void punknobs_prepare_init_macos(
	struct punknobs_config_backend* config,
	struct punknobs_error_info* error);

unsigned punknobs_macos_input_get_page(
	struct punknobs* context,
	void* input_info,
	struct punknobs_error_info* error);

#endif

