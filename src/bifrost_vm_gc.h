/******************************************************************************/
/*!
 * @file   bifrost_vm_gc.h
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @par
 *    Bifrost Scripting Language
 *
 * @brief
 *   A simple tracing garbage collector for the Bifrost Scripting Language.
 *   This uses a very basic mark and sweep algorithm.
 *
 *   References:
 *     [http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/]
 *
 *    Something to Think About Language Design Wise.
 *     [https://stackoverflow.com/questions/28320213/why-do-we-need-to-call-luas-collectgarbage-twice]
 *
 * @copyright Copyright (c) 2019-2025 Shareef Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_GC_H
#define BIFROST_VM_GC_H

#include "bifrost_libc.h"

#if __cplusplus
extern "C" {
#endif

typedef struct BifrostVM  BifrostVM;
typedef struct BifrostObj BifrostObj;

void  bfGC_Collect(BifrostVM* self);
void* bfGC_DefaultAllocator(void* user_data, void* ptr, size_t old_size, size_t new_size);
void* bfGC_AllocMemory(BifrostVM* self, void* ptr, size_t old_size, size_t new_size);
void  bfGC_PushRoot(BifrostVM* self, struct BifrostObj* obj);
void  bfGC_PopRoot(BifrostVM* self);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_GC_H */
