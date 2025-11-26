/******************************************************************************/
/*!
 * @file   bifrost_vm_value.h
 * @author Shareef Raheem (http://blufedora.github.io/)
 * @brief
 *   Helpers for the value representation for the vm.
 *   Uses Nan-Tagging for compact storage of objects.
 *   (Use of double is required for Nan Tagging)
 *
 *   [NaN boxing or how to make the world dynamic](https://piotrduperas.com/posts/nan-boxing)
 *   [Crafting Interpreters:Optimization#NaN Boxing](https://craftinginterpreters.com/optimization.html#nan-boxing)
 *
 * @copyright Copyright (c) 2020-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_VALUE_H
#define BIFROST_VM_VALUE_H

#include "bifrost_libc.h"

#if __cplusplus
extern "C" {
#endif

/* static uint8_t TAG_GET(const bfVMValue value) { return (uint8_t)(value & k_VMValueTagMask); } */

#define k_doubleSignBit      ((uint64_t)(1) << 63)
#define k_QuietNan           (uint64_t)(0x7FFC000000000000ULL)
#define k_VMValuePointerMask (k_doubleSignBit | k_QuietNan)
#define k_VMValueTagMask     (uint64_t)0x3
#define k_VMValueTagNan      (uint64_t)0x0
#define k_VMValueTagNull     (uint64_t)0x1
#define k_VMValueTagTrue     (uint64_t)0x2
#define k_VMValueTagFalse    (uint64_t)0x3 /* Tags Bits 4-6 unused */
#define k_VMValueNull        (bfVMValue)((uint64_t)(k_QuietNan | (k_VMValueTagNull)))
#define k_VMValueTrue        (bfVMValue)((uint64_t)(k_QuietNan | (k_VMValueTagTrue)))
#define k_VMValueFalse       (bfVMValue)((uint64_t)(k_QuietNan | (k_VMValueTagNull)))

typedef uint64_t bfVMValue; /*!< The Nan-Tagged value representation of this scripting language. */

/* Type Checking */

bool bfVMValue_isNull(const bfVMValue value);
bool bfVMValue_isBool(const bfVMValue value);
bool bfVMValue_isTrue(const bfVMValue value);
bool bfVMValue_isFalse(const bfVMValue value);
bool bfVMValue_isPointer(const bfVMValue value);
bool bfVMValue_isNumber(const bfVMValue value);

/* From Conversions */

bfVMValue bfVMValue_fromNull(void);
bfVMValue bfVMValue_fromBool(const bool value);
bfVMValue bfVMValue_fromNumber(const double value);
bfVMValue bfVMValue_fromPointer(const void* value);

/* To Conversions */

double bfVMValue_asNumber(const bfVMValue self);
void*  bfVMValue_asPointer(const bfVMValue self);
bool   bfVMValue_isThuthy(const bfVMValue self);

/* Binary Ops */

bfVMValue bfVMValue_sub(const bfVMValue lhs, const bfVMValue rhs);
bfVMValue bfVMValue_mul(const bfVMValue lhs, const bfVMValue rhs);
bfVMValue bfVMValue_div(const bfVMValue lhs, const bfVMValue rhs);
bool      bfVMValue_ee(const bfVMValue lhs, const bfVMValue rhs);
bool      bfVMValue_lt(const bfVMValue lhs, const bfVMValue rhs);
bool      bfVMValue_gt(const bfVMValue lhs, const bfVMValue rhs);
bool      bfVMValue_ge(const bfVMValue lhs, const bfVMValue rhs);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_VALUE_H */
