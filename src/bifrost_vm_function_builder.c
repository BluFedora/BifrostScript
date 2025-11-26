/******************************************************************************/
/*!
 * @file   bifrost_vm_function_builder.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   The parser uses this API to generate instructions.
 *   The output from this is an executable set of bytecode.
 *
 * @copyright Copyright (c) 2020-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#include "bifrost_vm_function_builder.h"

#include "bifrost_vm_obj.h"

static const size_t k_DefaultArraySize = 8;

void bfFuncBuilder_ctor(BifrostVMFunctionBuilder* self, BifrostLexer* lexer)
{
  self->name                 = NULL;
  self->name_len             = 0;
  self->constants            = NULL;
  self->instructions         = NULL;
  self->code_to_line         = NULL;
  self->local_vars           = bfVMArray_new(lexer->vm, string_range, k_DefaultArraySize);
  self->local_var_scope_size = bfVMArray_new(lexer->vm, int, k_DefaultArraySize);
  self->max_local_idx        = 0;
  self->vm                   = lexer->vm;
  self->current_line_no      = &lexer->current_line_no;
}

void bfFuncBuilder_begin(BifrostVMFunctionBuilder* self, const char* name, size_t length)
{
  LibC_assert(self->constants == NULL, "This builder has already began.");

  self->name         = name;
  self->name_len     = length;
  self->constants    = bfVMArray_new(self->vm, bfVMValue, k_DefaultArraySize);
  self->instructions = bfVMArray_new(self->vm, uint32_t, k_DefaultArraySize);
  self->code_to_line = bfVMArray_newA(self->vm, self->code_to_line, k_DefaultArraySize);
  bfVMArray_clear(&self->local_vars);
  bfVMArray_clear(&self->local_var_scope_size);

  bfFuncBuilder_pushScope(self);
}

uint32_t bfFuncBuilder_addConstant(BifrostVMFunctionBuilder* self, bfVMValue value)
{
  size_t index = bfVMArray_find(&self->constants, &value, NULL);

  if (index == BIFROST_ARRAY_INVALID_INDEX)
  {
    index = bfVMArray_size(&self->constants);
    bfVMArray_push(self->vm, &self->constants, &value);
  }

  return (uint32_t)index;
}

void bfFuncBuilder_pushScope(BifrostVMFunctionBuilder* self)
{
  bfScopeVarCount* count = bfVMArray_emplace(self->vm, &self->local_var_scope_size);
  *count                 = 0;
}

static inline size_t bfFuncBuilder__getVariable(BifrostVMFunctionBuilder* self, const char* name, size_t length, bool in_current_scope)
{
  const int* count = (const int*)bfVMArray_back(&self->local_var_scope_size);
  const int  end   = in_current_scope ? *count : (int)bfVMArray_size(&self->local_vars);

  for (int i = end - 1; i >= 0; --i)
  {
    const string_range* const var = self->local_vars + i;

    LibC_assert((int)bfVMArray_size(&self->local_vars) > i, "Invalid indexing.");

    if (length == var->str_len && bfVMString_ccmpn(name, var->str_bgn, length) == 0)
    {
      return i;
    }
  }

  return BIFROST_ARRAY_INVALID_INDEX;
}

uint32_t bfFuncBuilder_declVariable(BifrostVMFunctionBuilder* self, const char* name, size_t length)
{
  const size_t prev_decl = bfFuncBuilder__getVariable(self, name, length, true);

  if (prev_decl != BIFROST_ARRAY_INVALID_INDEX)
  {
    bfVM_SetLastError(BIFROST_VM_ERROR_COMPILE, self->vm, (int)*self->current_line_no, "ERROR: [%.*s] already declared.\n", (int)length, name);
    return (uint32_t)prev_decl;
  }

  const size_t  var_loc = bfVMArray_size(&self->local_vars);
  string_range* var     = bfVMArray_emplace(self->vm, &self->local_vars);

  *var = MakeStringLen(name, length);

  int* count = (int*)bfVMArray_back(&self->local_var_scope_size);
  ++(*count);

  if (self->max_local_idx < var_loc)
  {
    self->max_local_idx = var_loc;
  }

  return (uint32_t)var_loc;
}

uint16_t bfFuncBuilder_pushTemp(BifrostVMFunctionBuilder* self, uint16_t num_temps)
{
  const size_t  var_loc     = bfVMArray_size(&self->local_vars);
  const size_t  var_loc_end = var_loc + num_temps;
  string_range* vars        = bfVMArray_emplaceN(self->vm, &self->local_vars, num_temps);

  for (size_t i = 0; i < num_temps; ++i)
  {
    vars[i] = MakeStringLen(NULL, 0);
  }

  if (self->max_local_idx < var_loc_end)
  {
    self->max_local_idx = var_loc_end;
  }

  return (uint16_t)var_loc;
}

void bfFuncBuilder_popTemp(BifrostVMFunctionBuilder* self, uint16_t start)
{
  bfVMArray_resize(self->vm, &self->local_vars, start);
}

size_t bfFuncBuilder_getVariable(BifrostVMFunctionBuilder* self, const char* name, size_t length)
{
  return bfFuncBuilder__getVariable(self, name, length, false);
}

void bfFuncBuilder_popScope(BifrostVMFunctionBuilder* self)
{
  const int*   count    = (const int*)bfVMArray_back(&self->local_var_scope_size);
  const size_t num_vars = bfVMArray_size(&self->local_vars);
  const size_t new_size = num_vars - *count;

  bfVMArray_resize(self->vm, &self->local_vars, new_size);
  bfVMArray_pop(&self->local_var_scope_size);
}

static inline bfInstruction* bfFuncBuilder_addInst(BifrostVMFunctionBuilder* self)
{
  *(uint16_t*)bfVMArray_emplace(self->vm, &self->code_to_line) = (uint16_t)*self->current_line_no;
  return bfVMArray_emplace(self->vm, &self->instructions);
}

void bfFuncBuilder_addInstABC(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, uint16_t b, uint16_t c)
{
  *bfFuncBuilder_addInst(self) = BIFROST_MAKE_INST_OP_ABC(op, a, b, c);
}

void bfFuncBuilder_addInstABx(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, uint32_t bx)
{
  *bfFuncBuilder_addInst(self) = BIFROST_MAKE_INST_OP_ABx(op, a, bx);
}

void bfFuncBuilder_addInstAsBx(BifrostVMFunctionBuilder* self, bfInstructionOp op, uint16_t a, int32_t sbx)
{
  *bfFuncBuilder_addInst(self) = BIFROST_MAKE_INST_OP_AsBx(op, a, sbx);
}

void bfFuncBuilder_addInstBreak(BifrostVMFunctionBuilder* self)
{
  // NOTE(SR): Will be patched later when the loop ends.
  *bfFuncBuilder_addInst(self) = BIFROST_INST_INVALID;
}

void bfFuncBuilder_end(BifrostVMFunctionBuilder* self, BifrostObjFn* out, int arity)
{
  bfFuncBuilder_addInstABx(self, BIFROST_VM_OP_RETURN, 0, 0);
  bfFuncBuilder_popScope(self);

  out->super.type         = BIFROST_VM_OBJ_FUNCTION;
  out->name               = bfVMString_newLen(self->vm, self->name, self->name_len);
  out->arity              = arity;
  out->code_to_line       = self->code_to_line;
  out->constants          = self->constants;
  out->instructions       = self->instructions;
  out->needed_stack_space = self->max_local_idx + arity + 1;

  // Transfer of ownership of the constants to output function.
  self->constants = NULL;
}

void bfFuncBuilder_dtor(BifrostVMFunctionBuilder* self)
{
  bfVMArray_delete(self->vm, &self->local_vars);
  bfVMArray_delete(self->vm, &self->local_var_scope_size);
}
