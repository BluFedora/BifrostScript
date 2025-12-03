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

/* static uint8_t TAG_GET(const BifrostValue value) { return (uint8_t)(value & k_VMValueTagMask); } */

#define k_doubleSignBit      ((uint64_t)(1) << 63)
#define k_QuietNan           (uint64_t)(0x7FFC000000000000ULL)
#define k_VMValuePointerMask (k_doubleSignBit | k_QuietNan)
#define k_VMValueTagMask     (uint64_t)0x3
#define k_VMValueTagNan      (uint64_t)0x0
#define k_VMValueTagNull     (uint64_t)0x1
#define k_VMValueTagTrue     (uint64_t)0x2
#define k_VMValueTagFalse    (uint64_t)0x3 /* Tags Bits 4-6 unused */
#define k_VMValueNull        (BifrostValue)((uint64_t)(k_QuietNan | (k_VMValueTagNull)))
#define k_VMValueTrue        (BifrostValue)((uint64_t)(k_QuietNan | (k_VMValueTagTrue)))
#define k_VMValueFalse       (BifrostValue)((uint64_t)(k_QuietNan | (k_VMValueTagNull)))

typedef uint64_t BifrostValue; /*!< The Nan-Tagged value representation of this scripting language. */

/* Type Checking */

bool bfVMValue_isNull(const BifrostValue value);
bool bfVMValue_isBool(const BifrostValue value);
bool bfVMValue_isTrue(const BifrostValue value);
bool bfVMValue_isFalse(const BifrostValue value);
bool bfVMValue_isPointer(const BifrostValue value);
bool bfVMValue_isNumber(const BifrostValue value);

/* From Conversions */

BifrostValue bfVMValue_fromNull(void);
BifrostValue bfVMValue_fromBool(const bool value);
BifrostValue bfVMValue_fromNumber(const double value);
BifrostValue bfVMValue_fromPointer(const void* value);

/* To Conversions */

double bfVMValue_asNumber(const BifrostValue self);
void*  bfVMValue_asPointer(const BifrostValue self);
bool   bfVMValue_isThuthy(const BifrostValue self);

/* Binary Ops */

BifrostValue bfVMValue_sub(const BifrostValue lhs, const BifrostValue rhs);
BifrostValue bfVMValue_mul(const BifrostValue lhs, const BifrostValue rhs);
BifrostValue bfVMValue_div(const BifrostValue lhs, const BifrostValue rhs);
bool      bfVMValue_ee(const BifrostValue lhs, const BifrostValue rhs);
bool      bfVMValue_lt(const BifrostValue lhs, const BifrostValue rhs);
bool      bfVMValue_gt(const BifrostValue lhs, const BifrostValue rhs);
bool      bfVMValue_ge(const BifrostValue lhs, const BifrostValue rhs);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_VALUE_H */
