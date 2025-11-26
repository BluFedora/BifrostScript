#include "bifrost_libc.h"

#include <ctype.h>  /* isalpha, isdigit, isspace */
#include <stdarg.h> /* va_list, va_start, va_copy, va_end */
#include <stdio.h>  /* fprintf, stderr, fflush, vsnprintf,  */
#include <stdlib.h> /* abort, strtod */
#include <string.h> /* memcpy, memmove, memset, strncmp */

void(LibC_assert)(const char* const msg, const char* const condition_str, const char* const file, const int line, const char* const func)
{
  fprintf(stderr, "ASSERT(%s): %s:%s(%i): \"%s\"\n", msg, condition_str, file, line, func);
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
int    LibC_strncmp(const char* const lhs, const char* const rhs, const size_t length) { return strncmp(lhs, rhs, length); }
int    LibC_strcmp(const char* const lhs, const char* const rhs) { return strcmp(lhs, rhs); }

typedef struct BifrostStringHeader
{
  size_t capacity;
  size_t length;

} BifrostStringHeader;

typedef const char* ConstBifrostString;

extern void                 bfVMString_reserve(struct BifrostVM* vm, BifrostString* self, size_t new_capacity);
extern BifrostStringHeader* bfVMString_getHeader(ConstBifrostString self);

void bfVMString_sprintf(BifrostVM* vm, BifrostString* self, const char* format, ...)
{
  va_list args, args_cpy;
  va_start(args, format);

  va_copy(args_cpy, args);
  const size_t num_chars = (size_t)vsnprintf(NULL, 0, format, args_cpy);
  va_end(args_cpy);

  bfVMString_reserve(vm, self, num_chars + 2);
  vsnprintf(*self, num_chars + 1, format, args);
  bfVMString_getHeader(*self)->length = num_chars;

  va_end(args);
}
