#ifndef H_PUNKNOBS_BACKEND_WIN_HELPERS
#define H_PUNKNOBS_BACKEND_WIN_HELPERS

#include <stdbool.h>
#include <windows.h>

unsigned __stdcall device_loop(void* data);
unsigned __stdcall input_loop(void* data);

char* utf16_to_utf8(WCHAR* win_string, bool capitalize);

#endif
