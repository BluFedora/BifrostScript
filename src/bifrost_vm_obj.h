/******************************************************************************/
/*!
 * @file   bifrost_vm_obj.h
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
#ifndef BIFROST_VM_OBJ_H
#define BIFROST_VM_OBJ_H

#include "bifrost/bifrost_vm.h" /* bfNativeFnT, bfClassFinalizer, BifrostValue, BifrostHashMap */

#include "bifrost_vm_instruction_op.h"  // bfInstruction
#include "bifrost_vm_lexer.h"           // string_range

#if __cplusplus
extern "C" {
#endif

#define bf_flex_array_member  //!< C99 Feature, this project does not compile in C++ mode.

typedef enum BifrostObjType
{
  BIFROST_VM_OBJ_FUNCTION,         // 0b000
  BIFROST_VM_OBJ_MODULE,           // 0b001
  BIFROST_VM_OBJ_CLASS,            // 0b010
  BIFROST_VM_OBJ_INSTANCE,         // 0b011
  BIFROST_VM_OBJ_STRING,           // 0b100
  BIFROST_VM_OBJ_NATIVE_FN,        // 0b101
  BIFROST_VM_OBJ_NATIVE_INSTANCE,  // 0b110
  BIFROST_VM_OBJ_NATIVE_WEAK_REF,  // 0b111

} BifrostObjType;

#define BifrostVMObjType_mask 0x7 /*!< 0b111 */

typedef struct BifrostVMSymbol
{
  const BifrostObjStr* name;  /*!< [BifrostVM::symbols] is the owner. */
  BifrostValue         value; /*!< The associated value.              */

} BifrostVMSymbol;

typedef struct BifrostObj
{
  BifrostObjType     type;
  unsigned char      gc_mark;
  struct BifrostObj* next;

} BifrostObj;

typedef struct BifrostObjStr
{
  BifrostObj    super;
  BifrostString value;
  // char*         str;
  // uint32_t      capacity;
  // uint32_t      length;
  uint32_t hash;

} BifrostObjStr;

typedef struct BifrostObjFn
{
  BifrostObj               super;
  BifrostObjStr*           name;
  int32_t                  arity;  //!< An arity of -1 indicates variadic args [0, 512).
  uint16_t*                code_to_line;
  BifrostValue*            constants;
  const bfInstruction*     instructions;
  size_t                   needed_stack_space; /* params + locals + temps */
  struct BifrostObjModule* module;

} BifrostObjFn;

typedef struct BifrostObjModule
{
  BifrostObj       super;
  BifrostString    name;
  BifrostVMSymbol* variables;
  BifrostObjFn     init_fn;

} BifrostObjModule;

typedef struct BifrostObjClass
{
  BifrostObj              super;
  BifrostString           name;
  struct BifrostObjClass* base_clz;
  BifrostObjModule*       module; // TODO(SR): Remove me, only needed for dumb API decision....
  BifrostVMSymbol*        symbols;
  BifrostVMSymbol*        field_initializers;
  size_t                  extra_data;
  bfClassFinalizer        finalizer;

} BifrostObjClass;

#define INSTANCE_HEADER   \
  BifrostObj       super; \
  BifrostObjClass* clz  // Optional

typedef struct BifrostObjInstance
{
  INSTANCE_HEADER;
  BifrostHashMap fields;                           // <ConstBifrostString (Non owning string, [BifrostVM::symbols] is the owner), BifrostValue>
  char           extra_data[bf_flex_array_member]; /* This is for native class data. */

} BifrostObjInstance;

typedef struct BifrostObjNativeFn
{
  BifrostObj    super;
  bfNativeFnT   value;
  int32_t       arity;
  uint32_t      num_statics;
  BifrostValue* statics;
  uint16_t      extra_data_size;
  char          extra_data[bf_flex_array_member]; /* This is for native data. */

} BifrostObjNativeFn;

typedef struct BifrostObjReference
{
  INSTANCE_HEADER;
  size_t extra_data_size;
  char   extra_data[bf_flex_array_member]; /* This is for native data. */

} BifrostObjReference;

typedef struct BifrostObjWeakRef
{
  INSTANCE_HEADER;
  void* data;

} BifrostObjWeakRef;

typedef struct BifrostVMStackFrame
{
  BifrostObjFn*        fn;        /*!< Needed for stack traces, NULL for native functions. */
  const bfInstruction* ip;        /*!< The current instruction being executed.             */
  size_t               old_stack; /*!< The top of the stack to restore to.                 */
  size_t               stack;     /*!< The place where this stacks locals start.           */

} BifrostVMStackFrame;

#undef INSTANCE_HEADER

#define BIFROST_AS_OBJ(value) ((BifrostObj*)bfVMValue_asPointer((value)))

BifrostObjModule*    bfObj_NewModule(struct BifrostVM* self, string_range name);
BifrostObjClass*     bfObj_NewClass(struct BifrostVM* self, BifrostObjModule* module, string_range name, BifrostObjClass* base_clz, size_t extra_data);
BifrostObjInstance*  bfObj_NewInstance(struct BifrostVM* self, BifrostObjClass* clz);
BifrostObjFn*        bfObj_NewFunction(struct BifrostVM* self, BifrostObjModule* module);
BifrostObjNativeFn*  bfObj_NewNativeFn(struct BifrostVM* self, bfNativeFnT fn_ptr, int32_t arity, uint32_t num_statics, uint16_t extra_data);
BifrostObjStr*       bfObj_NewString(struct BifrostVM* self, string_range value);
BifrostObjReference* bfObj_NewReference(struct BifrostVM* self, size_t extra_data_size);
BifrostObjWeakRef*   bfObj_NewWeaKRef(struct BifrostVM* self, void* data);
size_t               bfObj_AllocationSize(const BifrostObj* obj);
void                 bfObj_Destruct(struct BifrostVM* self, BifrostObj* obj);
size_t               bfObj_Delete(struct BifrostVM* self, BifrostObj* obj);
bool                 bfObj_IsFunction(const BifrostObj* obj);
void                 bfObj_Finalize(struct BifrostVM* self, BifrostObj* obj);

/* array */

#define bfVMArray_new(vm, arr, initial_size) _bfVMArrayT_new((vm), sizeof((arr)[0]), (initial_size))

void*  _bfVMArrayT_new(struct BifrostVM* vm, const size_t stride, const size_t initial_size);
size_t bfVMArray_size(const void* const self);
void   bfVMArray_resize(struct BifrostVM* vm, void* const self, const size_t size);
void*  bfVMArray_emplace(struct BifrostVM* vm, void* const self);
void*  bfVMArray_emplaceN(struct BifrostVM* vm, void* const self, const size_t num_elements);
void*  bfVMArray_pop(void* const self);
void*  bfVMArray_back(const void* const self);
void   bfVMArray_clear(void* const self);
void   bfVMArray_delete(struct BifrostVM* vm, void* const self);

/* string */

typedef struct StringCmp
{
  const char* str;
  uint32_t    length;
  uint32_t    hash;

} StringCmp;

StringCmp StringCmp_Make(const char* const str, const size_t length);
StringCmp StringCmp_FromStr(ConstBifrostString self);
StringCmp StringCmp_FromBStr(const BifrostObjStr* self);
StringCmp StringCmp_FromStrView(const string_range self);
bool      StringCmp_Cmp(const StringCmp lhs, const StringCmp rhs);

void bfVMString_reserve(struct BifrostVM* vm, BifrostString* self, size_t new_capacity);
void bfVMString_sprintf(struct BifrostVM* vm, BifrostString* self, const char* format, ...);

size_t       BifrostString_length(const BifrostObjStr* self);
string_range BifrostString_AsStrRng2(const BifrostString self);

inline string_range BifrostString_AsStrRng(const BifrostObjStr* self)
{
  return MakeStringLen(self->value, BifrostString_length(self));
}

/* hash-map */

typedef struct bfHashMapIter
{
  const BifrostObjStr* key;
  BifrostValue         value;
  int                  index;
  bfHashNode*          next;

} bfHashMapIter;

void          bfHashMap_ctor(BifrostHashMap* self, struct BifrostVM* vm);
void          bfHashMap_set(BifrostHashMap* self, const BifrostObjStr* key, const BifrostValue value);
BifrostValue  bfHashMap_get(BifrostHashMap* self, const BifrostObjStr* key);
int           bfHashMap_removeCmp(BifrostHashMap* self, const void* key, bfHashMapCmp cmp);  // 'key' is the first param for 'cmp'
bfHashMapIter bfHashMap_itBegin(const BifrostHashMap* self);
int           bfHashMap_itIsValid(const bfHashMapIter* it);
void          bfHashMap_itGetNext(const BifrostHashMap* self, bfHashMapIter* it);
void          bfHashMap_clear(BifrostHashMap* self);
void          bfHashMap_dtor(BifrostHashMap* self);

#define bfHashMapFor(it, map)                     \
  for (bfHashMapIter it = bfHashMap_itBegin(map); \
       bfHashMap_itIsValid(&(it));                \
       bfHashMap_itGetNext(map, &(it)))

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_OBJ_H */