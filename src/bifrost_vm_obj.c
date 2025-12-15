/******************************************************************************/
/*!
 * @file   bifrost_vm_obj.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Handles the object's available to the vm runtime.
 *
 * @version 0.0.1
 * @date    2020-02-16
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#include "bifrost_vm_obj.h"

#include "bifrost_vm_gc.h"  // Allocation Functions

typedef struct BifrostStringHeader
{
  size_t capacity;
  size_t length;

} BifrostStringHeader;

static size_t StringAllocationSize(size_t capacity)
{
  return sizeof(BifrostStringHeader) + capacity;
}

static BifrostString bfVMString_newLen(struct BifrostVM* vm, const char* initial_data, size_t string_length)
{
  const size_t str_capacity = string_length + 1;
  const size_t total_size   = StringAllocationSize(str_capacity);

  BifrostStringHeader* const self = bfGC_AllocMemory(vm, NULL, 0u, total_size);

  if (self)
  {
    self->capacity   = str_capacity;
    self->length     = string_length;
    char* const data = (char*)self + sizeof(BifrostStringHeader);

    /*
     // NOTE(Shareef):
     //   According to the standard memcpy cannot take in a NULL
     //   pointer and "size" must be non-zero, kinda stupid but ok.
    */
    if (initial_data && string_length)
    {
      LibC_memcpy(data, initial_data, string_length);
    }

    data[string_length] = '\0';

    return data;
  }

  return NULL;
}

BifrostStringHeader* bfVMString_getHeader(ConstBifrostString self)
{
  return ((BifrostStringHeader*)(self)) - 1;
}

static size_t bfVMString_length(ConstBifrostString self)
{
  return bfVMString_getHeader(self)->length;
}

static void bfVMString_delete(struct BifrostVM* vm, BifrostString self)
{
  BifrostStringHeader* const header = bfVMString_getHeader(self);

  bfGC_AllocMemory(vm, header, StringAllocationSize(header->capacity), 0u);
}

static uint32_t BifrostString_Hash(const char* str, size_t length)
{
  uint32_t hash = 0x811c9dc5;

  const char* str_end = str + length;

  while (str != str_end)
  {
    hash ^= (unsigned char)*str;
    hash *= 0x01000193;
    ++str;
  }

  return hash;
}

static unsigned char EscapeConvert(const unsigned char c)
{
  switch (c)
  {
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    case 'v':  return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '\"': return '\"';
    case '?':  return '\?';
    default:   return c;
  }
}

static size_t CString_unescape(char* str)
{
  const char* oldStr = str;
  char*       newStr = str;

  while (*oldStr)
  {
    unsigned char c = *(unsigned char*)(oldStr++);

    if (c == '\\')
    {
      c = *(unsigned char*)(oldStr++);
      if (c == '\0') break;
      c = EscapeConvert(c);
    }

    *newStr++ = (char)c;
  }

  *newStr = '\0';

  return (newStr - str);
}

static void bfVMString_unescape(BifrostString self)
{
  bfVMString_getHeader(self)->length = CString_unescape(self);
}

inline static void SetupGCObject(BifrostObj* obj, BifrostObjType type, BifrostObj** next)
{
  obj->type    = type;
  obj->gc_mark = 0;
  obj->next    = NULL;

  if (next)
  {
    obj->next = *next;
    *next     = obj;
  }
}

inline static BifrostObj* AllocateVMObjectImpl(struct BifrostVM* self, size_t size, const BifrostObjType type)
{
  BifrostObj* const obj = bfGC_AllocMemory(self, NULL, 0u, size);

  LibC_memset(obj, 0xFD, size);
  SetupGCObject(obj, type, &self->gc_object_list);

  return obj;
}
#define AllocateVMObjectEx(T, vm, type, extra_size) (T*)AllocateVMObjectImpl(vm, sizeof(T) + extra_size, type)
#define AllocateVMObject(T, vm, type)               AllocateVMObjectEx(T, vm, type, 0)

BifrostObjModule* bfObj_NewModule(struct BifrostVM* self, string_range name)
{
  BifrostObjModule* const module = AllocateVMObject(BifrostObjModule, self, BIFROST_VM_OBJ_MODULE);

  module->name      = bfVMString_newLen(self, name.str_bgn, name.str_len);
  module->variables = bfVMArray_new(self, module->variables, 32);
  LibC_memset(&module->init_fn, 0x0, sizeof(module->init_fn));
  module->init_fn.module = module;

  SetupGCObject(&module->init_fn.super, BIFROST_VM_OBJ_FUNCTION, NULL);

  return module;
}

BifrostObjClass* bfObj_NewClass(struct BifrostVM* self, BifrostObjModule* module, string_range name, BifrostObjClass* base_clz, size_t extra_data)
{
  BifrostObjClass* const clz = AllocateVMObject(BifrostObjClass, self, BIFROST_VM_OBJ_CLASS);

  clz->name               = bfVMString_newLen(self, name.str_bgn, name.str_len);
  clz->base_clz           = base_clz;
  clz->module             = module;
  clz->symbols            = bfVMArray_new(self, clz->symbols, 32);
  clz->field_initializers = bfVMArray_new(self, clz->field_initializers, 32);
  clz->extra_data         = extra_data;
  clz->finalizer          = NULL;

  return clz;
}

BifrostObjInstance* bfObj_NewInstance(struct BifrostVM* self, BifrostObjClass* clz)
{
  BifrostObjInstance* const inst = AllocateVMObjectEx(BifrostObjInstance, self, BIFROST_VM_OBJ_INSTANCE, clz->extra_data);

  bfHashMap_ctor(&inst->fields, self);
  inst->clz = clz;

  const size_t num_fields = bfVMArray_size(&clz->field_initializers);

  for (size_t i = 0; i < num_fields; ++i)
  {
    BifrostVMSymbol* const sym = clz->field_initializers + i;

    bfHashMap_set(&inst->fields, sym->name, sym->value);
  }

  return inst;
}

BifrostObjFn* bfObj_NewFunction(struct BifrostVM* self, BifrostObjModule* module)
{
  BifrostObjFn* const fn = AllocateVMObject(BifrostObjFn, self, BIFROST_VM_OBJ_FUNCTION);

  fn->module = module;

  /* NOTE(SR): 'fn' Will be filled out later by a Function Builder. */

  return fn;
}

BifrostObjNativeFn* bfObj_NewNativeFn(struct BifrostVM* self, bfNativeFnT fn_ptr, int32_t arity, uint32_t num_statics, uint16_t extra_data)
{
  BifrostObjNativeFn* const fn = AllocateVMObjectEx(BifrostObjNativeFn, self, BIFROST_VM_OBJ_NATIVE_FN, sizeof(BifrostValue) * num_statics + extra_data);

  fn->value           = fn_ptr;
  fn->arity           = arity;
  fn->num_statics     = num_statics;
  fn->statics         = (BifrostValue*)(fn + 1);
  fn->extra_data_size = extra_data;

  return fn;
}

BifrostObjStr* bfObj_NewString(struct BifrostVM* self, string_range value)
{
  BifrostObjStr* const obj = AllocateVMObject(BifrostObjStr, self, BIFROST_VM_OBJ_STRING);

  obj->value = bfVMString_newLen(self, value.str_bgn, value.str_len);
  bfVMString_unescape(obj->value);
  obj->hash = BifrostString_Hash(obj->value, bfVMString_length(obj->value));

  return obj;
}

BifrostObjReference* bfObj_NewReference(struct BifrostVM* self, size_t extra_data_size)
{
  BifrostObjReference* const obj = AllocateVMObjectEx(BifrostObjReference, self, BIFROST_VM_OBJ_NATIVE_INSTANCE, extra_data_size);

  obj->clz             = NULL;
  obj->extra_data_size = extra_data_size;
  LibC_memset(&obj->extra_data, 0x0, extra_data_size);

  return obj;
}

BifrostObjWeakRef* bfObj_NewWeaKRef(struct BifrostVM* self, void* data)
{
  BifrostObjWeakRef* const obj = AllocateVMObject(BifrostObjWeakRef, self, BIFROST_VM_OBJ_NATIVE_WEAK_REF);

  obj->clz  = NULL;
  obj->data = data;

  return obj;
}

size_t bfObj_AllocationSize(const BifrostObj* obj)
{
  switch (obj->type & BifrostVMObjType_mask)
  {
    case BIFROST_VM_OBJ_MODULE:
    {
      return sizeof(BifrostObjModule);
    }
    case BIFROST_VM_OBJ_CLASS:
    {
      return sizeof(BifrostObjClass);
    }
    case BIFROST_VM_OBJ_INSTANCE:
    {
      return sizeof(BifrostObjInstance) + ((const BifrostObjInstance*)obj)->clz->extra_data;
    }
    case BIFROST_VM_OBJ_FUNCTION:
    {
      return sizeof(BifrostObjFn);
    }
    case BIFROST_VM_OBJ_NATIVE_FN:
    {
      return sizeof(BifrostObjNativeFn) + ((const BifrostObjNativeFn*)obj)->num_statics * sizeof(BifrostValue) + ((BifrostObjNativeFn*)obj)->extra_data_size;
    }
    case BIFROST_VM_OBJ_STRING:
    {
      return sizeof(BifrostObjStr);
    }
    case BIFROST_VM_OBJ_NATIVE_INSTANCE:
    {
      return sizeof(BifrostObjReference) + ((const BifrostObjReference*)obj)->extra_data_size;
    }
    case BIFROST_VM_OBJ_NATIVE_WEAK_REF:
    {
      return sizeof(BifrostObjWeakRef);
    }
    InvalidDefaultCase;
  }

  return 0u;
}

void bfObj_Destruct(struct BifrostVM* self, BifrostObj* obj)
{
  switch (obj->type & BifrostVMObjType_mask)
  {
    case BIFROST_VM_OBJ_MODULE:
    {
      BifrostObjModule* const module = (BifrostObjModule*)obj;
      bfVMString_delete(self, module->name);
      bfVMArray_delete(self, &module->variables);
      if (module->init_fn.name)
      {
        bfObj_Destruct(self, &module->init_fn.super);
      }
      break;
    }
    case BIFROST_VM_OBJ_CLASS:
    {
      BifrostObjClass* const clz = (BifrostObjClass*)obj;

      bfVMString_delete(self, clz->name);
      bfVMArray_delete(self, &clz->symbols);
      bfVMArray_delete(self, &clz->field_initializers);
      break;
    }
    case BIFROST_VM_OBJ_INSTANCE:
    {
      BifrostObjInstance* const inst = (BifrostObjInstance*)obj;

      bfHashMap_dtor(&inst->fields);
      break;
    }
    case BIFROST_VM_OBJ_FUNCTION:
    {
      BifrostObjFn* const fn = (BifrostObjFn*)obj;

      bfObj_Delete(self, &fn->name->super);
      bfVMArray_delete(self, &fn->constants);
      bfVMArray_delete(self, &fn->instructions);
      bfVMArray_delete(self, &fn->code_to_line);
      break;
    }
    case BIFROST_VM_OBJ_STRING:
    {
      BifrostObjStr* const str = (BifrostObjStr*)obj;
      bfVMString_delete(self, str->value);
      break;
    }
    case BIFROST_VM_OBJ_NATIVE_FN:
    case BIFROST_VM_OBJ_NATIVE_INSTANCE:
    case BIFROST_VM_OBJ_NATIVE_WEAK_REF:
    default:
    {
      break;
    }
  }
}

size_t bfObj_Delete(struct BifrostVM* self, BifrostObj* obj)
{
  const size_t obj_size = bfObj_AllocationSize(obj);

  bfObj_Destruct(self, obj);
  bfGC_AllocMemory(self, obj, obj_size, 0u);

  return obj_size;
}

bool bfObj_IsFunction(const BifrostObj* obj)
{
  return obj->type == BIFROST_VM_OBJ_FUNCTION || obj->type == BIFROST_VM_OBJ_NATIVE_FN;
}

void bfObj_Finalize(struct BifrostVM* self, BifrostObj* obj)
{
  // TODO(SR): Find a way to guarantee instances don't get finalized twice

  if (obj->type == BIFROST_VM_OBJ_INSTANCE)
  {
    BifrostObjInstance* inst = (BifrostObjInstance*)obj;

    if (inst->clz->finalizer)
    {
      inst->clz->finalizer(self, &inst->extra_data);
    }
  }
  else if (obj->type == BIFROST_VM_OBJ_NATIVE_INSTANCE)
  {
    BifrostObjReference* ref = (BifrostObjReference*)obj;

    if (ref->clz->finalizer)
    {
      ref->clz->finalizer(self, &ref->extra_data);
    }
  }
}

/* array */

typedef struct ArrayDefaultCompareData
{
  size_t      stride;
  const void* key;

} ArrayDefaultCompareData;

#define SELF_CAST(s) ((unsigned char**)(s))

typedef struct BifrostArrayHeader
{
  size_t capacity;
  size_t size;
  size_t stride;

} BifrostArrayHeader;

static BifrostArrayHeader* Array_getHeader(unsigned char* self)
{
  return (BifrostArrayHeader*)(self - sizeof(BifrostArrayHeader));
}

static size_t ArrayAllocationSize(size_t capacity, size_t stride)
{
  return sizeof(BifrostArrayHeader) + capacity * stride;
}

void* _bfVMArrayT_new(struct BifrostVM* vm, const size_t stride, const size_t initial_capacity)
{
  LibC_assert(stride, "_ArrayT_new:: The struct must be greater than 0.");
  LibC_assert(initial_capacity * stride, "_ArrayT_new:: Please initialize the Array with a size greater than 0");

  vm->gc_is_running              = true;
  BifrostArrayHeader* const self = (BifrostArrayHeader*)bfGC_AllocMemory(vm, NULL, 0u, ArrayAllocationSize(initial_capacity, stride));
  vm->gc_is_running              = false;

  LibC_assert(self, "Array_new:: The Dynamic Array could not be allocated");

  if (!self)
  {
    return NULL;
  }

  self->capacity = initial_capacity;
  self->size     = 0;
  self->stride   = stride;

  return (uint8_t*)self + sizeof(BifrostArrayHeader);
}

static void* Array_end(const void* const self)
{
  BifrostArrayHeader* const header = Array_getHeader(*SELF_CAST(self));
  return *(char**)self + (header->size * header->stride);
}

size_t bfVMArray_size(const void* const self)
{
  return Array_getHeader(*SELF_CAST(self))->size;
}

void bfVMArray_clear(void* const self)
{
  Array_getHeader(*SELF_CAST(self))->size = 0;
}

static void Array_reserve(struct BifrostVM* vm, void* const self, const size_t num_elements)
{
  BifrostArrayHeader* header = Array_getHeader(*SELF_CAST(self));

  if (header->capacity < num_elements)
  {
    size_t new_capacity = (header->capacity >> 3) + (header->capacity < 9 ? 3 : 6) + header->capacity;

    if (new_capacity < num_elements)
    {
      new_capacity = num_elements;
    }

    vm->gc_is_running              = true;
    BifrostArrayHeader* new_header = (BifrostArrayHeader*)bfGC_AllocMemory(
     vm,
     header,
     ArrayAllocationSize(header->capacity, header->stride),
     ArrayAllocationSize(new_capacity, header->stride));

    if (new_header)
    {
      new_header->capacity = new_capacity;
      *SELF_CAST(self)     = (unsigned char*)new_header + sizeof(BifrostArrayHeader);
    }
    else
    {
      bfVMArray_delete(vm, self);
      *SELF_CAST(self) = NULL;
    }

    vm->gc_is_running = false;
  }
}

void bfVMArray_resize(struct BifrostVM* vm, void* const self, const size_t size)
{
  Array_reserve(vm, self, size);
  Array_getHeader(*SELF_CAST(self))->size = size;
}

void* bfVMArray_emplace(struct BifrostVM* vm, void* const self)
{
  return bfVMArray_emplaceN(vm, self, 1);
}

void* bfVMArray_emplaceN(struct BifrostVM* vm, void* const self, const size_t num_elements)
{
  const size_t old_size = bfVMArray_size(self);
  Array_reserve(vm, self, old_size + num_elements);
  uint8_t* const      new_element = Array_end(self);
  BifrostArrayHeader* header      = Array_getHeader(*SELF_CAST(self));
  LibC_memset(new_element, 0x0, header->stride * num_elements);
  header->size += num_elements;
  return new_element;
}

void* bfVMArray_pop(void* const self)
{
  LibC_assert(bfVMArray_size(self) != 0, "Array_pop:: attempt to pop empty array");

  BifrostArrayHeader* const header      = Array_getHeader(*SELF_CAST(self));
  void* const               old_element = bfVMArray_back(self);
  --header->size;

  return old_element;
}

void* bfVMArray_back(const void* const self)
{
  const BifrostArrayHeader* const header = Array_getHeader(*SELF_CAST(self));

  return (char*)Array_end(self) - header->stride;
}

void bfVMArray_delete(struct BifrostVM* vm, void* const self)
{
  BifrostArrayHeader* const header = Array_getHeader(*SELF_CAST(self));

  bfGC_AllocMemory(vm, header, ArrayAllocationSize(header->capacity, header->stride), 0u);
}

/* string */

StringCmp StringCmp_Make(const char* const str, const size_t length)
{
  return (StringCmp){.str = str, .length = (uint32_t)length, .hash = BifrostString_Hash(str, length)};
}

StringCmp StringCmp_FromStr(ConstBifrostString self) { return StringCmp_Make(self, bfVMString_length(self)); }
StringCmp StringCmp_FromBStr(const BifrostObjStr* self)
{
  return (StringCmp){.str = self->value, .length = (uint32_t)BifrostString_length(self), .hash = self->hash};
}
StringCmp StringCmp_FromStrView(const string_range self) { return StringCmp_Make(self.str_bgn, self.str_len); }

bool StringCmp_Cmp(const StringCmp lhs, const StringCmp rhs)
{
  return lhs.hash == rhs.hash && lhs.length == rhs.length && LibC_strncmp(lhs.str, rhs.str, lhs.length) == 0;
}

BifrostStringHeader* bfVMString_getHeader(ConstBifrostString self);

size_t BifrostString_length(const BifrostObjStr* self)
{
  return bfVMString_length(self->value);
}

string_range BifrostString_AsStrRng2(const BifrostString self)
{
  return MakeStringLen(self, bfVMString_length(self));
}

inline static void String_FmtPush(BifrostVM* const vm, BifrostObjStr* const str, const char c)
{
  BifrostStringHeader* header = bfVMString_getHeader(str->value);

  if (header->length == header->capacity)
  {
    const size_t new_capacity = header->capacity == 0 ? 16 : header->capacity * 2;

    bfVMString_reserve(vm, &str->value, new_capacity);
    header = bfVMString_getHeader(str->value);
  }

  str->value[header->length++] = c;
}

inline static void String_FmtU64(BifrostVM* const vm, BifrostObjStr* const str, uint64_t value, const bool prefix_sign, const uint64_t base, const bool upper_case)
{
  static const char k_DigitTableLower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  static const char k_DigitTableUpper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  LibC_assert(base >= 2 && base <= 36, "Out of range base.");

  char tmp[64]; /* Enough to hold max u64 in base 2 */
  int  tmp_len = 0;

  const char* const digit_table = upper_case ? k_DigitTableUpper : k_DigitTableLower;

  if (value == 0)
  {
    tmp[tmp_len++] = '0';
  }
  else
  {
    while (value != 0)
    {
      tmp[tmp_len++]  = digit_table[value % base];
      value          /= base;
    }
  }

  if (prefix_sign)
  {
    String_FmtPush(vm, str, '+');
  }

  for (int i = tmp_len - 1; i >= 0; --i)
  {
    String_FmtPush(vm, str, tmp[i]);
  }
}

inline static void String_FmtI64(BifrostVM* const vm, BifrostObjStr* const str, const int64_t value, bool prefix_sign, const uint64_t base, const bool upper_case)
{
  uint64_t unsigned_value;

  if (value < 0)
  {
    String_FmtPush(vm, str, '-');

    unsigned_value = (uint64_t)(-(value + 1)) + 1;
    prefix_sign    = false;
  }
  else
  {
    unsigned_value = (uint64_t)value;
  }

  String_FmtU64(vm, str, unsigned_value, prefix_sign, base, upper_case);
}

inline static void String_FmtFlt(BifrostVM* const vm, BifrostObjStr* const str, double value, const bool prefix_sign, uint64_t precision)
{
  if (precision == 0) { precision = 6; } /* Default to a precision of 6. */

  if (value < 0.0)
  {
    String_FmtPush(vm, str, '-');
    value = -value;
  }

  const uint64_t integer_part = (uint64_t)value;

  String_FmtU64(vm, str, integer_part, prefix_sign, 10, false);

  if (precision > 0)
  {
    String_FmtPush(vm, str, '.');

    double fractional_part = value - integer_part;

    for (uint64_t i = 0; i < precision; ++i)
    {
      fractional_part *= 10.0;

      const char digit = (char)fractional_part;  // [0, 10)

      String_FmtPush(vm, str, '0' + digit);

      fractional_part -= digit;
    }
  }
}

inline static uint64_t StrFmt_ExtractImpl(const StrFmt* const fmt_arg, const uint64_t shift, const uint64_t bit_count)
{
  const uint64_t mask = ((uint64_t)(1) << bit_count) - 1;

  return (fmt_arg->flags >> shift) & mask;
}

#define StrFmt_Extract(flags, T) StrFmt_ExtractImpl(flags, StrFmt_Shift_##T, StrFmt_Count_##T)

void String_FmtImpl(BifrostVM* const vm, BifrostObjStr* const str, const char* const fmt_str, const size_t fmt_str_length, const StrFmt* const fmt_args, const size_t fmt_args_count)
{
  size_t fmt_arg_index  = 0;
  bool   has_escape_seq = false;

  bfVMString_getHeader(str->value)->length = 0;

  // TODO(SR):
  //  - Optimize for batch pushing by keeping track of non arg string parts.
  //  - Optimize for batch pushing for string.
  //  - Align from the width.

  for (size_t fmt_str_index = 0; fmt_str_index < fmt_str_length;)
  {
    const char c = fmt_str[fmt_str_index++];

    switch (c)
    {
      case '{':
      {
        LibC_assert(fmt_str_index < fmt_str_length, "{ must always be followed by another character.");

        const char next_c = fmt_str[fmt_str_index++];

        if (next_c == '}')  // {}
        {
          LibC_assert(fmt_arg_index < fmt_args_count, "Too many format holes for number of args passed in.");

          const StrFmt* const fmt_arg = fmt_args + fmt_arg_index++;

          switch (StrFmt_Extract(fmt_arg, Type))
          {
            case StrFmtType_Str:
            {
              const uint64_t    string_length = StrFmt_Extract(fmt_arg, Precision);
              const char* const string        = fmt_arg->data.str;

              for (uint64_t i = 0; i < string_length; ++i)
              {
                String_FmtPush(vm, str, string[i]);
              }

              break;
            }
            case StrFmtType_Int:
            {
              const bool     prefix_sign = StrFmt_Extract(fmt_arg, PlusSign) != 0;
              const uint64_t base        = StrFmt_Extract(fmt_arg, IsHex) ? 16 : 10;
              const bool     upper_case  = true;

              if (StrFmt_Extract(fmt_arg, IsSigned))
              {
                String_FmtI64(vm, str, fmt_arg->data.i64, prefix_sign, base, upper_case);
              }
              else
              {
                String_FmtU64(vm, str, fmt_arg->data.u64, prefix_sign, base, upper_case);
              }

              break;
            }
            case StrFmtType_Flt:
            {
              const bool     prefix_sign = StrFmt_Extract(fmt_arg, PlusSign) != 0;
              const uint64_t precision   = StrFmt_Extract(fmt_arg, Precision);

              String_FmtFlt(vm, str, fmt_arg->data.f64, prefix_sign, precision);

              break;
            }
            case StrFmtType_Char:
            {
              String_FmtPush(vm, str, fmt_arg->data.ch);
              break;
            }
          }
        }
        else if (next_c == '{')  // {{
        {
          String_FmtPush(vm, str, '{');
        }
        else
        {
          LibC_assert(false, "Unknown format hole character.");
        }
        break;
      }
      case '\\':
        has_escape_seq = true;
      default:
      {
        String_FmtPush(vm, str, c);
        break;
      }
    }
  }

  String_FmtPush(vm, str, '\0');  // TODO(SR): TODO REMOVE!!

  if (has_escape_seq)
  {
    bfVMString_unescape(str->value);
  }
  str->hash = BifrostString_Hash(str->value, bfVMString_length(str->value));
}
