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
  BIFROST_VM_OBJ_FUNCTION,   // 0b000
  BIFROST_VM_OBJ_MODULE,     // 0b001
  BIFROST_VM_OBJ_CLASS,      // 0b010
  BIFROST_VM_OBJ_INSTANCE,   // 0b011
  BIFROST_VM_OBJ_STRING,     // 0b100
  BIFROST_VM_OBJ_NATIVE_FN,  // 0b101
  BIFROST_VM_OBJ_REFERENCE,  // 0b110
  BIFROST_VM_OBJ_WEAK_REF,   // 0b111

} BifrostObjType;

#define BifrostVMObjType_mask 0x7 /*!< 0b111 */

typedef struct BifrostVMSymbol
{
  ConstBifrostString name;  /*!< Non owning string, [BifrostVM::symbols] is the owner. */
  BifrostValue       value; /*!< The associated value.                                 */

} BifrostVMSymbol;

typedef struct BifrostObj
{
  BifrostObjType     type;
  unsigned char      gc_mark;
  struct BifrostObj* next;

} BifrostObj;

typedef struct BifrostObjFn
{
  BifrostObj               super;
  BifrostString            name;
  int32_t                  arity;  //!< An arity of -1 indicates variadic args [0, 512).
  uint16_t*                code_to_line;
  BifrostValue*            constants;
  bfInstruction*           instructions;
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
  BifrostObjModule*       module;
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

typedef struct BifrostObjStr
{
  BifrostObj    super;
  BifrostString value;
  unsigned      hash;

} BifrostObjStr;

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
  BifrostObjFn*  fn;        /*!< Needed for addition debug info for stack traces, NULL for native functions. */
  bfInstruction* ip;        /*!< The current instruction being executed.                                     */
  size_t         old_stack; /*!< The top of the stack to restore to.                                         */
  size_t         stack;     /*!< The place where this stacks locals start.                                   */

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

#define BIFROST_ARRAY_INVALID_INDEX           ((size_t)(-1))
#define bfVMArray_new(vm, T, initial_size)    (T*)_bfVMArrayT_new((vm), sizeof(T), (initial_size))
#define bfVMArray_newA(vm, arr, initial_size) _bfVMArrayT_new((vm), sizeof((arr)[0]), (initial_size))

typedef int (*bfVMArrayFindCompare)(const void*, const void*);

void*  _bfVMArrayT_new(struct BifrostVM* vm, const size_t stride, const size_t initial_size);
size_t bfVMArray_size(const void* const self);
void*  bfVMArray_at(const void* const self, const size_t index);
void   bfVMArray_resize(struct BifrostVM* vm, void* const self, const size_t size);
void*  bfVMArray_emplace(struct BifrostVM* vm, void* const self);
void*  bfVMArray_emplaceN(struct BifrostVM* vm, void* const self, const size_t num_elements);
void*  bfVMArray_pop(void* const self);
void*  bfVMArray_back(const void* const self);
void   bfVMArray_clear(void* const self);
void   bfVMArray_push(struct BifrostVM* vm, void* const self, const void* const data);
void   bfVMArray_delete(struct BifrostVM* vm, void* const self);

/* string */

BifrostString bfVMString_newLen(struct BifrostVM* vm, const char* initial_data, size_t string_length);
const char*   bfVMString_cstr(ConstBifrostString self);
size_t        bfVMString_length(ConstBifrostString self);
void          bfVMString_reserve(struct BifrostVM* vm, BifrostString* self, size_t new_capacity);
void          bfVMString_sprintf(struct BifrostVM* vm, BifrostString* self, const char* format, ...);
void          bfVMString_unescape(BifrostString self);
int           bfVMString_cmp(ConstBifrostString self, ConstBifrostString other);
int           bfVMString_ccmpn(ConstBifrostString self, const char* other, size_t length);
uint32_t      bfVMString_hash(const char* str);
uint32_t      bfVMString_hashN(const char* str, size_t length);
void          bfVMString_delete(struct BifrostVM* vm, BifrostString self);

/* hash-map */

typedef struct bfHashMapIter
{
  const void* key;
  void*       value;
  int         index;
  bfHashNode* next;

} bfHashMapIter;

/*
    The defaults are the following.

    hash       - Assumes the keys are nul terminated strings. So if you use a
                  different data-type you MUST pass in a valid hash function.

    cmp        - Like [hash] assumes a nul terminated string and will compare each
                  character. So if you use a different data-type you MUST pass in a
                  valid compare function.

    value_size - By default is the size of a pointer.
  */
void bfHashMapParams_init(BifrostHashMapParams* self, struct BifrostVM* vm);

void          bfHashMap_ctor(BifrostHashMap* self, const BifrostHashMapParams* params);
void          bfHashMap_set(BifrostHashMap* self, const void* key, void* value);
void*         bfHashMap_get(BifrostHashMap* self, const void* key);
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