/******************************************************************************/
/*!
 * @file   bifrost_vm_instruction_op.h
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @par
 *    Bifrost Scripting Language
 *
 * @brief
 *    The list of op codes the virtual machine handles along with
 *    the spec on interpreting each code.
 *
 * @version 0.0.1-beta
 * @date    2019-07-01
 *
 * @copyright Copyright (c) 2019 Shareef Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_INSTRUCTION_OP_H
#define BIFROST_VM_INSTRUCTION_OP_H

#include "bifrost_libc.h"

#if __cplusplus
extern "C" {
#endif

#define BIFROST_VM_OP_LOAD_BASIC_TRUE           0
#define BIFROST_VM_OP_LOAD_BASIC_FALSE          1
#define BIFROST_VM_OP_LOAD_BASIC_NULL           2
#define BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE 3
#define BIFROST_VM_OP_LOAD_BASIC_CONSTANT       4

// TODO(SR): Bit Logic Ops
//   BIFROST_VM_OP_BIT_OR,   // rA = rB | rC
//   BIFROST_VM_OP_BIT_AND,  // rA = rB & rC
//   BIFROST_VM_OP_BIT_XOR,  // rA = rB ^ rC
//   BIFROST_VM_OP_BIT_NOT,  // rA = ~rB
//   BIFROST_VM_OP_BIT_LS,   // rA = (rB << rC)
//   BIFROST_VM_OP_BIT_RS,   // rA = (rB >> rC)

// Total of 26 / 32 possible ops.

/*!
   ///////////////////////////////////////////
   // 0     5         14        23       32 //
   // [ooooo|aaaaaaaaa|bbbbbbbbb|ccccccccc] //
   // [ooooo|aaaaaaaaa|bxbxbxbxbxbxbxbxbxb] //
   // [ooooo|aaaaaaaaa|sBxbxbxbxbxbxbxbxbx] //
   // opcode = 0       - 31                 //
   // rA     = 0       - 511                //
   // rB     = 0       - 511                //
   // rBx    = 0       - 262143             //
   // rsBx   = -131071 - 131072             //
   // rC     = 0       - 511                //
   ///////////////////////////////////////////
 */
#define BF_INST_OP_XTABLE                                                                                                                                \
  /* Load OPs  */                                                                                                                                        \
  BF_INST_OP(LOAD_SYMBOL, "rA = rB.SYMBOLS[rC]")                                                                                                         \
  BF_INST_OP(LOAD_BASIC, "rA = (rBx == 0 : true) || (rBx == 1 : false) || (rBx == 2 : null) || (rBx == 3 : <current-module>) || (rBx > 3 : K[rBx - 4])") \
  /* Store OPs */                                                                                                                                        \
  BF_INST_OP(STORE_MOVE, "rA              = rBx")                                                                                                        \
  BF_INST_OP(STORE_SYMBOL, "rA.SYMBOLS[rB] = rC")                                                                                                        \
  /* Memory OPs */                                                                                                                                       \
  BF_INST_OP(NEW_CLZ, "rA = new local[rBx];")                                                                                                            \
  /* Math OPs */                                                                                                                                         \
  BF_INST_OP(MATH_ADD, "rA = rB + rC")                                                                                                                   \
  BF_INST_OP(MATH_SUB, "rA = rB - rC")                                                                                                                   \
  BF_INST_OP(MATH_MUL, "rA = rB * rC")                                                                                                                   \
  BF_INST_OP(MATH_DIV, "rA = rB / rC")                                                                                                                   \
  BF_INST_OP(MATH_MOD, "rA = rB % rC")                                                                                                                   \
  BF_INST_OP(MATH_POW, "rA = rB ^ rC")                                                                                                                   \
  BF_INST_OP(MATH_INV, "rA = -rB")                                                                                                                       \
  /* Comparisons */                                                                                                                                      \
  BF_INST_OP(CMP_EE, "rA = rB == rC")                                                                                                                    \
  BF_INST_OP(CMP_NE, "rA = rB != rC")                                                                                                                    \
  BF_INST_OP(CMP_LT, "rA = rB <  rC")                                                                                                                    \
  BF_INST_OP(CMP_LE, "rA = rB <= rC")                                                                                                                    \
  BF_INST_OP(CMP_GT, "rA = rB >  rC")                                                                                                                    \
  BF_INST_OP(CMP_GE, "rA = rB >= rC")                                                                                                                    \
  BF_INST_OP(CMP_AND, "rA = rB && rC")                                                                                                                   \
  BF_INST_OP(CMP_OR, "rA = rB || rC")                                                                                                                    \
  BF_INST_OP(NOT, "rA = !rBx")                                                                                                                           \
  /* Control Flow */                                                                                                                                     \
  BF_INST_OP(CALL_FN, "call(local[rB]) (params-start = rA, num-args = rC)")                                                                              \
  BF_INST_OP(JUMP, "ip += rsBx")                                                                                                                         \
  BF_INST_OP(JUMP_IF, "if (rA) ip += rsBx")                                                                                                              \
  BF_INST_OP(JUMP_IF_NOT, "if (!rA) ip += rsBx")                                                                                                         \
  BF_INST_OP(RETURN, "pop the current call frame.")

typedef enum bfInstructionOp
{
#define BF_INST_OP(name, str) BIFROST_VM_OP_##name,
  BF_INST_OP_XTABLE
#undef BF_INST_OP

} bfInstructionOp;

typedef uint32_t bfInstruction;

#define BIFROST_INST_OP_MASK     (bfInstruction)0x1F /* 31 in hex  */
#define BIFROST_INST_OP_OFFSET   (bfInstruction)0
#define BIFROST_INST_RA_MASK     (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RA_OFFSET   (bfInstruction)5
#define BIFROST_INST_RB_MASK     (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RB_OFFSET   (bfInstruction)14
#define BIFROST_INST_RC_MASK     (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RC_OFFSET   (bfInstruction)23
#define BIFROST_INST_RBx_MASK    (bfInstruction)0x3FFFF
#define BIFROST_INST_RBx_OFFSET  (bfInstruction)14
#define BIFROST_INST_RsBx_MASK   (bfInstruction)0x3FFFF
#define BIFROST_INST_RsBx_OFFSET (bfInstruction)14
#define BIFROST_INST_RsBx_MAX    ((bfInstruction)BIFROST_INST_RsBx_MASK / 2)

#define BIFROST_INST_INVALID     0xFFFFFFFF

#define BIFROST_MAKE_INST_OP(op) \
  (bfInstruction)(op & BIFROST_INST_OP_MASK)

#define BIFROST_MAKE_INST_RC(c) \
  ((c & BIFROST_INST_RC_MASK) << BIFROST_INST_RC_OFFSET)

#define BIFROST_MAKE_INST_OP_ABC(op, a, b, c)               \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   ((b & BIFROST_INST_RB_MASK) << BIFROST_INST_RB_OFFSET) | \
   BIFROST_MAKE_INST_RC((c))

#define BIFROST_MAKE_INST_OP_ABx(op, a, bx)                 \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   ((bx & BIFROST_INST_RBx_MASK) << BIFROST_INST_RBx_OFFSET)

#define BIFROST_MAKE_INST_OP_AsBx(op, a, bx)                \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   (((bx + BIFROST_INST_RsBx_MAX) & BIFROST_INST_RsBx_MASK) << BIFROST_INST_RsBx_OFFSET)

/* NOTE(Shareef):
    This macro helps change / patch pieces of an instruction.

    inst : bfInstruction*;
    x    : can be one of - OP, RA, RB, RC, RBx, RsBx
    val  : integer (range depends on 'x').
 */
#define bfInst_patchX(inst, x, val) *(inst) = (*(inst) & ~(BIFROST_INST_##x##_MASK << BIFROST_INST_##x##_OFFSET)) | BIFROST_MAKE_INST_##x(val)

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_INSTRUCTION_OP_H */
