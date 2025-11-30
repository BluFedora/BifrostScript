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

static inline void SetupGCObject(BifrostObj* obj, BifrostVMObjType type, BifrostObj** next)
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

static inline void* AllocateVMObjectImpl(struct BifrostVM* self, size_t size, const BifrostVMObjType type)
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
  BifrostObjModule* module = AllocateVMObject(BifrostObjModule, self, BIFROST_VM_OBJ_MODULE);

  module->name      = bfVMString_newLen(self, name.str_bgn, name.str_len);
  module->variables = bfVMArray_newA(self, module->variables, 32);
  LibC_memset(&module->init_fn, 0x0, sizeof(module->init_fn));
  module->init_fn.module = module;

  SetupGCObject(&module->init_fn.super, BIFROST_VM_OBJ_FUNCTION, NULL);

  return module;
}

BifrostObjClass* bfObj_NewClass(struct BifrostVM* self, BifrostObjModule* module, string_range name, BifrostObjClass* base_clz, size_t extra_data)
{
  BifrostObjClass* clz = AllocateVMObject(BifrostObjClass, self, BIFROST_VM_OBJ_CLASS);

  clz->name               = bfVMString_newLen(self, name.str_bgn, name.str_len);
  clz->base_clz           = base_clz;
  clz->module             = module;
  clz->symbols            = bfVMArray_newA(self, clz->symbols, 32);
  clz->field_initializers = bfVMArray_newA(self, clz->field_initializers, 32);
  clz->extra_data         = extra_data;
  clz->finalizer          = NULL;

  return clz;
}

BifrostObjInstance* bfObj_NewInstance(struct BifrostVM* self, BifrostObjClass* clz)
{
  BifrostObjInstance* inst = AllocateVMObjectEx(BifrostObjInstance, self, BIFROST_VM_OBJ_INSTANCE, clz->extra_data);

  BifrostHashMapParams hash_params;
  bfHashMapParams_init(&hash_params, self);
  hash_params.value_size = sizeof(bfVMValue);

  bfHashMap_ctor(&inst->fields, &hash_params);
  inst->clz = clz;

  const size_t num_fields = bfVMArray_size(&clz->field_initializers);

  for (size_t i = 0; i < num_fields; ++i)
  {
    BifrostVMSymbol* const sym = clz->field_initializers + i;

    bfHashMap_set(&inst->fields, sym->name, &sym->value);
  }

  return inst;
}

BifrostObjFn* bfObj_NewFunction(struct BifrostVM* self, BifrostObjModule* module)
{
  BifrostObjFn* fn = AllocateVMObject(BifrostObjFn, self, BIFROST_VM_OBJ_FUNCTION);

  fn->module = module;

  /* NOTE(SR): 'fn' Will be filled out later by a Function Builder. */

  return fn;
}

BifrostObjNativeFn* bfObj_NewNativeFn(struct BifrostVM* self, bfNativeFnT fn_ptr, int32_t arity, uint32_t num_statics, uint16_t extra_data)
{
  BifrostObjNativeFn* fn = AllocateVMObjectEx(BifrostObjNativeFn, self, BIFROST_VM_OBJ_NATIVE_FN, sizeof(bfVMValue) * num_statics + extra_data);

  fn->value           = fn_ptr;
  fn->arity           = arity;
  fn->num_statics     = num_statics;
  fn->statics         = (bfVMValue*)((char*)fn + sizeof(BifrostObjNativeFn));
  fn->extra_data_size = extra_data;

  return fn;
}

BifrostObjStr* bfObj_NewString(struct BifrostVM* self, string_range value)
{
  BifrostObjStr* obj = AllocateVMObject(BifrostObjStr, self, BIFROST_VM_OBJ_STRING);

  obj->value = bfVMString_newLen(self, value.str_bgn, value.str_len);
  bfVMString_unescape(obj->value);
  obj->hash = bfVMString_hashN(obj->value, bfVMString_length(obj->value));

  return obj;
}

BifrostObjReference* bfObj_NewReference(struct BifrostVM* self, size_t extra_data_size)
{
  BifrostObjReference* obj = AllocateVMObjectEx(BifrostObjReference, self, BIFROST_VM_OBJ_REFERENCE, extra_data_size);

  obj->clz             = NULL;
  obj->extra_data_size = extra_data_size;
  LibC_memset(&obj->extra_data, 0x0, extra_data_size);

  return obj;
}

BifrostObjWeakRef* bfObj_NewWeaKRef(struct BifrostVM* self, void* data)
{
  BifrostObjWeakRef* obj = AllocateVMObject(BifrostObjWeakRef, self, BIFROST_VM_OBJ_WEAK_REF);

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
      return sizeof(BifrostObjNativeFn) + ((const BifrostObjNativeFn*)obj)->num_statics * sizeof(bfVMValue) + ((BifrostObjNativeFn*)obj)->extra_data_size;
    }
    case BIFROST_VM_OBJ_STRING:
    {
      return sizeof(BifrostObjStr);
    }
    case BIFROST_VM_OBJ_REFERENCE:
    {
      return sizeof(BifrostObjReference) + ((const BifrostObjReference*)obj)->extra_data_size;
    }
    case BIFROST_VM_OBJ_WEAK_REF:
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

      bfVMString_delete(self, fn->name);
      bfVMArray_delete(self, &fn->constants);
      bfVMArray_delete(self, &fn->instructions);
      bfVMArray_delete(self, &fn->code_to_line);
      break;
    }
    case BIFROST_VM_OBJ_NATIVE_FN:
    {
      break;
    }
    case BIFROST_VM_OBJ_STRING:
    {
      BifrostObjStr* const str = (BifrostObjStr*)obj;
      bfVMString_delete(self, str->value);
      break;
    }
    case BIFROST_VM_OBJ_REFERENCE:
    {
      break;
    }
    case BIFROST_VM_OBJ_WEAK_REF:
    {
      break;
    }
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
  else if (obj->type == BIFROST_VM_OBJ_REFERENCE)
  {
    BifrostObjReference* ref = (BifrostObjReference*)obj;

    if (ref->clz->finalizer)
    {
      ref->clz->finalizer(self, &ref->extra_data);
    }
  }
}

/* array */

typedef struct
{
  size_t      stride;
  const void* key;

} ArrayDefaultCompareData;

#define SELF_CAST(s) ((unsigned char**)(s))

typedef struct
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

// bfVMArray_push
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

void bfVMArray_push(struct BifrostVM* vm, void* const self, const void* const data)
{
  const size_t stride = Array_getHeader(*SELF_CAST(self))->stride;

  Array_reserve(vm, self, bfVMArray_size(self) + 1);

  LibC_memcpy(Array_end(self), data, stride);
  ++Array_getHeader(*SELF_CAST(self))->size;
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

void* bfVMArray_at(const void* const self, const size_t index)
{
  return *(char**)self + (Array_getHeader(*SELF_CAST(self))->stride * index);
}

void* bfVMArray_pop(void* const self)
{
  LibC_assert(bfVMArray_size(self) != 0, "Array_pop:: attempt to pop empty array");

  BifrostArrayHeader* const header      = Array_getHeader(*SELF_CAST(self));
  void* const               old_element = bfVMArray_at(self, header->size - 1);
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
  vm->gc_is_running = true;

  BifrostArrayHeader* const header = Array_getHeader(*SELF_CAST(self));

  bfGC_AllocMemory(vm, header, ArrayAllocationSize(header->capacity, header->stride), 0u);

  vm->gc_is_running = false;
}

/* string */

typedef struct BifrostStringHeader
{
  size_t capacity;
  size_t length;

} BifrostStringHeader;

BifrostStringHeader* bfVMString_getHeader(ConstBifrostString self);

static size_t StringAllocationSize(size_t capacity)
{
  return sizeof(BifrostStringHeader) + capacity;
}

BifrostString bfVMString_newLen(struct BifrostVM* vm, const char* initial_data, size_t string_length)
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

const char* bfVMString_cstr(ConstBifrostString self)
{
  return self;
}

size_t bfVMString_length(ConstBifrostString self)
{
  return bfVMString_getHeader(self)->length;
}

void bfVMString_reserve(struct BifrostVM* vm, BifrostString* self, size_t new_capacity)
{
  BifrostStringHeader* header = bfVMString_getHeader(*self);

  if (new_capacity > header->capacity)
  {
    const size_t old_capacity = header->capacity;

    while (header->capacity < new_capacity)
    {
      header->capacity *= 2;
    }

    vm->gc_is_running = true;

    header = (BifrostStringHeader*)bfGC_AllocMemory(vm, header, StringAllocationSize(old_capacity), StringAllocationSize(header->capacity));

    if (header)
    {
      *self = (char*)header + sizeof(BifrostStringHeader);
    }
    else
    {
      bfVMString_delete(vm, *self);
      *self = NULL;
    }

    vm->gc_is_running = false;
  }
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

void bfVMString_unescape(BifrostString self)
{
  bfVMString_getHeader(self)->length = CString_unescape(self);
}

int bfVMString_cmp(ConstBifrostString self, ConstBifrostString other)
{
  const size_t len1 = bfVMString_length(self);
  const size_t len2 = bfVMString_length(other);

  if (len1 != len2)
  {
    return -1;
  }

  return LibC_strncmp(self, other, len1);
}

int bfVMString_ccmpn(ConstBifrostString self, const char* other, size_t length)
{
  if (length > bfVMString_length(self))
  {
    return -1;
  }

  return LibC_strncmp(bfVMString_cstr(self), other, length);
}

uint32_t bfVMString_hash(const char* str)
{
  uint32_t hash = 0x811c9dc5;

  while (*str)
  {
    hash ^= (unsigned char)*str++;
    hash *= 0x01000193;
  }

  return hash;
}

uint32_t bfVMString_hashN(const char* str, size_t length)
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

void bfVMString_delete(struct BifrostVM* vm, BifrostString self)
{
  vm->gc_is_running = true;

  BifrostStringHeader* const header = bfVMString_getHeader(self);

  bfGC_AllocMemory(vm, header, StringAllocationSize(header->capacity), 0u);

  vm->gc_is_running = false;
}

BifrostStringHeader* bfVMString_getHeader(ConstBifrostString self)
{
  return ((BifrostStringHeader*)(self)) - 1;
}
