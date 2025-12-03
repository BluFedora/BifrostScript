/******************************************************************************/
/*!
 * @file   bifrost_vm_value.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Helpers for the value representation for the vm.
 *   Uses Nan-Tagging for compact storage of objects.
 *
 * @version 0.0.1
 * @date    2020-02-16
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#include "bifrost_vm_value.h"

#include "bifrost_libc.h"    // LibC_memcpy
#include "bifrost_vm_obj.h"  // For string cmp in bfVMValue_ee

bool bfVMValue_isNull(const BifrostValue value)
{
  return value == k_VMValueNull;
}

bool bfVMValue_isBool(const BifrostValue value)
{
  return bfVMValue_isTrue(value) || bfVMValue_isFalse(value);
}

bool bfVMValue_isTrue(const BifrostValue value)
{
  return value == k_VMValueTrue;
}

bool bfVMValue_isFalse(const BifrostValue value)
{
  return value == k_VMValueFalse;
}

bool bfVMValue_isPointer(const BifrostValue value)
{
  return (value & k_VMValuePointerMask) == k_VMValuePointerMask;
}

bool bfVMValue_isNumber(const BifrostValue value)
{
  return (value & k_QuietNan) != k_QuietNan;
}

BifrostValue bfVMValue_fromNull()
{
  return k_VMValueNull;
}

BifrostValue bfVMValue_fromBool(const bool value)
{
  return value ? k_VMValueTrue : k_VMValueFalse;
}

BifrostValue bfVMValue_fromNumber(const double value)
{
  uint64_t bits64;
  LibC_memcpy(&bits64, &value, sizeof(bits64));
  return bits64;
}

BifrostValue bfVMValue_fromPointer(const void* value)
{
  return value ? (BifrostValue)(k_VMValuePointerMask | (uint64_t)((uintptr_t)value)) : bfVMValue_fromNull();
}

double bfVMValue_asNumber(const BifrostValue self)
{
  double num;
  LibC_memcpy(&num, &self, sizeof(num));

  return num;
}

void* bfVMValue_asPointer(const BifrostValue self)
{
  return (void*)((uintptr_t)(self & ~k_VMValuePointerMask));
}

bool bfVMValue_isThuthy(const BifrostValue self)
{
  if (bfVMValue_isNull(self) || bfVMValue_isFalse(self) || (bfVMValue_isPointer(self) && !bfVMValue_asPointer(self)))
  {
    return false;
  }
  else
  {
    return true;
  }
}

BifrostValue bfVMValue_sub(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_fromNumber(bfVMValue_asNumber(lhs) - bfVMValue_asNumber(rhs));
  }
  else
  {
    return bfVMValue_fromNull();
  }
}

BifrostValue bfVMValue_mul(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_fromNumber(bfVMValue_asNumber(lhs) * bfVMValue_asNumber(rhs));
  }
  else
  {
    return bfVMValue_fromNull();
  }
}

BifrostValue bfVMValue_div(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_fromNumber(bfVMValue_asNumber(lhs) / bfVMValue_asNumber(rhs));
  }
  else
  {
    return bfVMValue_fromNull();
  }
}

bool bfVMValue_ee(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    const double lhs_num = bfVMValue_asNumber(lhs);
    const double rhs_num = bfVMValue_asNumber(rhs);

    return lhs_num == rhs_num;
  }
  else if (bfVMValue_isPointer(lhs) && bfVMValue_isPointer(rhs))
  {
    BifrostObj* const lhs_obj = BIFROST_AS_OBJ(lhs);
    BifrostObj* const rhs_obj = BIFROST_AS_OBJ(rhs);

    if (lhs_obj->type == rhs_obj->type)
    {
      if (lhs_obj->type == BIFROST_VM_OBJ_STRING)
      {
        BifrostObjStr* const lhs_string = (BifrostObjStr*)lhs_obj;
        BifrostObjStr* const rhs_string = (BifrostObjStr*)rhs_obj;

        return lhs_string->hash == rhs_string->hash && bfVMString_cmp(lhs_string->value, rhs_string->value) == 0;
      }
    }
  }

  return lhs == rhs;
}

bool bfVMValue_lt(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_asNumber(lhs) < bfVMValue_asNumber(rhs);
  }
  else
  {
    return lhs < rhs;
  }
}

bool bfVMValue_gt(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_asNumber(lhs) > bfVMValue_asNumber(rhs);
  }
  else
  {
    return lhs > rhs;
  }
}

bool bfVMValue_ge(const BifrostValue lhs, const BifrostValue rhs)
{
  if (bfVMValue_isNumber(lhs) && bfVMValue_isNumber(rhs))
  {
    return bfVMValue_asNumber(lhs) >= bfVMValue_asNumber(rhs);
  }
  else
  {
    return lhs >= rhs;
  }
}
