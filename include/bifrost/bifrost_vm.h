/******************************************************************************/
/*!
 * @file   bifrost_vm.h
 * @author Shareef Raheem (http://blufedora.github.io)
 * @par
 *    Bifrost Scripting Language\n
 *    Dependencies:             \n
 *      > C99 or later.         \n
 *      > C Runtime Library     \n
 *
 * @brief
 *  The main API for the Bifrost Scripting Language.
 *
 * @copyright Copyright (c) 2019-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_API_H
#define BIFROST_VM_API_H

#include <stdbool.h> /* bool, true, false */
#include <stdint.h>  /* uint64_t          */

#if __cplusplus
extern "C" {
#endif

// clang-format off
#if defined(BF_VM_EXPORT_STATIC)

    #define BF_VM_API /* empty */

#elif defined(_WIN32) || defined(_WIN64)

    #if defined(BF_VM_EXPORT_DYNAMIC)
        #define BF_VM_API __declspec(dllexport)
    #else
        #define BF_VM_API __declspec(dllimport)
    #endif

#else

    #if __GNUC__ >= 4
        #define BF_VM_API __attribute__((visibility("default")))
    #else
        #define BF_VM_API
    #endif

#endif
// clang-format on

/* Forward Declarations */
typedef struct BifrostParser       BifrostParser;
typedef struct BifrostVMStackFrame BifrostVMStackFrame;
typedef struct BifrostObj          BifrostObj;
typedef struct BifrostObjInstance  BifrostObjInstance;
typedef struct BifrostObjModule    BifrostObjModule;
typedef struct BifrostObjNativeFn  BifrostObjNativeFn;
typedef struct BifrostVM           BifrostVM;
typedef struct bfValueHandleImpl*  bfValueHandle; /*!< An opaque handle to a VM Value to keep it alive from the GC. */

typedef uint64_t bfVMValue; /*!< The Nan-Tagged value representation of this scripting language. */

#define InvalidDefaultCase \
  default: break;

/// NOTE(Shareef):
///   The memory layout: [BifrostStringHeader (capacity | length) | BifrostString (char*)]
///   A 'BifrostString' can be used anywhere a 'normal' C string can be used.
typedef char*       BifrostString;
typedef const char* ConstBifrostString;

/*!
 * @brief
 *   Signature of a native C function the vm can call.
 */
typedef void (*bfNativeFnT)(BifrostVM* vm, int32_t num_args);

/*!
 * @brief
 *   An optional destructor function for classes.
 */
typedef void (*bfClassFinalizer)(BifrostVM* vm, void* instance);

typedef enum BifrostVMError
{
  BIFROST_VM_ERROR_NONE,                    /*!< NONE       */
  BIFROST_VM_ERROR_OUT_OF_MEMORY,           /*!< ANYONE     */
  BIFROST_VM_ERROR_RUNTIME,                 /*!< VM Runtime */
  BIFROST_VM_ERROR_LEXER,                   /*!< Lexer      */
  BIFROST_VM_ERROR_COMPILE,                 /*!< Parser     */
  BIFROST_VM_ERROR_FUNCTION_ARITY_MISMATCH, /*!< VM         */
  BIFROST_VM_ERROR_MODULE_ALREADY_DEFINED,  /*!< VM         */
  BIFROST_VM_ERROR_MODULE_NOT_FOUND,        /*!< VM         */
  BIFROST_VM_ERROR_INVALID_OP_ON_TYPE,      /*!< VM         */
  BIFROST_VM_ERROR_INVALID_ARGUMENT,        /*!< VM         */
  BIFROST_VM_ERROR_STACK_TRACE_BEGIN,       /*!< VM Runtime */
  BIFROST_VM_ERROR_STACK_TRACE,             /*!< VM Runtime */
  BIFROST_VM_ERROR_STACK_TRACE_END,         /*!< VM Runtime */

} BifrostVMError;

typedef enum BifrostVMStandardModule
{
  BIFROST_VM_STD_MODULE_IO          = (1 << 0), /*!< "std:io"          */
  BIFROST_VM_STD_MODULE_MEMORY      = (1 << 1), /*!< "std:memory"      */
  BIFROST_VM_STD_MODULE_FUNCTIONAL  = (1 << 2), /*!< "std:functional"  */
  BIFROST_VM_STD_MODULE_COLLECTIONS = (1 << 3), /*!< "std:collections" */
#define BIFROST_VM_STD_MODULE_ALL 0xFFFFFFFF    /*!< "std:*"           */

} BifrostVMStandardModule;

typedef enum BifrostVMType
{
  BIFROST_VM_STRING,   /*!< String value                                                               */
  BIFROST_VM_NUMBER,   /*!< Number value                                                               */
  BIFROST_VM_BOOL,     /*!< Boolean value                                                              */
  BIFROST_VM_NIL,      /*!< null value                                                                 */
  BIFROST_VM_OBJECT,   /*!< Any type of object, both weak and strong instances are considered objects. */
  BIFROST_VM_FUNCTION, /*!< A function object, both native and script defined.                         */
  BIFROST_VM_MODULE,   /*!< A vm module                                                                */

} BifrostVMType; /*!< The type of object stored in the vm at a place in memory. */

typedef struct BifrostMethodBind
{
  const char* name;        /*!< The name of the method. (NUL terminated)                                     */
  bfNativeFnT fn;          /*!< The function to call.                                                        */
  int32_t     arity;       /*!< Number of parameters the function expects or -1 if any number of parameters. */
  uint32_t    num_statics; /*!< The number of slots for static variables the vm will reserved for you .      */
  uint16_t    extra_data;  /*!< The number of bytes the vm will give you for user data storage.              */

} BifrostMethodBind; /*!< Definition of a class method function. */

/*!
 * @brief
 *   Creates a definition of a class method with the passed parameters.
 *
 * @param name
 *   The name of the function that will be binded to the class. [BifrostMethodBind::name]
 *
 * @param func
 *   The native function to be called by an instance of the class. [BifrostMethodBind::fn]
 *
 * @param arity
 *   The number of arguments the \p func expects to take in. use -1 if you take in a variable
 *   number of arguments.
 *
 * @param num_statics
 *   The number of static slots this method wants to have access to.
 *
 * @param extra_data
 *   The number of bytes to allocate for user data purposes.
 *
 * @return BifrostMethodBind
 *   A valid BifrostMethodBind object for use in an array of [BifrostVMClassBind::methods].
 */
BF_VM_API BifrostMethodBind bfMethodBind_make(const char* name, bfNativeFnT func, int32_t arity, uint32_t num_statics, uint16_t extra_data);

/*!
 * @brief
 *   This should be used as the last element of [BifrostVMClassBind::methods] since
 *   we do not take in the explicit size of the array.
 *
 * @return BifrostMethodBind
 *   A nulled out BifrostMethodBind indicating the end of the [BifrostVMClassBind::methods] array.
 */
BF_VM_API BifrostMethodBind bfMethodBind_end(void);

typedef struct BifrostVMClassBind
{
  const char*              name;            /*!< The name of the class to bind.                                                            */
  size_t                   extra_data_size; /*!< Number of bytes to allocate towards userdata.                                             */
  const BifrostMethodBind* methods;         /*!< The methods to binded to this class. use 'bfMethodBind_end()' as the last element.        */
  bfClassFinalizer         finalizer;       /*!< Optional finalizer method, can be null. Called at the end of a class's instance lifetime. */

} BifrostVMClassBind; /*!< Definition of a vm class. */

/*!
 * @brief
 *  If the source is NULL then it is assumed the module could not be found.
 *  and an appropriate error will be issued.
 */
typedef struct BifrostVMModuleLookUp
{
  const char* source;     /*!< Must have been allocated by the same allocator as the vm's one. (BifrostVMParams::memory_fn) */
  size_t      source_len; /*!< The number of bytes to used by [BifrostVMModuleLookUp::source]                               */

} BifrostVMModuleLookUp;

/*!
 * @brief
 *   The definition of the function that will be called on an error.
 */
typedef void (*bfErrorFn)(BifrostVM* vm, BifrostVMError err, int line_no, const char* message);

/*!
 * @brief
 *   The definition of the function that will be called when a script tried to print to the screen.
 */
typedef void (*bfPrintFn)(BifrostVM* vm, const char* message);

/*!
 * @brief
 *   The [BifrostVMModuleLookUp::source] field must be allocated from the same allocator
 *   that was passed in as [BifrostVMParams::memory_fn].
 */
typedef void (*bfModuleFn)(BifrostVM* vm, const char* from, const char* module, BifrostVMModuleLookUp* out);

/*!
 * @brief
 *   If old_size is 0u / ptr == NULL : Act as Malloc.\n
 *   If new_size == 0u               : Act as Free.\n
 *   Otherwise                       : Act as Realloc.\n
 */
typedef void* (*bfMemoryFn)(void* user_data, void* ptr, size_t old_size, size_t new_size);

typedef struct BifrostVMParams
{
  bfErrorFn  error_fn;           /*!< The callback for anytime an error occurs.                                                              */
  bfPrintFn  print_fn;           /*!< The callback for when a script tried to print a message.                                               */
  bfModuleFn module_fn;          /*!< The callback for attempting to load a non std:* module.                                                */
  bfMemoryFn memory_fn;          /*!< The callback for the vm asking for memory.                                                             */
  size_t     min_heap_size;      /*!< The minimum size of the virtual heap must be at all times.                                             */
  size_t     heap_size;          /*!< The starting heap size. Must be greater or equal to [BifrostVMParams::min_heap_size].                  */
  float      heap_growth_factor; /*!< The percent amount to grow the size of the virtual heap before calling the GC again. (Ex: 0.5f = x1.5) */
  void*      user_data;          /*!< The user_data for the memory allocation callback.                                                      */

} BifrostVMParams; /*! The parameters in which to initialize a BifrostVM. */

/*!
 * @brief
 *  Initializes \p self to some these defaults:
 *    self->error_fn           = NULL;                 - errors will have to be check with return values and 'bfVM_errorString'
 *    self->print_fn           = NULL;                 - 'print' will be a no op.
 *    self->module_fn          = NULL;                 - unable to load user modules
 *    self->memory_fn          = bfGCDefaultAllocator; - uses c library's realloc and free by default
 *    self->min_heap_size      = 1000000;              - 1mb
 *    self->heap_size          = 5242880;              - 5mb
 *    self->heap_growth_factor = 0.5f;                 - Grow by x1.5
 *    self->user_data          = NULL;                 - User data for the memory allocator, and maybe other future things.
 *
 * @param self
 *   The BifrostVMParams to initialize to reasonable defaults.
 */
BF_VM_API void bfVMParams_initDefault(BifrostVMParams* self);

typedef enum BifrostVMBuildInSymbol
{
  BIFROST_VM_SYMBOL_CTOR, /*! Symbol the default class constructor method.    */
  BIFROST_VM_SYMBOL_DTOR, /*! Symbol the class destructor method.             */
  BIFROST_VM_SYMBOL_CALL, /*! The call operator for class instances.          */
  BIFROST_VM_SYMBOL_MAX,  /*! For being able to loop through builtin symbols. */

} BifrostVMBuildInSymbol; /*! Common symbols that need to have fast lookup. */

#define BIFROST_HASH_MAP_BUCKET_SIZE 128

typedef unsigned (*bfHashMapHash)(const void* key);
typedef int (*bfHashMapCmp)(const void* lhs, const void* rhs);

typedef struct bfHashNode bfHashNode;

typedef struct BifrostHashMapParams
{
  struct BifrostVM* vm;
  bfHashMapHash     hash;
  bfHashMapCmp      cmp;
  size_t            value_size;

} BifrostHashMapParams;

typedef struct BifrostHashMap
{
  BifrostHashMapParams params;
  bfHashNode*          buckets[BIFROST_HASH_MAP_BUCKET_SIZE];
  unsigned             num_buckets;

} BifrostHashMap;

/*!
 * @brief
 *   The self contained virtual machine for the Bifrost
 *   scripting language.
 *
 *   Consider all these member variables private. They
 *   are exposed so that you may declare a VM on the stack.
 *
 *   If you want ABI compatibility use 'bfVM_new' and 'bfVM_delete'
 *   and do not use this struct directly.
 */
struct BifrostVM
{
  BifrostVMParams      params;                                  /*!< The user defined parameters used by the VM                                     */
  BifrostVMStackFrame* frames;                                  /*!< The call stack.                                                                */
  bfVMValue*           stack;                                   /*!< The base pointer to the stack memory.                                          */
  bfVMValue*           stack_top;                               /*!< The usable top of the [BifrostVM::stack].                                      */
  BifrostString*       symbols;                                 /*!< Every symbol ever used in the vm, a 'perfect hash'.                            */
  BifrostObj*          gc_object_list;                          /*!< The list of every object allocated by this VM.                                 */
  BifrostHashMap       modules;                                 /*!< <BifrostObjStr, BifrostObjModule*> for fast module lookup                      */
  BifrostParser*       parser_stack;                            /*!< For handling the recursive nature of importing modules.                        */
  bfValueHandle        handles;                                 /*!< Additional GC Roots for Extended C Lifetimes                                   */
  bfValueHandle        free_handles;                            /*!< A pool of handles for reduced allocations.                                     */
  BifrostString        last_error;                              /*!< The last error to happen in a user readable way                                */
  size_t               bytes_allocated;                         /*!< The total amount of memory this VM has asked for                               */
  BifrostObj*          finalized;                               /*!< Objects that have finalized but still need to be freed                         */
  BifrostObj*          temp_roots[8];                           /*!< Objects temporarily protected from the GC                                      */
  uint8_t              temp_roots_top;                          /*!< BifrostVM::temp_roots size                                                     */
  bool                 gc_is_running;                           /*!< This is so that when calling a finalizer the GC isn't run.                     */
  uint32_t             build_in_symbols[BIFROST_VM_SYMBOL_MAX]; /*!< Symbols that should be loaded at startup for a faster runtime.                 */
  BifrostObjNativeFn*  current_native_fn;                       /*!< The currently executing native function for accessing of userdata and statics. */
};

/*!
 * @brief
 *   If you already have a block of memory of size 'sizeof(BifrostVM)'
 *   this basically 'placement new's into the passed in block.
 *
 * @param self
 *   The block of memory to create the vm in.
 *
 * @param params
 *   The customization points for the virtual machine.
 */
BF_VM_API void bfVM_ctor(BifrostVM* self, const BifrostVMParams* params);

/*!
 * @brief
 *   Returns the user data.
 *
 * @param self
 *   The vm from which you want the user data from.
 *
 * @return void*
 *   The user data pointer.
 */
BF_VM_API void* bfVM_userData(const BifrostVM* self);

/*!
 * @brief
 *   Creates a new module.
 *
 * @param self
 *   The vm to create the module in.
 *
 * @param idx
 *    The index on the stack to write the module to.
 *
 * @param module
 *    The name of the module.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_MODULE_ALREADY_DEFINED - The module with that name has already been defined.
 */
BF_VM_API BifrostVMError bfVM_moduleMake(BifrostVM* self, size_t idx, const char* module);

/*!
 * @brief
 *   Loads up standard module(s) into the vm.
 *   All modules loaded by this function are prefixed with 'std:'.
 *
 * @param self
 *  The vm to load the module in.
 *
 * @param idx
 *  Where to put the module. This function can be called multiple
 *  times with the same parameters to just grab the standard module.
 *
 * @param module_flags
 *  Must be a valid set of bits from 'BifrostVMStandardModule'.
 */
BF_VM_API void bfVM_moduleLoadStd(BifrostVM* self, size_t idx, uint32_t module_flags);

/*!
 * @brief
 *   Loads a module named \p module into slot \p idx.
 *
 * @param self
 *   The vm to load the module in.
 *
 * @param idx
 *   Where to put the module in the API stack.
 *
 * @param module
 *   The name of the module to load.
 *
 * @param module_name_len
 *   Length of the module name.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_MODULE_NOT_FOUND - The module could not be found.
 */
BF_VM_API BifrostVMError bfVM_moduleLoad(BifrostVM* self, size_t idx, const char* module, const size_t module_name_len);

/*!
 * @brief
 *   Unloads a module of name _ \p module_.
 *   Use this method to either save memory or reload a module.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param module
 *   The name of the module to unload.
 *
 * @param module_name_len
 *   Length of the module name.
 */
BF_VM_API void bfVM_moduleUnload(BifrostVM* self, const char* module, const size_t module_name_len);

/*!
 * @brief
 *   Purges all loaded modules from the vm.
 *
 * @param self
 *   The vm to operate on.
 */
BF_VM_API void bfVM_moduleUnloadAll(BifrostVM* self);

/*!
 * @brief
 *   Returns the number of slots you are allowed to access
 *   in the API stack.
 *
 * @param self
 *   The vm to operate on.
 *
 * @return size_t
 *   The number of slots you may access from the API.
 */
BF_VM_API size_t bfVM_stackSize(const BifrostVM* self);

/*!
 * @brief
 *   Resizes the API stack to \p size.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param size
 *   The new size of the API stack.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_OUT_OF_MEMORY - Failed to resize the API stack.
 */
BF_VM_API BifrostVMError bfVM_stackResize(BifrostVM* self, size_t size);

/*!
 * @brief
 *   Creates an instance of the class at \p clz_idx and stores it in \p dst_idx.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param clz_idx
 *   The index of the class to create the instance from.
 *
 * @param dst_idx
 *   Where the newly created instance will be stored.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - The value at \p clz_idx is either not an object or a class.
 *   BIFROST_VM_ERROR_OUT_OF_MEMORY      - Failed to get memory for the new instance.
 */
BF_VM_API BifrostVMError bfVM_stackMakeInstance(BifrostVM* self, size_t clz_idx, size_t dst_idx);

/*!
 * @brief
 *   Creates a native object at index and returns the allocated memory.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param idx
 *   The destination to where the reference object will be created.
 *
 * @param extra_data_size
 *   The size of the user_data block the returned pointer will pointer to.
 *
 * @return void*
 *   NULL      - If the memory was not able to be acquired for the reference.
 *   Otherwise - A pointer of pointing to \p extra_data_size bytes of memory.
 */
BF_VM_API void* bfVM_stackMakeReference(BifrostVM* self, size_t idx, size_t extra_data_size);

/*!
 * @brief
 *   Creates a reference object with a custom class binding.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param module_idx
 *   The module to define the new class in.
 *
 * @param clz_bind
 *   The class binding definition to add to \p module_idx and assigns it to the new reference object.
 *
 * @param dst_idx
 *   The index to store the new object in.
 *
 * @return void*
 *   NULL      - If the memory was not able to be acquired for the reference.
 *   Otherwise - A pointer of pointing to clz_bind->extra_data_size bytes of memory.
 */
BF_VM_API void* bfVM_stackMakeReferenceClz(BifrostVM*                self,
                                           size_t                    module_idx,
                                           const BifrostVMClassBind* clz_bind,
                                           size_t                    dst_idx);

/*!
 * @brief
 *   Creates a weak reference object ans stores it in \p idx.
 *   A weak reference just stores a pointer so make sure the lifetime
 *   of \p value outlives all uses of this object.
 *
 *   TODO: Return error for memory alloc failure.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param idx
 *   The index to store the newly created object.
 *
 * @param value
 *   The pointer to be stored in this weak reference.
 */
BF_VM_API void bfVM_stackMakeWeakRef(BifrostVM* self, size_t idx, void* value);

/*!
 * @brief
 *   Sets the class of a reference at \p idx to the class at \p clz_idx.
 *
 *   TODO: Return error for invalid types.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param idx
 *   The index of the reference object.
 *
 * @param clz_idx
 *   The index of the class object.
 */
BF_VM_API void bfVM_referenceSetClass(BifrostVM* self, size_t idx, size_t clz_idx);

/*!
 * @brief
 *   Set the base class of \p idx to \p clz_index.
 *
 *   TODO: Return error for incorrect types used.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param idx
 *   The class that will have it's base class set.
 *
 * @param clz_idx
 *   The base class that will be assigned to \p idx.
 */
BF_VM_API void bfVM_classSetBaseClass(BifrostVM* self, size_t idx, size_t clz_idx);

/*!
 * @brief
 *   Loads a variable by string name of an instance, class, or module object.
 *   If variable is not found the BIFROST_VM_NIL is put in the slot.
 *
 * @param self
 *   The vm to operate on.
 *
 * @param dst_idx
 *   The location the variable will be loaded into.
 *
 * @param inst_or_class_or_module
 *   The index of the instance, class, or module.
 *
 * @param variable
 *   The name of the variable to load from the object at \p dst_idx.
 */
BF_VM_API void bfVM_stackLoadVariable(BifrostVM* self, size_t dst_idx, size_t inst_or_class_or_module, const char* variable);

/*!
 * @brief
 *   Stores into \p inst_or_class_or_module \p field = \p value_idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param inst_or_class_or_module
 *   The instance, class, or module to set the field of.
 *
 * @param field
 *   A nul terminated string specifying the name of the field to set in \p inst_or_class_or_module.
 *
 * @param value_idx
 *   The location of the value to write to \p inst_or_class_or_module's \p field.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - \p inst_or_class_or_module was not a valid object to set a field on.
 */
BF_VM_API BifrostVMError bfVM_stackStoreVariable(BifrostVM*  self,
                                                 size_t      inst_or_class_or_module,
                                                 const char* field,
                                                 size_t      value_idx);

/*!
 * @brief
 *   Creates a native function object and assigns it to \p inst_or_class_or_module \p field = \p value_idx.
 *   This function is a wrapper around 'bfVM_stackStoreClosure' but with no statics or userdata.
 *
 *   TODO: Return error for memory alloc failure.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param inst_or_class_or_module
 *   The instance, class, or module to set the field of.
 *
 * @param field
 *   A nul terminated string specifying the name of the field to set in \p inst_or_class_or_module.
 *
 * @param func
 *   The function to be binded.
 *
 * @param arity
 *   The number of arguments your function expects. Use -1 for variable number of arguments.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - \p inst_or_class_or_module was not a valid object to set a field on.
 */
BF_VM_API BifrostVMError bfVM_stackStoreNativeFn(BifrostVM*  self,
                                                 size_t      inst_or_class_or_module,
                                                 const char* field,
                                                 bfNativeFnT func,
                                                 int32_t     arity);

/*!
 * @brief
 *   Creates a native function object with more advanced parameters than 'bfVM_stackStoreNativeFn'.
 *
 *   TODO: Return error for memory alloc failure.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param inst_or_class_or_module
 *   The instance, class, or module to set the field of.
 *
 * @param field
 *   A nul terminated string specifying the name of the field to set in \p inst_or_class_or_module.
 *
 * @param func
 *   The function to be binded.
 *
 * @param arity
 *   The number of arguments your function expects. Use -1 for variable number of arguments.
 *
 * @param num_statics
 *   Number of static variable slots for the native function.
 *
 * @param extra_data
 *   The number of bytes for user_data storage.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - \p inst_or_class_or_module was not a valid object to set a field on.
 */
BF_VM_API BifrostVMError bfVM_stackStoreClosure(BifrostVM*  self,
                                                size_t      inst_or_class_or_module,
                                                const char* field,
                                                bfNativeFnT func,
                                                int32_t     arity,
                                                uint32_t    num_statics,
                                                uint16_t    extra_data);

/*!
 * @brief
 *   Gets the static value at \p static_idx from the currently run native function.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param dst_idx
 *   The location to receive the static variable in.
 *
 * @param static_idx
 *   The index of the static you want to get.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_ARGUMENT - There is no current native function.
 *                                     - \p static_idx is not a valid index.
 */
BF_VM_API BifrostVMError bfVM_closureGetStatic(BifrostVM* self, size_t dst_idx, size_t static_idx);

/*!
 * @brief
 *   Sets \p closure_idx 's static slot at \p static_idx to the value at \p value_idx.
 * @param self
 *   The vm that will be operated on.
 *
 * @param closure_idx
 *   The index of the native function to set the static of.
 *
 * @param static_idx
 *   The slot to be set.
 *
 * @param value_idx
 *   The index of the value to set.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - \p closure_idx is not a valid object type to be setting the field of.
 *   BIFROST_VM_ERROR_INVALID_ARGUMENT   - \p static_idx is an invalid index into the number of statics \p closure_idx contains.
 */
BF_VM_API BifrostVMError bfVM_closureSetStatic(BifrostVM* self, size_t closure_idx, size_t static_idx, size_t value_idx);

/* TODO: add a function line 'bfVM_closureSetStatic' that operates on the current native function. */

/*!
 * @brief
 *   Gets the user data of the closure specified at \p closure_idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param closure_idx
 *   The index of the closure you want to grab the user dat of.
 *
 * @return void*
 *   NULL      - If \p closure_idx is not a function object.
 *   Otherwise - The user data stored in \p closure_idx.
 */
BF_VM_API void* bfVM_closureStackGetExtraData(BifrostVM* self, size_t closure_idx);

/*!
 * @brief
 *   Gets the user data of the closure of the current native function.
 *   MUST only be called from within a native function.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @return void*
 *   NULL      - If not in a current native function.
 *   Otherwise - The user data stored in \p closure_idx.
 */
BF_VM_API void* bfVM_closureGetExtraData(BifrostVM* self);

/*!
 * @brief
 *   Creates a class binding an sets the field (BifrostVMClassBind::name) of \p inst_or_class_or_module
 *   to the new class.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param inst_or_class_or_module
 *   The instance, class, or module to set the field of.
 *
 * @param clz_bind
 *   The parameters in which to create the class.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_INVALID_OP_ON_TYPE - \p inst_or_class_or_module is not the type of object that can have a field set.
 */
BF_VM_API BifrostVMError bfVM_stackStoreClass(BifrostVM* self, size_t inst_or_class_or_module, const BifrostVMClassBind* clz_bind);

/*!
 * @brief
 *   Creates a string value and stores it in \p idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index to store the newly created string in.
 *
 * @param value
 *   The beginning of the string you want to store.
 *
 * @param len
 *   The number of bytes to copy from \p value to create the string value.
 */
BF_VM_API void bfVM_stackSetString(BifrostVM* self, size_t idx, const char* value, const size_t len);

/*!
 * @brief
 *   Creates a number value and stores it in \p idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index to store the newly created number value in.
 *
 * @param value
 *   The number that will be stored in the vm API stack.
 */
BF_VM_API void bfVM_stackSetNumber(BifrostVM* self, size_t idx, double value);

/*!
 * @brief
 *   Creates a boolean value and stores it in \p idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index to store the newly created boolean value in.
 *
 * @param value
 *   The boolean value that will be stored in the vm API stack.
 */
BF_VM_API void bfVM_stackSetBool(BifrostVM* self, size_t idx, bool value);

/*!
 * @brief
 *   Copies a nil value and stores it in \p idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index to store a nil value in.
 */
BF_VM_API void bfVM_stackSetNil(BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Reads an instance object from \p idx.
 *   Also works on null values, just returns NULL
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the to read the instance from.
 *
 * @return void*
 *   NULL      - if \p is a null object.
 *   Otherwise - a pointer to the instance object memory.
 */
BF_VM_API void* bfVM_stackReadInstance(const BifrostVM* self, size_t idx);  // Also works on null values, just returns NULL

/*!
 * @brief
 *   Reads a string from the API stack.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the to read the string from.
 *
 * @param out_size
 *   The size of the returned string.
 *   Can be null if you do not care about it's size.
 *
 * @return const char*
 *   A nul-terminated string stored in \p idx.
 */
BF_VM_API const char* bfVM_stackReadString(const BifrostVM* self, size_t idx, size_t* out_size);  // 'out_size' can be NULL.

/*!
 * @brief
 *   Reads a number from the API stack.
 *   If \p idx does not contain a number an assert is triggered.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index to read the number from.
 *
 * @return double
 *   Returns the number at the API stack.
 */
BF_VM_API double bfVM_stackReadNumber(const BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Reads a boolean from the API stack.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the to read the boolean from.
 *
 * @return bool
 *   true  - If the value stored in \p idx is true.
 *   false - If the value stored in \p idx is false.
 */
BF_VM_API bool bfVM_stackReadBool(const BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Grabs the type of the object stored at \p idx.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the to read the type from.
 *
 * @return BifrostVMType
 *   The type of object stored at index.
 */
BF_VM_API BifrostVMType bfVM_stackGetType(BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Gets the number of arguments the function at \p idx
 *   expects to be taken in.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the function to get the arity from.
 *
 * @return int32_t
 *   The arity of the function at \p idx, will be -1 if the function is variadic.
 */
BF_VM_API int32_t bfVM_stackGetArity(const BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Creates a handle to a the value at \p idx so that you can
 *   cache that value and keep it safe from being garbage collected.
 *
 *   Be sure to 'bfVM_stackDestroyHandle' the handle before the end of the
 *   vm's lifetime.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index that you want a reference to.
 *
 * @return bfValueHandle
 *   The opaque handle to the value at \p idx.
 */
BF_VM_API bfValueHandle bfVM_stackMakeHandle(BifrostVM* self, size_t idx);

/*!
 * @brief
 *   Loads the handle's value into the API stack.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param dst_idx
 *   The destination in the API stack to write the handle's value to.
 *
 * @param handle
 *   The handle to load into the API stack.
 *   Must not be NULL.
 */
BF_VM_API void bfVM_stackLoadHandle(BifrostVM* self, size_t dst_idx, bfValueHandle handle);

/*!
 * @brief
 *   Destroys the \p handle rendering it invalid after this function is called.
 *   Freeing a null handle is safe.
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param handle
 *   The handle to be destroyed.
 */
BF_VM_API void bfVM_stackDestroyHandle(BifrostVM* self, bfValueHandle handle);

/*!
 * @brief
 *   Gets you the arity of the function pointed to by handle.
 *   This is a performance function so that you do not have to:
 *   bfVM_stackLoadHandle then bfVM_stackGetArity.
 *
 * @param handle
 *   The handle that should be pointing to a function object.
 *
 * @return int32_t
 *   The arity of the function at \p idx, will be -1 if the function is variadic.
 */
BF_VM_API int32_t bfVM_handleGetArity(bfValueHandle handle);

/*!
 * @brief
 *   Gets you the type of the value pointed to by handle.
 *   This is a performance function so that you do not have to:
 *   bfVM_stackLoadHandle then bfVM_stackGetType.
 *
 * @param handle
 *   The handle you want to grab the type of.
 *
 * @return BifrostVMType
 *   The type of object stored at index.
 */
BF_VM_API BifrostVMType bfVM_handleGetType(bfValueHandle handle);

/*!
 * @brief
 *   Calls a function using the vm's callstack.
 *   Return value of the function is in API_stack[args_start]
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param idx
 *   The index of the function to call.
 *   Must be pointing to a function object.
 *
 * @param args_start
 *   The start of the list of arguments to the function.
 *
 * @param num_args
 *   The number of arguments to read starting from \p args_start.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_FUNCTION_ARITY_MISMATCH - Wrong number of arguments passed to the function.
 *   BIFROST_VM_ERROR_RUNTIME                 - There was a runtime error somewhere along the function execution.
 */
BF_VM_API BifrostVMError bfVM_call(BifrostVM* self, size_t idx, size_t args_start, int32_t num_args);

/*!
 * @brief
 *   Executes source code into a module. This is the main entry point
 *   for running code written for this language.
 *
 *   The final module will be locates in API_stack[0].
 *
 * @param self
 *   The vm that will be operated on.
 *
 * @param module
 *   The name of the module to store the code into.
 *   if NULL we will exec in an anon module.
 *
 * @param source
 *   The beginning of the source string.
 *
 * @param source_length
 *   The length of the source string.
 *
 * @return BifrostVMError
 *   BIFROST_VM_ERROR_MODULE_ALREADY_DEFINED - If the module has already been defined we have a problem.
 *   BIFROST_VM_ERROR_COMPILE                - If the \p source string contains invalid code.
 *   BIFROST_VM_ERROR_RUNTIME                - There was a runtime error somewhere along the source execution.
 */
BF_VM_API BifrostVMError bfVM_execInModule(BifrostVM* self, const char* module, const char* source, size_t source_length);

/*!
 * @brief
 *   Manually calls the garbage collection on the vm.
 *   This is not necessary for general usage of this library but if you
 *   have a particularly opportune time to gc then this may be of use.
 *
 * @param self
 *   The vm to garbage collect.
 */
BF_VM_API void bfVM_gc(BifrostVM* self);

/*!
 * @brief
 *  Returns the string representation of \p symbol.
 *
 * @param self
 *   Currently ignored as a paremeter, for future compatibility.
 *
 * @param symbol
 *   The symbol to retrieve the name of.
 *   Must NOT be 'BIFROST_VM_SYMBOL_MAX'.
 *
 * @return const char*
 *   A NUL terminated string of the symbol at \p symbol.
 */
BF_VM_API const char* bfVM_buildInSymbolStr(const BifrostVM* self, BifrostVMBuildInSymbol symbol);

/*!
 * @brief
 *   Returns a user friendly string of the last error to occur. (NUL terminated)
 *
 * @param self
 *   The vm the error happened on.
 *
 * @return ConstBifrostString
 *   A user friendly string of the last error to occur. (NUL terminated)
 */
BF_VM_API ConstBifrostString bfVM_errorString(const BifrostVM* self);

/*!
 * @brief
 *   Frees the memory pointed by the members of the vm.
 *
 * @param self
 *   The vm to destruct the internal members of.
 */
BF_VM_API void bfVM_dtor(BifrostVM* self);

/*!
 * @brief
 *   Sets last error message + calls the error callback.
 */
#define bfVM_SetLastError(error_type, vm, line_no, fmt, ...)              \
  bfVMString_sprintf(vm, &vm->last_error, (fmt), ##__VA_ARGS__);          \
  if ((vm)->params.error_fn != NULL)                                      \
  {                                                                       \
    (vm)->params.error_fn((vm), error_type, (line_no), (vm)->last_error); \
  }

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_API_H */
