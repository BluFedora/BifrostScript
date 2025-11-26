#ifndef BIFROST_LIBC_H
#define BIFROST_LIBC_H

/* Lightweight headers for base types */

#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL      */
#include <stdint.h>  /* size int types    */

/* assert.h */

void LibC_assert(const char* const msg, const char* const condition_str, const char* const file, const int line, const char* const func);

#define LibC_assert(condition, msg)                                   \
  do {                                                                \
    if (!(condition))                                                 \
    {                                                                 \
      (LibC_assert)((msg), #condition, __FILE__, __LINE__, __func__); \
    }                                                                 \
  } while (0)

/* ctype.h */

bool LibC_isalpha(const char c);
bool LibC_isdigit(const char c);
bool LibC_isspace(const char c);

/* stdlib.h */

void   LibC_free(void* const ptr);
void*  LibC_realloc(void* const ptr, const size_t size);
double LibC_strtod(char const* const str, char** out_end);

/* string.h */

void LibC_memcpy(void* const dst, const void* const src, const size_t size);
int  LibC_memcmp(const void* const lhs, const void* const rhs, const size_t length);
void LibC_memmove(void* const dst, const void* const src, const size_t size);
void LibC_memset(void* const dst, const int value, const size_t size);
int  LibC_strncmp(const char* const lhs, const char* const rhs, const size_t length);
int  LibC_strcmp(const char* const lhs, const char* const rhs);

/* custom */

typedef char*            BifrostString;
typedef struct BifrostVM BifrostVM;

void bfVMString_sprintf(BifrostVM* vm, BifrostString* self, const char* format, ...);

#endif /* BIFROST_LIBC_H */
