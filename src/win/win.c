#include "include/punknobs.h"
#include "include/punknobs_win.h"

#include "common/punknobs_private.h"
#include "win/win.h"

void punknobs_prepare_init_win(
	struct punknobs_config_backend* config,
	struct punknobs_config_win* data,
	struct punknobs_error_info* error)
{
	config->data = data;

	config->init = punknobs_win_init;
	config->clean = punknobs_win_clean;
	config->start = punknobs_win_start;
	config->stop = punknobs_win_stop;
	config->register_add = punknobs_win_register_add;
	config->register_del = punknobs_win_register_del;
	config->reenumerate = punknobs_win_reenumerate;

	config->device_get_punknobs_id = punknobs_win_device_get_punknobs_id;
	config->device_get_name = punknobs_win_device_get_name;
	config->device_get_vendor_id = punknobs_win_device_get_vendor_id;
	config->device_get_product_id = punknobs_win_device_get_product_id;
	config->device_get_plugged = punknobs_win_device_get_plugged;
	config->device_get_registered = punknobs_win_device_get_registered;
	config->device_get_backend_data = punknobs_win_device_get_backend_data;

	config->input_get_punknobs_id = punknobs_win_input_get_punknobs_id;
	config->input_get_time = punknobs_win_input_get_time;
	config->input_get_type = punknobs_win_input_get_type;
	config->input_get_code = punknobs_win_input_get_code;
	config->input_get_value = punknobs_win_input_get_value;
	config->input_get_backend_data = punknobs_win_input_get_backend_data;

	punknobs_error_ok(error);
}
