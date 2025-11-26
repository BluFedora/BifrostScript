/******************************************************************************/
/*!
 * @file   bifrost_vm_parser.h
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Handles the pasring of the languages grammar and uses
 *   the function builder to generate a function.
 *
 *   The output is a module with an executable function assuming the
 *   parser ran into no issues.
 *
 *   References:
 *     [http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/]
 *
 * @version 0.0.1
 * @date    2020-02-16
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#ifndef BIFROST_VM_PARSER_H
#define BIFROST_VM_PARSER_H

#include "bifrost_vm_lexer.h"  // size_t, BifrostLexer, bfToken, bool

#if __cplusplus
extern "C" {
#endif

typedef struct BifrostVM                BifrostVM;
typedef struct BifrostVMFunctionBuilder BifrostVMFunctionBuilder;
typedef struct BifrostObjModule         BifrostObjModule;
typedef struct BifrostObjClass          BifrostObjClass;
typedef struct BifrostObjFn             BifrostObjFn;
typedef struct LoopInfo                 LoopInfo;

typedef struct BifrostParser
{
  struct BifrostParser*     parent;
  BifrostLexer*             lexer;
  bfToken                   current_token;
  BifrostVMFunctionBuilder* fn_builder_stack;
  BifrostVMFunctionBuilder* fn_builder;
  BifrostObjModule*         current_module;
  BifrostObjClass*          current_clz;
  BifrostVM*                vm;
  bool                      has_error;
  LoopInfo*                 loop_stack;

} BifrostParser;

void bfParser_ctor(BifrostParser* self, struct BifrostVM* vm, BifrostLexer* lexer, struct BifrostObjModule* current_module);
bool bfParser_compile(BifrostParser* self);
void bfParser_dtor(BifrostParser* self);

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_PARSER_H */
