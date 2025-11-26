/******************************************************************************/
/*!
 * @file   bifrost_vm_debug.h
 * @author Shareef Raheem (http://blufedora.github.io/)
 * @brief
 *   This contains extra functions for dumping out internal vm state into strings.
 *
 * @copyright Copyright (c) 2020-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_DEBUG_H
#define BIFROST_VM_DEBUG_H

#include "bifrost_vm_instruction_op.h" /* bfInstruction */
#include "bifrost_vm_value.h"          /* bfVMValue     */

#if __cplusplus
extern "C" {
#endif

typedef enum bfTokenType    bfTokenType;
typedef struct bfToken      bfToken;
typedef struct BifrostObjFn BifrostObjFn;

size_t      bfDbg_ValueToString(bfVMValue value, char* buffer, size_t buffer_size);
size_t      bfDbg_ValueTypeToString(bfVMValue value, char* buffer, size_t buffer_size);
const char* bfDbg_InstOpToString(const bfInstructionOp op);
void        bfDbg_DisassembleInstructions(int indent, const bfInstruction* code, size_t code_length, uint16_t* code_to_line);
void        bfDbg_DisassembleFunction(int indent, const BifrostObjFn* function);
const char* bfDbg_TokenTypeToString(bfTokenType t);
void        bfDbg_PrintToken(const bfToken* token);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_DEBUG_H */
