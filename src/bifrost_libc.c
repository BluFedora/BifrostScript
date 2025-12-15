#include "bifrost_libc.h"

#include <ctype.h>  /* isalpha, isdigit, isspace        */
#include <stdio.h>  /* fprintf, stderr, fflush,         */
#include <stdlib.h> /* realloc, free, abort, strtod     */
#include <string.h> /* memcpy, memmove, memset          */

void(LibC_assert)(const char* const msg, const char* const condition_str, const char* const file, const int line, const char* const func)
{
  fprintf(stderr, "ASSERT(%s)[%s|%i]: \"%s\" {%s}\n", file, func, line, msg, condition_str);
  fflush(stderr);
  abort();
}

bool   LibC_isalpha(const char c) { return isalpha(c); }
bool   LibC_isdigit(const char c) { return isdigit(c); }
bool   LibC_isspace(const char c) { return isspace(c); }
void   LibC_free(void* const ptr) { free(ptr); }
void*  LibC_realloc(void* const ptr, const size_t size) { return realloc(ptr, size); }
double LibC_strtod(char const* const str, char** out_end) { return strtod(str, out_end); }
void   LibC_memcpy(void* const dst, const void* const src, const size_t size) { memcpy(dst, src, size); }
int    LibC_memcmp(const void* const lhs, const void* const rhs, const size_t length) { return memcmp(lhs, rhs, length); }
void   LibC_memmove(void* const dst, const void* const src, const size_t size) { memmove(dst, src, size); }
void   LibC_memset(void* const dst, const int value, const size_t size) { memset(dst, value, size); }
