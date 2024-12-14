#ifndef H_PUNKNOBS_ERROR
#define H_PUNKNOBS_ERROR

#include "include/punknobs.h"

#define punknobs_error_throw(context, error, code) \
	punknobs_error_throw_extra(\
		context,\
		error,\
		code,\
		PUNKNOBS_ERROR_FILE,\
		PUNKNOBS_ERROR_LINE)
#define PUNKNOBS_ERROR_FILE __FILE__
#define PUNKNOBS_ERROR_LINE __LINE__

void punknobs_error_throw_extra(
	struct punknobs* context,
	struct punknobs_error_info* error,
	enum punknobs_error code,
	const char* file,
	unsigned line);

void punknobs_error_init(
	struct punknobs* context);

#endif
