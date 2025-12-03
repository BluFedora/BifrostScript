/******************************************************************************/
/*!
 * @file   vm_command_line.cpp
 * @author Shareef Raheem (https://blufedora.github.io/)
 * @brief
 *   Command Line Interface for the Virtual Machine.
 *
 * @version 0.0.1
 * @date    2020-02-17
 *
 * @copyright Copyright (c) 2020-2021
 */
/******************************************************************************/

#define _CRT_SECURE_NO_WARNINGS
#include "bifrost/bifrost_vm.hpp"  // VM C++ API

#include <cassert>   // assert
#include <cstdio>    // printf, fopen, fclose, ftell, fseek, fread, malloc
#include <iostream>  // cin

struct MemoryUsageTracker final
{
  std::size_t peak_usage;
  std::size_t current_usage;
};

static void  errorHandler(BifrostVM* vm, BifrostVMError err, int line_no, const char* message) noexcept;
static void  printHandler(BifrostVM* vm, const char* message) noexcept;
static void  moduleHandler(BifrostVM* vm, const char* from, const char* module, BifrostVMModuleLookUp* out) noexcept;
static void* memoryHandler(void* user_data, void* ptr, size_t old_size, size_t new_size) noexcept;
static void  waitForInput() noexcept;

int main(int argc, char* argv[])
{
#if defined(__EMSCRIPTEN__) && __EMSCRIPTEN__
  const char* const file_name = "assets/scripts/test_script.bscript";
#else
  if (argc != 2)
  {
    std::printf("There is an example script loaded at 'assets/scripts/test_script.bscript'\n");
    std::printf("usage %s <file-name>\n", argv[0]);
    waitForInput();
    return 0;
  }

  const char* const file_name = argv[1];
#endif

  MemoryUsageTracker mem_tracker{0, 0};

  BifrostVMParams params;
  bfVMParams_initDefault(&params);
  params.error_fn  = &errorHandler;
  params.print_fn  = &printHandler;
  params.module_fn = &moduleHandler;
  params.memory_fn = &memoryHandler;
  params.user_data = &mem_tracker;

  {
    BifrostVM vm;
    bfVM_ctor(&vm, &params);

    BifrostVMModuleLookUp load_file;

    moduleHandler(&vm, nullptr, file_name, &load_file);

    if (!load_file.source || load_file.source_len == 0)
    {
      std::printf("failed to load '%s'\n", file_name);
      return 1;
    }

#if 1
    bfVM_stackResize(&vm, 1);
    bfVM_moduleLoadStd(&vm, 0, BIFROST_VM_STD_MODULE_ALL);

    const BifrostVMError err = bfVM_execInModule(&vm, nullptr, load_file.source, load_file.source_len);
#else
    vm.stackResize(1);
    vm.moduleLoad(0, BIFROST_VM_STD_MODULE_ALL);

    const BifrostVMError err = vm.execInModule(nullptr, load_file.source, load_file.source_len);
#endif

    memoryHandler(bfVM_userData(&vm), const_cast<char*>(load_file.source), sizeof(char) * (load_file.source_len + 1u), 0u);

    if (err)
    {
      waitForInput();
      return err;
    }

    std::printf("Memory Stats:\n");
    std::printf("\tPeak    Usage: %u (bytes)\n", unsigned(mem_tracker.peak_usage));
    std::printf("\tCurrent Usage: %u (bytes)\n", unsigned(mem_tracker.current_usage));
  }

  std::printf("\tAfter    Dtor: %u (bytes)\n", unsigned(mem_tracker.current_usage));

  waitForInput();
  return 0;
}

static void errorHandler(BifrostVM* /*vm*/, BifrostVMError err, int line_no, const char* message) noexcept
{
  const char* err_type_str = nullptr;

  switch (err)
  {
    case BIFROST_VM_ERROR_OUT_OF_MEMORY:
      err_type_str = "OOM";
      break;
    case BIFROST_VM_ERROR_RUNTIME:
      err_type_str = "Runtime";
      break;
    case BIFROST_VM_ERROR_LEXER:
      err_type_str = "Lexer";
      break;
    case BIFROST_VM_ERROR_COMPILE:
      err_type_str = "Compiler";
      break;
    case BIFROST_VM_ERROR_FUNCTION_ARITY_MISMATCH:
      err_type_str = "Function Arity Mismatch";
      break;
    case BIFROST_VM_ERROR_MODULE_ALREADY_DEFINED:
      err_type_str = "Module Already Exists";
      break;
    case BIFROST_VM_ERROR_MODULE_NOT_FOUND:
      err_type_str = "Missing Module";
      break;
    case BIFROST_VM_ERROR_INVALID_OP_ON_TYPE:
      err_type_str = "Invalid Type";
      break;
    case BIFROST_VM_ERROR_INVALID_ARGUMENT:
      err_type_str = "Invalid Arg";
      break;
    case BIFROST_VM_ERROR_STACK_TRACE_BEGIN:
      err_type_str = "Trace Bgn";
      break;
    case BIFROST_VM_ERROR_STACK_TRACE:
      err_type_str = "STACK";
      break;
    case BIFROST_VM_ERROR_STACK_TRACE_END:
      err_type_str = "Trace End";
      break;
    case BIFROST_VM_ERROR_NONE:
      err_type_str = "none";
      break;
  }

  assert(err_type_str != nullptr);

  std::printf("%s Error[Line %i]: %s\n", err_type_str, line_no, message);
}

static void printHandler(BifrostVM* /*vm*/, const char* message) noexcept
{
  std::printf("%s\n", message);
}

static void moduleHandler(BifrostVM* vm, const char* /*from*/, const char* module, BifrostVMModuleLookUp* out) noexcept
{
  FILE* const file      = std::fopen(module, "rb");  // NOLINT(android-cloexec-fopen)
  char*       buffer    = nullptr;
  long        file_size = 0u;

  if (file)
  {
    std::fseek(file, 0, SEEK_END);
    file_size = std::ftell(file);

    if (file_size != -1L)
    {
      std::fseek(file, 0, SEEK_SET);  // same as std::rewind(file);
      buffer = static_cast<char*>(memoryHandler(bfVM_userData(vm), nullptr, 0u, sizeof(char) * (std::size_t(file_size) + 1)));

      if (buffer)
      {
        std::fread(buffer, sizeof(char), file_size, file);
        buffer[file_size] = '\0';
      }
      else
      {
        file_size = 0;
      }
    }
    else
    {
      file_size = 0;
    }

    std::fclose(file);
  }

  out->source     = buffer;
  out->source_len = file_size;
}

static void* memoryHandler(void* user_data, void* ptr, size_t old_size, size_t new_size) noexcept
{
  //
  // These checks are largely redundant since it just reimplements
  // what 'realloc' already does, this is mostly for demonstrative
  // purposes on how to write your own memory allocator function.
  //

  MemoryUsageTracker* const mum_tracker = static_cast<MemoryUsageTracker*>(user_data);

  mum_tracker->current_usage -= old_size;
  mum_tracker->current_usage += new_size;

  if (mum_tracker->current_usage > mum_tracker->peak_usage)
  {
    mum_tracker->peak_usage = mum_tracker->current_usage;
  }

  if (old_size == 0u || ptr == nullptr)  // Both checks are not needed but just for illustration of both ways of checking for new allocation.
  {
    return new_size != 0 ? std::malloc(new_size) : nullptr;  // Returning nullptr for a new_size of 0 is not strictly required.
  }

  if (new_size == 0u)
  {
    std::free(ptr);
  }
  else
  {
    return std::realloc(ptr, new_size);
  }

  return nullptr;
}

static void waitForInput() noexcept
{
#ifndef __EMSCRIPTEN__
  try
  {
    std::cin.ignore();
  }
  catch (const std::ios::failure&)
  {
  }
#endif
}

#if 0

// TODO(SR): REMOVE ME
static void nativeFunctionTest(BifrostVM* vm, int32_t num_args)
{
  assert(num_args == 2);

  const bfVMNumberT num0 = bfVM_stackReadNumber(vm, 0);
  const bfVMNumberT num1 = bfVM_stackReadNumber(vm, 1);

  bfVM_stackResize(vm, 2);
  bfVM_moduleLoad(vm, 0, "main");
  bfVM_stackLoadVariable(vm, 0, 0, "fibbonacci");
  bfVM_stackSetNumber(vm, 1, num0);

  bfVM_call(vm, 0, 1, 1);

  bfVM_stackSetNumber(vm, 0, bfVM_stackReadNumber(vm, 1) * num1);
}

// TODO(SR): REMOVE ME
static void nativeFunctionMathPrint(BifrostVM* vm, int32_t num_args)
{
  assert(num_args == 0);
  (void)vm;
  printf("This is from the math module\n");
}

// TODO(SR): REMOVE ME
static void userClassMath_mult(BifrostVM* vm, int32_t num_args)
{
  (void)num_args;

  const bfVMNumberT num0 = bfVM_stackReadNumber(vm, 0);
  const bfVMNumberT num1 = bfVM_stackReadNumber(vm, 1);

  bfVM_stackSetNumber(vm, 0, num0 * num1);
}


void bfRegisterModuleMemory(BifrostVM* vm);

    /*
      Initialization
    */
    BifrostVM* vm = bfVM_new(&vm_params);

    bfVM_stackResize(vm, 1);
    bfVM_moduleMake(vm, 0, "std:math");
    bfVM_moduleBindNativeFn(vm, 0, "math_print", &nativeFunctionMathPrint, 0);

    bfRegisterModuleMemory(vm);

    bfVM_moduleMake(vm, 0, "std:array");
    /*
    typedef struct
    {
      BifrostValue* data;

    } StdArray;

    static const BifrostMethodBind s_ArrayClassMethods[] = {
     {"ctor", &userClassFile_ctor, -1},
     {"size", &userClassFile_print, -1},
     {"capacity", &userClassFile_print, -1},
     {"push", &userClassFile_print, -1},
     {"insert", &userClassFile_close, -1},
     {"removeAt", &userClassFile_close, -1},
     {"pop", &userClassFile_close, -1},
     {"concat", &userClassFile_close, -1},
     {"clone", &userClassFile_close, -1},
     {"copy", &userClassFile_close, -1},
     {"resize", &userClassFile_close, -1},
     {"reserve", &userClassFile_close, -1},
     {"sortedFindFrom", &userClassFile_close, -1},
     {"sortedFind", &userClassFile_close, -1},
     {"findFrom", &userClassFile_close, -1},
     {"find", &userClassFile_close, -1},
     {"[]", &userClassFile_close, -1},
     {"[]=", &userClassFile_close, -1},
     {"sort", &userClassFile_close, -1},
     {"clear", &userClassFile_close, -1},
     {"front", &userClassFile_close, -1},
     {"back", &userClassFile_close, -1},
     {"dtor", &userClassFile_close, -1},
     {NULL, NULL, 0},
    };

    BifrostVMClassBind array_clz = {
     .name            = "Array",
     .extra_data_size = sizeof(StdArray),
     .methods         = s_ArrayClassMethods};

    bfVM_moduleBindClass(vm, 0, &array_clz);
 */
    /*
      Running Code
    */
    long        source_size;
    char* const source = load_from_file(argv[1], &source_size);

    if (source && source_size)
    {
      const BifrostVMError err = bfVM_execInModule(vm, "main", source, (size_t)source_size);
      free(source);

      if (err != BIFROST_VM_ERROR_NONE)
      {
        printf("ERROR FROM MAIN!\n");
      }
      else
      {
        printf("### Calling GC ###\n");
        bfVM_gc(vm);
        printf("### GC Done    ###\n");

        bfVM_stackResize(vm, 2);
        bfVM_moduleLoad(vm, 0, "main");
        bfVM_stackLoadVariable(vm, 0, 0, "fibbonacci");
        bfVM_stackSetNumber(vm, 1, 9.0);

        bfVM_call(vm, 0, 1, 1);

        bfVMNumberT result = bfVM_stackReadNumber(vm, 1);

        printf("VM Result0: %f\n", result);

        bfVM_stackResize(vm, 3);
        bfVM_moduleLoad(vm, 0, "main");
        bfVM_moduleBindNativeFn(vm, 0, "facAndMult", &nativeFunctionTest, 2);
        bfVM_stackLoadVariable(vm, 0, 0, "facAndMult");
        bfVM_stackSetNumber(vm, 1, 9.0);
        bfVM_stackSetNumber(vm, 2, 3.0);

        bfVM_call(vm, 0, 1, 2);

        result = bfVM_stackReadNumber(vm, 1);

        printf("VM Result1: %f\n", result);

        printf("<--- Class Registration: --->\n");

        bfVM_stackResize(vm, 1);
        bfVM_moduleLoad(vm, 0, "main");

        static const BifrostMethodBind s_TestClassMethods[] = {
         {"mult", &userClassMath_mult, 2},
         {NULL, NULL, 0},
        };

        BifrostVMClassBind test_clz = {
         .name            = "Math",
         .extra_data_size = 0u,
         .methods         = s_TestClassMethods};

        bfVM_moduleBindClass(vm, 0, &test_clz);

        printf("-----------------------------\n");

        bfVM_stackLoadVariable(vm, 0, 0, "testNative");

        if (bfVM_call(vm, 0, 0, 0))
        {
          printf("There was an error running 'testNative'\n");
        }
      }
    }
    else
    {
      printf("Could not load file %s\n", argv[1]);
    }

    bfVM_delete(vm);
  }
  else
  {
    printf("Invalid number of arguments passed (%i)\n", argc);
    return -1;
  }
  return 0;
}

typedef struct
{
  bfValueHandle fn; /*!< Function to call. */

} bfClosure;

void bfCoreClosure_ctor(BifrostVM* vm, int32_t num_args)
{
  assert(num_args == 2);

  bfClosure* const self = bfVM_stackReadInstance(vm, 0);
  self->fn              = bfVM_stackMakeHandle(vm, 1);
}

void bfCoreClosure_call(BifrostVM* vm, int32_t num_args)
{
  bfClosure* const self = bfVM_stackReadInstance(vm, 0);

  const size_t arity = bfVM_handleGetArity(self->fn);
  bfVM_stackResize(vm, arity + 1);
  bfVM_stackLoadHandle(vm, arity, self->fn);

  bfVM_call(vm, arity, 0, num_args);
}

static void bfCoreClosure_finalizer(BifrostVM* vm, void* instance)
{
  const bfClosure* const self = instance;
  bfVM_stackDestroyHandle(vm, self->fn);
}

void bfCoreMemory_gc(BifrostVM* vm, int32_t num_args)
{
  assert(num_args == 0);
  bfVM_gc(vm);
}

void bfRegisterModuleMemory(BifrostVM* vm)
{
  bfVM_stackResize(vm, 1);
  bfVM_moduleMake(vm, 0, "std:memory");
  bfVM_moduleBindNativeFn(vm, 0, "gc", &bfCoreMemory_gc, 0);

  bfVM_moduleMake(vm, 0, "std:functional");

  static const BifrostMethodBind s_ClosureClassMethods[] =
   {
    {"ctor", &bfCoreClosure_ctor, 2},
    {"call", &bfCoreClosure_call, -1},
    {NULL, NULL, 0},
   };

  BifrostVMClassBind closure_clz =
   {
    .name            = "Closure",
    .extra_data_size = sizeof(bfClosure),
    .methods         = s_ClosureClassMethods,
    .finalizer       = bfCoreClosure_finalizer,
   };

  bfVM_moduleBindClass(vm, 0, &closure_clz);
}

#endif