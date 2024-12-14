#include "include/punknobs.h"
#include "common/punknobs_private.h"
#include "common/punknobs_error.h"

#include <stdbool.h>

#if defined(PUNKNOBS_ERROR_LOG_MANUAL) || defined(PUNKNOBS_ERROR_LOG_THROW)
	#include <stdio.h>
#endif

#ifdef PUNKNOBS_ERROR_ABORT
	#include <stdlib.h>
#endif

void punknobs_error_init(
	struct punknobs* context)
{
#ifndef PUNKNOBS_ERROR_SKIP
	char** log = context->error_messages;

	log[PUNKNOBS_ERROR_OK] =
		"out-of-bound error message";
	log[PUNKNOBS_ERROR_NULL] =
		"null pointer";
	log[PUNKNOBS_ERROR_ALLOC] =
		"failed malloc";
	log[PUNKNOBS_ERROR_BOUNDS] =
		"out-of-bounds index";
	log[PUNKNOBS_ERROR_DOMAIN] =
		"invalid domain";
	// TODO
#endif
}

void punknobs_error_log(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
#ifndef PUNKNOBS_ERROR_SKIP
	#ifdef PUNKNOBS_ERROR_LOG_MANUAL
		#ifdef PUNKNOBS_ERROR_LOG_DEBUG
			fprintf(
				stderr,
				"error in %s line %u: ",
				error->file,
				error->line);
		#endif

		if (error->code < PUNKNOBS_ERROR_COUNT)
		{
			fprintf(stderr, "%s\n", context->error_messages[error->code]);
		}
		else
		{
			fprintf(stderr, "%s\n", context->error_messages[0]);
		}
	#endif
#endif
}

const char* punknobs_error_get_msg(
	struct punknobs* context,
	struct punknobs_error_info* error)
{
	if (error->code < PUNKNOBS_ERROR_COUNT)
	{
		return context->error_messages[error->code];
	}
	else
	{
		return context->error_messages[0];
	}
}

enum punknobs_error punknobs_error_get_code(
	struct punknobs_error_info* error)
{
	return error->code;
}

const char* punknobs_error_get_file(
	struct punknobs_error_info* error)
{
	return error->file;
}

unsigned punknobs_error_get_line(
	struct punknobs_error_info* error)
{
	return error->line;
}

void punknobs_error_ok(
	struct punknobs_error_info* error)
{
	error->code = PUNKNOBS_ERROR_OK;
	error->file = "";
	error->line = 0;
}

void punknobs_error_throw_extra(
	struct punknobs* context,
	struct punknobs_error_info* error,
	enum punknobs_error code,
	const char* file,
	unsigned line)
{
#ifndef PUNKNOBS_ERROR_SKIP
	error->code = code;
	error->file = file;
	error->line = line;

	#ifdef PUNKNOBS_ERROR_LOG_THROW
		#ifdef PUNKNOBS_ERROR_LOG_DEBUG
			fprintf(
				stderr,
				"error in %s line %u: ",
				file,
				line);
		#endif

		if (error->code < PUNKNOBS_ERROR_COUNT)
		{
			fprintf(stderr, "%s\n", context->error_messages[error->code]);
		}
		else
		{
			fprintf(stderr, "%s\n", context->error_messages[0]);
		}
	#endif

	#ifdef PUNKNOBS_ERROR_ABORT
		abort();
	#endif
#endif
}
