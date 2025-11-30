/******************************************************************************/
/*!
 * @file   bifrost_vm_function_builder.h
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   The parser uses this API to generate instructions.
 *   The output from this is an executable set of bytecode.
 *
 * @copyright Copyright (c) 2020-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_FUNCTION_BUILDER_H
#define BIFROST_VM_FUNCTION_BUILDER_H

#include "bifrost_vm_instruction_op.h" /* bfInstructionOp */
#include "bifrost_vm_value.h"          /* bfVMValue        */

#if __cplusplus
extern "C" {
#endif

typedef struct BifrostVM    BifrostVM;
typedef struct BifrostObjFn BifrostObjFn;
typedef struct BifrostLexer BifrostLexer;
typedef struct string_range string_range;

typedef int bfScopeVarCount;

typedef struct BifrostVMFunctionBuilder
{
  const char*      name;     /*!< Stored in the source so no need to dynamically alloc */
  size_t           name_len; /*!< Length of [BifrostVMFunctionBuilder::name] */
  bfVMValue*       constants;
  string_range*    local_vars; /*!< Stored in the source so no need to dynamically alloc */
  bfScopeVarCount* local_var_scope_size;
  uint32_t*        instructions;
  uint16_t*        code_to_line;
  size_t           max_local_idx;
  BifrostVM*       vm;
  size_t*          current_line_no;

} BifrostVMFunctionBuilder;

void     bfFuncBuilder_ctor(BifrostVMFunctionBuilder* self, BifrostLexer* lexer);
void     bfFuncBuilder_begin(BifrostVMFunctionBuilder* self, const char* name, size_t length);
uint32_t bfFuncBuilder_addConstant(BifrostVMFunctionBuilder* self, const bfVMValue value);
void     bfFuncBuilder_pushScope(BifrostVMFunctionBuilder* self);
uint32_t bfFuncBuilder_declVariable(BifrostVMFunctionBuilder* self, const char* name, size_t length);
uint16_t bfFuncBuilder_pushTemp(BifrostVMFunctionBuilder* self, uint16_t num_temps);
void     bfFuncBuilder_popTemp(BifrostVMFunctionBuilder* self, uint16_t start);
size_t   bfFuncBuilder_getVariable(BifrostVMFunctionBuilder* self, const char* name, size_t length);
void     bfFuncBuilder_popScope(BifrostVMFunctionBuilder* self);
void     bfFuncBuilder_addInstABC(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, uint16_t b, uint16_t c);
void     bfFuncBuilder_addInstABx(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, uint32_t bx);
void     bfFuncBuilder_addInstAsBx(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, int32_t sbx);
void     bfFuncBuilder_addInstBreak(BifrostVMFunctionBuilder* self);
void     bfFuncBuilder_end(BifrostVMFunctionBuilder* self, BifrostObjFn* out, int arity);
void     bfFuncBuilder_dtor(BifrostVMFunctionBuilder* self);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_FUNCTION_BUILDER_H */
