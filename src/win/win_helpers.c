#include "include/punknobs.h"

#include "common/punknobs_private.h"
#include "win/win.h"
#include "win/win_helpers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PUNKNOBS_XUSB_HARDWARE_NAME "XUSB Controller"

unsigned __stdcall device_loop(void* data)
{
	// TODO
	return 0;
}

unsigned __stdcall input_loop(void* data)
{
	// TODO
	return 0;
}

// utf16 to utf8 conversion
char* utf16_to_utf8(WCHAR* win_string, bool capitalize)
{
	// early exit if utf16 input is NULL
	if (win_string == NULL)
	{
		// input error
		return NULL;
	}

	// utf8 buffer
	size_t utf8_alloc_step = 32 * 4; // allocation increment for the utf8 buffer in bytes
	size_t utf8_alloc_size = 1; // allocation size of the utf8 buffer in bytes
	char* utf8 = NULL; // utf8 buffer

	// initialize loop variables
	uint32_t utf16 = '\0'; // current utf16 character
	uint32_t tmp = 0; // temporary space for quad-byte utf16 reconstruction
	char* utf8_string = NULL; // current utf8 codepoint in the utf8 buffer 
	size_t utf8_size = 0; // length of the converted utf8 string in bytes

	// only exit the loop *after* we have converted NUL
	do
	{
		// get two bytes of utf16 text
		utf16 = *win_string;

		// increase memory allocation when we are running out of space
		if ((utf8_alloc_size - utf8_size) < 5)
		{
			utf8_alloc_size += utf8_alloc_step;
			utf8 = realloc(utf8, utf8_alloc_size);

			if (utf8 == NULL)
			{
				// alloc error
				return NULL;
			}
		}

		// recompute the current utf8 codepoint pointer here
		// in case we reallocated the underlying buffer
		utf8_string = utf8 + utf8_size;

		// quad-byte utf16 UCS reconstruction
		if ((utf16 & 0xFC00) == 0xD800)
		{
			// the first six bits of the first two bytes
			// start like 0xD8, try loading two more bytes
			// to decode the ucs value from these four bytes
			tmp = (0x3FF & utf16) << 10;
			++win_string;
			utf16 = *win_string;

			if ((utf16 & 0xFC00) == 0xDC00)
			{
				// the first six bits of the next two bytes
				// start like 0xDC, decode the ucs value from
				// all four bytes before starting utf8 encoding
				utf16 = ((0x3FF & utf16) | tmp) + 0x10000;
			}

			// if the first six bits of the next two bytes
			// did not start like 0xDC, we will fall back
			// to handling them as double-byte utf16,
			// ignoring the invalid first two bytes
		}

		// utf8 conversion from ucs
		if (utf16 < 0x80)
		{
			utf8_string[0] = capitalize && (islower(utf16) > 0) ? (utf16 - 32) : utf16;
			utf8_size += 1;
		}
		else if (utf16 < 0x800)
		{
			utf8_string[0] = 0xC0 | (0x1F & (utf16 >> 6));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 2;
		}
		else if (utf16 < 0x10000)
		{
			utf8_string[0] = 0xE0 | (0x0F & (utf16 >> 12));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 6));
			utf8_string[2] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 3;
		}
		else
		{
			utf8_string[0] = 0xF0 | (0x07 & (utf16 >> 18));
			utf8_string[1] = 0x80 | (0x3F & (utf16 >> 12));
			utf8_string[2] = 0x80 | (0x3F & (utf16 >> 6));
			utf8_string[3] = 0x80 | (0x3F & (utf16 >> 0));
			utf8_size += 4;
		}

		// prepare getting the next two bytes of utf16 text
		++win_string;
	}
	while (utf16 != '\0');

	// shrink the utf8 buffer to the actual length of the converted string
	utf8 = realloc(utf8, utf8_size);

	if (utf8 == NULL)
	{
		// alloc error
		return NULL;
	}

	// all good
	return utf8;
}
