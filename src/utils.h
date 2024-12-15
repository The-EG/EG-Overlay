#pragma once
#include <crtdbg.h>
#include "lamath.h"
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>

#define egoverlay_calloc(num, size) _calloc_dbg(num, size, _CLIENT_BLOCK, __FILE__, __LINE__)
#define egoverlay_realloc(data, newsize) _realloc_dbg(data, newsize, _CLIENT_BLOCK, __FILE__, __LINE__)
#define egoverlay_malloc(size) _malloc_dbg(size, _CLIENT_BLOCK, __FILE__, __LINE__)
#define egoverlay_free(data) _free_dbg(data, _CLIENT_BLOCK)

char *load_file(const char *path, size_t *length);

uint32_t djb2_hash_string(const char *string);

char *wchar_to_char(const wchar_t *wstr);
wchar_t *char_to_wchar(const char *str);

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(p) (void)p
#endif

void error_and_exit(const char *title, const char *msg_format, ...);
