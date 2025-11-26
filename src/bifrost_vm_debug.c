/******************************************************************************/
/*!
 * @file   bifrost_vm_debug.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   This contains extra functions for dumping out internal vm
 *   state into strings.
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#include "bifrost_vm_debug.h"

#include "bifrost_vm_instruction_op.h"
#include "bifrost_vm_obj.h"

#include <stdio.h> /* sprintf */

static void bfDbgIndentPrint(const int indent)
{
  for (int j = 0; j < indent; ++j)
  {
    printf("  ");
  }
}

size_t bfDbg_ValueToString(bfVMValue value, char* buffer, size_t buffer_size)
{
  if (bfVMValue_isNumber(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "%g", bfVMValue_asNumber(value));
  }
  else if (bfVMValue_isBool(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "%s", bfVMValue_isTrue(value) ? "true" : "false");
  }
  else if (bfVMValue_isNull(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "null");
  }
  else if (bfVMValue_isPointer(value))
  {
    const BifrostObj* const obj = bfVMValue_asPointer(value);

    switch (obj->type)
    {
      case BIFROST_VM_OBJ_FUNCTION:
      {
        const BifrostObjFn* const obj_fn = (const BifrostObjFn*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<fn %s>", obj_fn->name);
      }
      case BIFROST_VM_OBJ_MODULE:
      {
        return (size_t)snprintf(buffer, buffer_size, "<module>");
      }
      case BIFROST_VM_OBJ_CLASS:
      {
        const BifrostObjClass* const obj_clz = (const BifrostObjClass*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<class %s>", obj_clz->name);
      }
      case BIFROST_VM_OBJ_INSTANCE:
      {
        return (size_t)snprintf(buffer, buffer_size, "<instance>");
      }
      case BIFROST_VM_OBJ_STRING:
      {
        const BifrostObjStr* const obj_string = (const BifrostObjStr*)obj;

        return (size_t)snprintf(buffer, buffer_size, "%s", obj_string->value);
      }
      case BIFROST_VM_OBJ_NATIVE_FN:
      {
        return (size_t)snprintf(buffer, buffer_size, "<native function>");
      }
      case BIFROST_VM_OBJ_REFERENCE:
      {
        const BifrostObjReference* const obj_ref = (const BifrostObjReference*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<obj reference class(%s)>", obj_ref->clz ? obj_ref->clz->name : "null");
      }
      case BIFROST_VM_OBJ_WEAK_REF:
      {
        const BifrostObjWeakRef* const obj_weak_ref = (const BifrostObjWeakRef*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<obj weak ref %p>", obj_weak_ref->data);
      }
    }
  }

  return 0u;
}

size_t bfDbg_ValueTypeToString(bfVMValue value, char* buffer, size_t buffer_size)
{
  if (bfVMValue_isNumber(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "<Number>");
  }
  else if (bfVMValue_isBool(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "<Boolean>");
  }
  else if (bfVMValue_isNull(value))
  {
    return (size_t)snprintf(buffer, buffer_size, "<Nil>");
  }
  else if (bfVMValue_isPointer(value))
  {
    const BifrostObj* const obj = bfVMValue_asPointer(value);

    switch (obj->type)
    {
      case BIFROST_VM_OBJ_FUNCTION:
      {
        const BifrostObjFn* const obj_fn = (const BifrostObjFn*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<fn %s>", obj_fn->name);
      }
      case BIFROST_VM_OBJ_MODULE:
      {
        return (size_t)snprintf(buffer, buffer_size, "<Module>");
      }
      case BIFROST_VM_OBJ_CLASS:
      {
        const BifrostObjClass* const obj_clz = (const BifrostObjClass*)obj;

        return (size_t)snprintf(buffer, buffer_size, "<Class %s>", obj_clz->name);
      }
      case BIFROST_VM_OBJ_INSTANCE:
      {
        return (size_t)snprintf(buffer, buffer_size, "<Instance>");
      }
      case BIFROST_VM_OBJ_STRING:
      {
        return (size_t)snprintf(buffer, buffer_size, "<String>");
      }
      case BIFROST_VM_OBJ_NATIVE_FN:
      {
        return (size_t)snprintf(buffer, buffer_size, "<NativeFunction>");
      }
      case BIFROST_VM_OBJ_REFERENCE:
      {
        return (size_t)snprintf(buffer, buffer_size, "<Reference>");
      }
      case BIFROST_VM_OBJ_WEAK_REF:
      {
        return (size_t)snprintf(buffer, buffer_size, "<Weak Ref>");
      }
    }
  }

  return (size_t)snprintf(buffer, buffer_size, "<Undefined>");
}

const char* bfDbg_InstOpToString(const bfInstructionOp op)
{
  switch (op)
  {
#define BF_INST_OP(name, str) \
  case BIFROST_VM_OP_##name: return #name;
    BF_INST_OP_XTABLE
#undef BF_INST_OP
    default: return "OP_UNKNOWN";
  }
}

static const char* bfDbg_InstOpComment(const bfInstructionOp op)
{
  switch (op)
  {
#define BF_INST_OP(name, str) \
  case BIFROST_VM_OP_##name: return str;
    BF_INST_OP_XTABLE
#undef BF_INST_OP
    default: return "OP_UNKNOWN";
  }
}

extern void bfInst_decode(const bfInstruction inst, uint8_t* op_out, uint32_t* ra_out, uint32_t* rb_out, uint32_t* rc_out, uint32_t* rbx_out, int32_t* rsbx_out);

void bfDbg_DisassembleInstructions(int indent, const bfInstruction* code, size_t code_length, uint16_t* code_to_line)
{
  bfDbgIndentPrint(indent);

  printf("-----------------------------------------------------------------------------------------\n");
  for (size_t i = 0; i < code_length; ++i)
  {
    bfDbgIndentPrint(indent);

    if (code_to_line)
    {
      printf("Line[%3i]: ", (int)code_to_line[i]);
    }

    uint8_t  op;
    uint32_t regs[4];
    int32_t  rsbx;
    bfInst_decode(code[i], &op, regs + 0, regs + 1, regs + 2, regs + 3, &rsbx);

    // const char* const comment = bfDbg_InstOpComment(op);
    const char* const comment = "";

    printf("| 0x%08X | %11s |a: %3u| b: %3u| c: %3u| bx: %7u| sbx: %+7i| %s\n", code[i], bfDbg_InstOpToString(op), regs[0], regs[1], regs[2], regs[3], rsbx, comment);
  }

  bfDbgIndentPrint(indent);
  printf("-----------------------------------------------------------------------------------------\n");
}

void bfDbg_DisassembleFunction(int indent, const BifrostObjFn* function)
{
  const size_t num_constants      = bfVMArray_size(&function->constants);
  const size_t num_instructions   = bfVMArray_size(&function->instructions);
  const size_t needed_stack_space = function->needed_stack_space;

  bfDbgIndentPrint(indent + 0);
  printf("Function(%s, arity = %i, stack_space = %i, module = '%s'):\n", function->name, function->arity, (int)needed_stack_space, function->module->name);

  bfDbgIndentPrint(indent + 1);
  printf("Constants(%i):\n", (int)num_constants);

  for (size_t i = 0; i < num_constants; ++i)
  {
    char temp_buffer[128];

    bfDbg_ValueToString(function->constants[i], temp_buffer, sizeof(temp_buffer));

    bfDbgIndentPrint(indent + 2);
    printf("[%i] = %s\n", (int)i, temp_buffer);
  }

  bfDbgIndentPrint(indent + 1);
  printf("Instructions(%i):\n", (int)num_instructions);
  bfDbg_DisassembleInstructions(indent + 2, function->instructions, num_instructions, function->code_to_line);

  bfDbgIndentPrint(indent + 1);
}

const char* bfDbg_TokenTypeToString(bfTokenType t)
{
  switch (t)
  {
#define BIFROST_TOKEN(TokenName, ParsePrefix, ParseInfix, ParsePrecedence) \
  case TokenName: return #TokenName;
    BIFROST_TOKEN_XTABLE
#undef BIFROST_TOKEN
    default: return "Invalid Token Type";
  }
}

void bfDbg_PrintToken(const bfToken* token)
{
  const char* const type_str = bfDbg_TokenTypeToString(token->type);

  printf("[%30s] => ", type_str);
  if (token->type == BIFROST_TOKEN_CONST_REAL)
  {
    printf("[%g]", token->num);
  }
  else
  {
    printf("[%.*s]", (int)string_range_length(token->str_range), token->str_range.str_bgn);
  }

  printf("\n");
}
