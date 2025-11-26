/******************************************************************************/
/*!
 * @file   bifrost_vm_parser.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Handles the parsing of the languages grammar and uses
 *   the function builder to generate a function.
 *
 *   The output is a module with an executable function assuming the
 *   parser ran into no issues.
 *
 *   References:
 *     [Pratt Parsers: Expression Parsing Made Easy](http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/)
 *     [Simple but Powerful Pratt Parsing](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#include "bifrost_vm_parser.h"

#include "bifrost/bifrost_vm.h"  // bfVM_SetLastError

#include "bifrost_vm_debug.h"
#include "bifrost_vm_function_builder.h"
#include "bifrost_vm_gc.h"
#include "bifrost_vm_obj.h"

#define Parser_EmitError(self, fmt, ...)                                                                             \
  if (!self->has_error)                                                                                              \
  {                                                                                                                  \
    bfVM_SetLastError(BIFROST_VM_ERROR_COMPILE, (self)->vm, (int)self->lexer->current_line_no, (fmt), ##__VA_ARGS__) \
  }                                                                                                                  \
  self->has_error = true

extern uint32_t          bfVM_getSymbol(BifrostVM* self, string_range name);
extern BifrostObjModule* bfVM_findModule(BifrostVM* self, const char* name, size_t name_len);
extern bfVMValue         bfVM_stackFindVariable(BifrostObjModule* module_obj, const char* variable, size_t variable_len);
extern BifrostObjModule* bfVM_importModule(BifrostVM* self, const char* from, const char* name, size_t name_len);

/*  */

uint16_t bfVM_xSetVariable(BifrostVMSymbol** variables, BifrostVM* vm, string_range name, bfVMValue value)
{
  const size_t idx      = bfVM_getSymbol(vm, name);
  const size_t old_size = bfVMArray_size(variables);

  if (idx >= old_size)
  {
    const size_t new_size = idx + 1;

    bfVMArray_resize(vm, variables, new_size);

    for (size_t i = old_size; i < new_size; ++i)
    {
      (*variables)[i].name  = NULL;
      (*variables)[i].value = bfVMValue_fromNull();
    }
  }

  (*variables)[idx].name  = vm->symbols[idx];
  (*variables)[idx].value = value;

  return idx & 0xFFFF;
}

static uint16_t parserGetSymbol(const BifrostParser* const self, const string_range name)
{
  return (uint16_t)bfVM_getSymbol(self->vm, name);
}

#define BIFROST_VAR_LOCATION_BITS 15

static const uint16_t BIFROST_VM_INVALID_SLOT = (1 << BIFROST_VAR_LOCATION_BITS) - 1;

/* Variable Info */

typedef enum VariableKind
{
  V_LOCAL,
  V_MODULE,

} VariableKind;

typedef struct VariableInfo
{
  uint16_t kind : 1;                             /* VariableKind                                        */
  uint16_t location : BIFROST_VAR_LOCATION_BITS; /* V_LOCAL => register index, V_MODULE => symbol index */

} VariableInfo;

static VariableInfo VariableInfo_temp(const uint16_t temp_loc)
{
  VariableInfo ret;
  ret.kind     = V_LOCAL;
  ret.location = temp_loc;

  return ret;
}

static VariableInfo VariableInfo_declareLocal(const BifrostParser* const self, string_range name)
{
  VariableInfo var;
  var.kind     = V_LOCAL;
  var.location = (uint16_t)bfFuncBuilder_declVariable(self->fn_builder, name.str_bgn, name.str_len);

  return var;
}

static VariableInfo VariableInfo_local(const BifrostParser* const self, const string_range name)
{
  VariableInfo var;
  var.kind     = V_LOCAL;
  var.location = (uint16_t)bfFuncBuilder_getVariable(self->fn_builder, name.str_bgn, name.str_len);

  return var;
}

static VariableInfo VariableInfo_LocalOrSymbol(const BifrostParser* const self, const string_range name)
{
  VariableInfo var = VariableInfo_local(self, name);

  if (var.location == BIFROST_VM_INVALID_SLOT)
  {
    var.kind     = V_MODULE;
    var.location = parserGetSymbol(self, name);
  }

  return var;
}

/* Expr Grammar Rules */

// https://en.wikipedia.org/wiki/Order_of_operations#Programming_languages
//
// Higher means takes more precedence.
typedef enum Precedence
{
  PREC_NONE,        //
  PREC_ASSIGN,      // = += -= /= *=
  PREC_OR,          // ||
  PREC_AND,         // &&
  PREC_EQUALITY,    // == !=
  PREC_TERNARY,     // ? :
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // - !
  PREC_PREFIX,      // ++x
  PREC_POSTFIX,     // x++
  PREC_CALL         // . () [] :

} Precedence;

typedef struct ExprInfo
{
  uint16_t     write_loc;
  VariableInfo var;

} ExprInfo;

typedef void (*PrefixParseletFn)(BifrostParser* parser, ExprInfo* expr_info, const bfToken* token);
typedef void (*InfixParseletFn)(BifrostParser* parser, ExprInfo* expr_info, const ExprInfo* lhs, const bfToken* token, const Precedence prec);

typedef struct GrammarRule
{
  PrefixParseletFn prefix;
  InfixParseletFn  infix;
  Precedence       precedence;

} GrammarRule;

static ExprInfo exprMake(uint16_t write_loc, VariableInfo variable)
{
  ExprInfo ret;
  ret.write_loc = write_loc;
  ret.var       = variable;

  return ret;
}

static ExprInfo exprMakeTemp(uint16_t temp_loc)
{
  return exprMake(temp_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
}

/* Loop Info */

struct LoopInfo
{
  size_t    body_start;
  LoopInfo* parent;
};

static void loopPush(BifrostParser* const self, LoopInfo* const loop)
{
  loop->parent     = self->loop_stack;
  self->loop_stack = loop;
  loop->body_start = bfVMArray_size(&self->fn_builder->instructions);
}

static void loopPop(BifrostParser* const self)
{
  LibC_assert(self->loop_stack != NULL, "Invalid Loop Pop");

  const size_t body_end = bfVMArray_size(&self->fn_builder->instructions);

  for (size_t i = self->loop_stack->body_start; i < body_end; ++i)
  {
    bfInstruction* const inst = self->fn_builder->instructions + i;

    // NOTE(SR): Patch up break statements.
    if (*inst == BIFROST_INST_INVALID)
    {
      const size_t offset_to_end_of_loop = body_end - i;

      *inst = BIFROST_MAKE_INST_OP_AsBx(BIFROST_VM_OP_JUMP, 0, offset_to_end_of_loop);
    }
  }

  self->loop_stack = self->loop_stack->parent;
}

static bool Parser_parseStatement(BifrostParser* const self);

/* Jump Helpers */

static uint32_t parserMakeJump(BifrostParser* const self)
{
  const uint32_t jump_idx = (uint32_t)bfVMArray_size(&self->fn_builder->instructions);
  bfFuncBuilder_addInstAsBx(self->fn_builder, BIFROST_VM_OP_JUMP, 0, 0);
  return jump_idx;
}

static uint32_t parserMakeJumpRev(BifrostParser* const self)
{
  return (uint32_t)bfVMArray_size(&self->fn_builder->instructions);
}

static inline void parserPatchJumpHelper(BifrostParser* const self, uint32_t jump_idx, uint32_t cond_var, int jump_amt, bool if_not)
{
  bfInstruction* const inst = self->fn_builder->instructions + jump_idx;

  if (cond_var == BIFROST_VM_INVALID_SLOT)
  {
    *inst = BIFROST_MAKE_INST_OP_AsBx(BIFROST_VM_OP_JUMP, 0, jump_amt);
  }
  else
  {
    *inst = BIFROST_MAKE_INST_OP_AsBx(
     (if_not ? BIFROST_VM_OP_JUMP_IF_NOT : BIFROST_VM_OP_JUMP_IF),
     cond_var,
     jump_amt);
  }
}

static void parserPatchJump(BifrostParser* const self, uint32_t jump_idx, uint32_t cond_var, bool if_not)
{
  const uint32_t current_loc = (uint32_t)bfVMArray_size(&self->fn_builder->instructions);

  parserPatchJumpHelper(self, jump_idx, cond_var, (int)current_loc - (int)jump_idx, if_not);
}

static void parserPatchJumpRev(BifrostParser* const self, uint32_t jump_idx, uint32_t cond_var, bool if_not)
{
  const uint32_t current_loc = (uint32_t)bfVMArray_size(&self->fn_builder->instructions);

  bfFuncBuilder_addInstAsBx(self->fn_builder, BIFROST_VM_OP_JUMP, 0, 0);
  parserPatchJumpHelper(self, current_loc, cond_var, (int)jump_idx - (int)current_loc, if_not);
}

/* Variable Load / Store */

static void parserVariableLoad(BifrostParser* const self, VariableInfo variable, uint16_t write_loc)
{
  LibC_assert(variable.location != BIFROST_VM_INVALID_SLOT, "");
  LibC_assert(write_loc != BIFROST_VM_INVALID_SLOT, "");

  switch (variable.kind)
  {
    case V_LOCAL:
    {
      /* @Optimization: Redundant store removal. */
      if (write_loc != variable.location)
      {
        bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_STORE_MOVE, write_loc, variable.location);
      }
      break;
    }
    case V_MODULE:
    {
      const uint16_t module_expr = bfFuncBuilder_pushTemp(self->fn_builder, 1);

      bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, module_expr, BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE);
      bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_LOAD_SYMBOL, write_loc, module_expr, variable.location);

      bfFuncBuilder_popTemp(self->fn_builder, module_expr);
      break;
    }
    default:
    {
      LibC_assert(false, "parserLoadVariable invalid variable type.");
      break;
    }
  }
}

static void parserVariableStore(BifrostParser* const self, VariableInfo variable, uint16_t read_loc)
{
  LibC_assert(variable.location != BIFROST_VM_INVALID_SLOT, "");
  LibC_assert(read_loc != BIFROST_VM_INVALID_SLOT, "");

  switch (variable.kind)
  {
    case V_LOCAL:
    {
      bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_STORE_MOVE, variable.location, read_loc);
      break;
    }
    case V_MODULE:
    {
      const uint16_t module_expr = bfFuncBuilder_pushTemp(self->fn_builder, 1);

      bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, module_expr, BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE);
      bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_STORE_SYMBOL, module_expr, variable.location, read_loc);

      bfFuncBuilder_popTemp(self->fn_builder, module_expr);
      break;
    }
    default:
    {
      LibC_assert(false, "parserLoadVariable invalid variable type.");
      break;
    }
  }
}

static bfVMValue parserTokenConstexprValue(const BifrostParser* const self, const bfToken* token)
{
  /* TODO(SR): The call to 'bfVM_createString' should be delayed, interning the string would save on memory. */

  switch (token->type)
  {
    case BIFROST_TOKEN_CONST_REAL: return bfVMValue_fromNumber(token->num);
    case BIFROST_TOKEN_CONST_BOOL: return bfVMValue_fromBool(token->str_range.str_bgn[0] == 't');
    case BIFROST_TOKEN_CONST_STR:  return bfVMValue_fromPointer(bfObj_NewString(self->vm, token->str_range));
    case BIFROST_TOKEN_CONST_NIL:  return bfVMValue_fromNull();
    default:                       break;
  }

  LibC_assert(false, "parserConstexprValue called on non constexpr token.");
  return 0;
}

/* Function Builder Stack */

static void bfParser_pushBuilder(BifrostParser* const self, const char* fn_name, size_t fn_name_len)
{
  self->fn_builder = bfVMArray_emplace(self->vm, &self->fn_builder_stack);
  bfFuncBuilder_ctor(self->fn_builder, self->lexer);
  bfFuncBuilder_begin(self->fn_builder, fn_name, fn_name_len);
}

static void bfParser_popBuilder(BifrostParser* const self, BifrostObjFn* fn_out, int arity)
{
  bfFuncBuilder_end(self->fn_builder, fn_out, arity);

  // bfDbg_DisassembleFunction(0, fn_out);

  bfFuncBuilder_dtor(self->fn_builder);
  bfVMArray_pop(&self->fn_builder_stack);
  self->fn_builder = bfVMArray_back(&self->fn_builder_stack);
}

/* Parser Token Helpers */

static inline bool bfParser_eat(BifrostParser* const self, bfTokenType type, bool is_optional, const char* error_msg)
{
  if (self->current_token.type == type)
  {
    self->current_token = bfLexer_nextToken(self->lexer);
    return true;
  }

  if (!is_optional)
  {
    // const char*  expected  = bfDbg_TokenTypeToString(type);
    // const char*  received  = bfDbg_TokenTypeToString(self->current_token.type);

    Parser_EmitError(self, "%s", error_msg);

    while (self->current_token.type != BIFROST_TOKEN_SEMI_COLON && self->current_token.type != BIFROST_TOKEN_EOP)
    {
      self->current_token = bfLexer_nextToken(self->lexer);
    }

    self->has_error = true;
  }

  return false;
}

static inline bool bfParser_match(BifrostParser* const self, bfTokenType type)
{
  // TODO(SR): Report error on "self->current_token.type == BIFROST_TOKEN_EOP"
  return bfParser_eat(self, type, true, NULL) || self->current_token.type == BIFROST_TOKEN_EOP;
}

static inline bool bfParser_is(BifrostParser* const self, bfTokenType type)
{
  return self->current_token.type == type || self->current_token.type == BIFROST_TOKEN_EOP;
}

/* Expression Parsing */

static inline GrammarRule typeToRule(const bfTokenType type);

// Pratt Parser
//   [Vaughan Pratt - Top Down Operator Precendence 1973](reference/Vaughan.Pratt.TDOP.pdf | https://tdop.github.io/)
//
//   [Pratt Parsing and Precedence Climbing Are the Same Algorithm](https://www.oilshell.org/blog/2016/11/01.html)
//
static void parseExpr(BifrostParser* const self, ExprInfo* expr_loc, const Precedence minimum_prec)
{
  bfToken           token = self->current_token;
  const GrammarRule rule  = typeToRule(token.type);

  if (!rule.prefix)
  {
    Parser_EmitError(self, "No prefix operator for token: %s", bfDbg_TokenTypeToString(token.type));
    return;
  }

  bfParser_match(self, token.type);

  rule.prefix(self, expr_loc, &token);

  while (minimum_prec < typeToRule(self->current_token.type).precedence)
  {
    token                       = self->current_token;
    const InfixParseletFn infix = typeToRule(token.type).infix;

    if (!infix)
    {
      Parser_EmitError(self, "No infix operator for token: %s", bfDbg_TokenTypeToString(token.type));
      return;
    }

    bfParser_match(self, token.type);

    infix(self, expr_loc, expr_loc, &token, typeToRule(token.type).precedence);
  }
}

/* Function Call Helpers */

static uint16_t FunctionCall_parseParameters(BifrostParser* const self, uint16_t temp_first, uint16_t num_params, bfTokenType end_token)
{
  if (!bfParser_is(self, end_token))
  {
    do
    {
      const uint16_t param_loc  = num_params == 0 ? temp_first : bfFuncBuilder_pushTemp(self->fn_builder, 1);
      ExprInfo       param_expr = exprMakeTemp(param_loc);

      parseExpr(self, &param_expr, PREC_NONE);

      ++num_params;

    } while (bfParser_match(self, BIFROST_TOKEN_COMMA));
  }

  return num_params;
}

static void FunctionCall_finish(BifrostParser* const self, VariableInfo fn, VariableInfo return_loc, uint32_t zero_slot)
{
  const bool     is_local_fn  = fn.kind == V_LOCAL;
  const uint16_t function_loc = is_local_fn ? (uint16_t)fn.location : bfFuncBuilder_pushTemp(self->fn_builder, 1);

  if (!is_local_fn)
  {
    parserVariableLoad(self, fn, function_loc);
  }

  uint16_t       num_params = 0;
  const uint16_t temp_first = bfFuncBuilder_pushTemp(self->fn_builder, 1);

  if (zero_slot != BIFROST_VM_INVALID_SLOT)
  {
    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_STORE_MOVE, temp_first, zero_slot);
    ++num_params;
  }

  num_params = FunctionCall_parseParameters(self, temp_first, num_params, BIFROST_TOKEN_R_PAREN);
  bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "Function call must end with a closing parenthesis.");

  bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_CALL_FN, temp_first, function_loc, num_params);

  if (return_loc.location != BIFROST_VM_INVALID_SLOT)
  {
    parserVariableStore(self, return_loc, temp_first);
  }

  bfFuncBuilder_popTemp(self->fn_builder, (is_local_fn ? temp_first : function_loc));
}

static void parseVarDecl(BifrostParser* const self, const bool is_static)
{
  /* GRAMMAR(SR):
       var <identifier>;
       var <identifier> = <expr>;
       static var <identifier>;
       static var <identifier> = <expr>;
  */

  bfParser_match(self, BIFROST_TOKEN_VAR);

  const string_range name = self->current_token.str_range;

  if (bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Expected identifier after var keyword."))
  {
    if (is_static)
    {
      const uint16_t location = bfVM_xSetVariable(&self->current_module->variables, self->vm, name, bfVMValue_fromNull());

      if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
      {
        VariableInfo var;
        var.kind     = V_MODULE;
        var.location = location;

        const uint16_t expr_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);
        ExprInfo       expr     = exprMakeTemp(expr_loc);
        parseExpr(self, &expr, PREC_NONE);
        parserVariableStore(self, var, expr_loc);
        bfFuncBuilder_popTemp(self->fn_builder, expr_loc);
      }
    }
    else
    {
      const VariableInfo var = VariableInfo_declareLocal(self, name);

      if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
      {
        ExprInfo expr = exprMakeTemp(var.location);
        parseExpr(self, &expr, PREC_NONE);
      }
    }

    bfParser_eat(self, BIFROST_TOKEN_SEMI_COLON, false, "Expected semi colon after variable declaration.");
  }
}

void bfParser_ctor(BifrostParser* const self, struct BifrostVM* vm, BifrostLexer* lexer, struct BifrostObjModule* current_module)
{
  self->parent           = vm->parser_stack;
  vm->parser_stack       = self;
  self->lexer            = lexer;
  self->current_token    = bfLexer_nextToken(lexer);
  self->fn_builder_stack = bfVMArray_new(vm, BifrostVMFunctionBuilder, 2);
  self->has_error        = false;
  self->current_clz      = NULL;
  self->loop_stack       = NULL;
  self->vm               = vm;
  self->current_module   = current_module;
  bfParser_pushBuilder(self, self->current_module->name, bfVMString_length(self->current_module->name));
}

bool bfParser_compile(BifrostParser* const self)
{
  while (Parser_parseStatement(self))
  {
  }

  return self->has_error;
}

void bfParser_dtor(BifrostParser* const self)
{
  self->vm->parser_stack = self->parent;

  BifrostObjFn* const module_fn = &self->current_module->init_fn;

  bfParser_popBuilder(self, module_fn, 0);
  bfVMArray_delete(self->vm, &self->fn_builder_stack);
}

static void parseBlock(BifrostParser* const self)
{
  /* GRAMMAR(SR):
       { <statement>... }
  */

  // TODO(SR): Add one liner blocks?
  bfParser_eat(self, BIFROST_TOKEN_L_CURLY, false, "Block must start with an opening curly boi.");

  bfFuncBuilder_pushScope(self->fn_builder);

  while (!bfParser_is(self, BIFROST_TOKEN_R_CURLY))
  {
    if (!Parser_parseStatement(self))
    {
      break;
    }
  }

  bfFuncBuilder_popScope(self->fn_builder);

  bfParser_eat(self, BIFROST_TOKEN_R_CURLY, false, "Block must end with an closing curly boi.");
}

static string_range parserBeginFunction(BifrostParser* const self, const bool require_name)
{
  string_range name_str = MakeString("__INVALID__");

  if (bfParser_is(self, BIFROST_TOKEN_IDENTIFIER))
  {
    name_str = self->current_token.str_range;
    bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Function name expected after 'func' keyword.");
  }
  else if (bfParser_is(self, BIFROST_TOKEN_L_SQR_BOI))
  {
    name_str.str_bgn = "[]";
    name_str.str_len = 2;

    bfParser_eat(self, BIFROST_TOKEN_L_SQR_BOI, false, "");
    bfParser_eat(self, BIFROST_TOKEN_R_SQR_BOI, false, "Closing square bracket must be after opening for function decl.");

    if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
    {
      name_str.str_bgn = "[]=";
      name_str.str_len = 3;
    }
  }
  else if (!require_name)
  {
    name_str.str_bgn = NULL;
    name_str.str_len = 0;
  }
  else
  {
    Parser_EmitError(self, "An identifier, \"[]\" or \"[]=\" is expected after 'func' keyword.");
  }

  bfParser_pushBuilder(self, name_str.str_bgn, string_range_length(name_str));
  return name_str;
}

/* Function Parsing */

static int parserParseFunction(BifrostParser* const self)
{
  /* GRAMMAR(SR):
      func <identifier>(<identifier>,...) { ... }
      func <identifier>() { ... }
      func []()  { ... }
      func [](<identifier>,...)  { ... }
      func []=() { ... }
      func []=(<identifier>,...) { ... }
  */

  int arity = 0;
  bfParser_eat(self, BIFROST_TOKEN_L_PAREN, false, "Expected parameter list after function name.");

  while (!bfParser_is(self, BIFROST_TOKEN_R_PAREN))
  {
    const string_range param_str = self->current_token.str_range;
    bfFuncBuilder_declVariable(self->fn_builder, param_str.str_bgn, string_range_length(param_str));

    bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Parameter names must be a word and not a keyword.");
    bfParser_eat(self, BIFROST_TOKEN_COMMA, true, "The last comma isn't needed but this allows a func (a b c) syntax.");

    ++arity;
  }

  bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "Function must have a body.");
  parseBlock(self);
  bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);
  return arity;
}

static void parseFunctionDecl(BifrostParser* const self)
{
  bfParser_match(self, BIFROST_TOKEN_FUNC);

  const bool          is_local = bfVMArray_size(&self->fn_builder_stack) != 1;
  const string_range  name_str = parserBeginFunction(self, true);
  const int           arity    = parserParseFunction(self);
  BifrostObjFn* const fn       = bfObj_NewFunction(self->vm, self->current_module);
  const bfVMValue     fn_value = bfVMValue_fromPointer(fn);
  bfParser_popBuilder(self, fn, arity);

  if (is_local)
  {
    const VariableInfo fn_var = VariableInfo_declareLocal(self, name_str);
    const uint32_t     k_loc  = bfFuncBuilder_addConstant(self->fn_builder, fn_value);
    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, (uint16_t)fn_var.location, BIFROST_VM_OP_LOAD_BASIC_CONSTANT + k_loc);
  }
  else
  {
    bfVM_xSetVariable(&self->current_module->variables, self->vm, name_str, fn_value);
  }
}

static void Expr_parseFunctionExpr(BifrostParser* const self, ExprInfo* expr, const bfToken* token)
{
  (void)token;

  parserBeginFunction(self, false);
  const int           arity = parserParseFunction(self);
  BifrostObjFn* const fn    = bfObj_NewFunction(self->vm, self->current_module);
  bfParser_popBuilder(self, fn, arity);

  const uint32_t k_loc = bfFuncBuilder_addConstant(self->fn_builder, bfVMValue_fromPointer(fn));
  bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, expr->write_loc, BIFROST_VM_OP_LOAD_BASIC_CONSTANT + k_loc);
}

static void parseImport(BifrostParser* const self)
{
  /* GRAMMAR(SR):
      import <const-string> for <identifier>, <identifier> (= | as) <identifier>, ...;

      import <const-string>;
  */

  bfParser_match(self, BIFROST_TOKEN_IMPORT);

  const bfToken      name_token = self->current_token;
  const string_range name_str   = name_token.str_range;
  bfParser_eat(self, BIFROST_TOKEN_CONST_STR, false, "Import statments must be followed by a constant string.");

  BifrostObjModule* const imported_module = bfVM_importModule(self->vm, self->current_module->name, name_str.str_bgn, string_range_length(name_str));

  if (imported_module == NULL)
  {
    Parser_EmitError(self, "Failed to import module: '%.*s'", string_range_length(name_str), name_str.str_bgn);
  }

  if (bfParser_match(self, BIFROST_TOKEN_CTRL_FOR))
  {
    do
    {
      const string_range var_str = self->current_token.str_range;
      bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Imported variable name must be an identifier.");

      const string_range src_name = var_str;
      string_range       dst_name = var_str;

      if (bfParser_match(self, BIFROST_TOKEN_EQUALS) || bfParser_match(self, BIFROST_TOKEN_AS))
      {
        dst_name = self->current_token.str_range;
        bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Import alias must be an identifier.");
      }

      if (imported_module)
      {
        bfVM_xSetVariable(&self->current_module->variables, self->vm, dst_name, bfVM_stackFindVariable(imported_module, src_name.str_bgn, string_range_length(src_name)));
      }
    } while (bfParser_match(self, BIFROST_TOKEN_COMMA));
  }
  else if (imported_module != NULL)
  {
    const size_t variable_count = bfVMArray_size(&imported_module->variables);

    for (size_t variable_index = 0; variable_index < variable_count; ++variable_index)
    {
      const BifrostVMSymbol* const module_symbol = imported_module->variables + variable_index;

      if (module_symbol->name == NULL)
      {
        continue;
      }

      const string_range variable_name = MakeStringLen(module_symbol->name, bfVMString_length(module_symbol->name));

      // TODO(SR): The way symbols are stored is dumb and leaves lots of empty slots.
      if (!bfVMValue_isNull(module_symbol->value))
      {
        bfVM_xSetVariable(&self->current_module->variables, self->vm, variable_name, module_symbol->value);
      }
    }
  }

  bfParser_eat(self, BIFROST_TOKEN_SEMI_COLON, false, "Import must end with a semi-colon.");
}

static void parseForStatement(BifrostParser* const self)
{
  /* GRAMMAR(SR):
      for (
        <statement> | <none>;
        <expr>      | <none>;
        <statement> | <none>) {}
      (; | <none>)
  */
  /*
    // This gets compiled into roughly:
    //
    // <statement>;
    //
    // label_cond:
    //   if (<cond>)
    //     goto label_loop;
    //   else
    //     goto label_loop_end;
    //
    // label_inc:
    //   <increment>
    //   goto label_cond;
    //
    // label_loop:
    //   <statements>...;
    //   goto label_inc;
    // label_loop_end:
    //
  */

  bfParser_eat(self, BIFROST_TOKEN_L_PAREN, false, "Expected '(' after 'for' keyword.");

  bfFuncBuilder_pushScope(self->fn_builder);
  {
    LoopInfo loop;

    if (!bfParser_match(self, BIFROST_TOKEN_SEMI_COLON))
    {
      Parser_parseStatement(self);
    }

    const uint32_t inc_to_cond = parserMakeJumpRev(self);
    const uint16_t cond_loc    = bfFuncBuilder_pushTemp(self->fn_builder, 1);

    if (!bfParser_is(self, BIFROST_TOKEN_SEMI_COLON))
    {
      ExprInfo cond_expr = exprMake(cond_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
      parseExpr(self, &cond_expr, PREC_NONE);
    }
    else
    {
      bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, cond_loc, BIFROST_VM_OP_LOAD_BASIC_TRUE);
    }

    const uint32_t cond_to_loop = parserMakeJump(self);
    const uint32_t cond_to_end  = parserMakeJump(self);

    bfFuncBuilder_popTemp(self->fn_builder, cond_loc);

    bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);

    const uint32_t loop_to_inc = parserMakeJumpRev(self);
    if (!bfParser_match(self, BIFROST_TOKEN_R_PAREN))
    {
      Parser_parseStatement(self);
      bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "Expected ( list after for loop.");
    }
    parserPatchJumpRev(self, inc_to_cond, BIFROST_VM_INVALID_SLOT, false);

    parserPatchJump(self, cond_to_loop, cond_loc, false);
    loopPush(self, &loop);
    parseBlock(self);
    parserPatchJumpRev(self, loop_to_inc, BIFROST_VM_INVALID_SLOT, false);

    parserPatchJump(self, cond_to_end, cond_loc, true);
    loopPop(self);
  }
  bfFuncBuilder_popScope(self->fn_builder);
  bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);
}

// Expr Parsers

static void Expr_parseGroup(BifrostParser* const self, ExprInfo* expr_info, const bfToken* token)
{
  (void)token;
  parseExpr(self, expr_info, PREC_NONE);
  bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "Missing closing parenthesis for an group expression.");
}

static void bfParser_loadConstant(BifrostParser* const self, ExprInfo* expr_info, bfVMValue value)
{
  const uint32_t const_loc = bfFuncBuilder_addConstant(self->fn_builder, value);

  bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, expr_info->write_loc, const_loc + BIFROST_VM_OP_LOAD_BASIC_CONSTANT);
}

static void Expr_parseLiteral(BifrostParser* const self, ExprInfo* expr_info, const bfToken* token)
{
  const bfVMValue constexpr_value = parserTokenConstexprValue(self, token);

  if (bfVMValue_isTrue(constexpr_value))
  {
    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, expr_info->write_loc, BIFROST_VM_OP_LOAD_BASIC_TRUE);
  }
  else if (bfVMValue_isFalse(constexpr_value))
  {
    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, expr_info->write_loc, BIFROST_VM_OP_LOAD_BASIC_FALSE);
  }
  else if (bfVMValue_isNull(constexpr_value))
  {
    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_LOAD_BASIC, expr_info->write_loc, BIFROST_VM_OP_LOAD_BASIC_NULL);
  }
  else
  {
    bfParser_loadConstant(self, expr_info, constexpr_value);
  }
}

static void Expr_parseNew(BifrostParser* const self, ExprInfo* expr, const bfToken* token)
{
  (void)token;

  const string_range clz_name = self->current_token.str_range;

  if (bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "'new' must be called on a class name."))
  {
    const VariableInfo clz_var = VariableInfo_LocalOrSymbol(self, clz_name);
    const uint16_t     clz_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);

    parserVariableLoad(self, clz_var, clz_loc);

    bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_NEW_CLZ, expr->write_loc, clz_loc);

    string_range ctor_name = MakeString("ctor");

    if (bfParser_match(self, BIFROST_TOKEN_DOT))
    {
      if (bfParser_is(self, BIFROST_TOKEN_IDENTIFIER))
      {
        ctor_name = self->current_token.str_range;
      }

      bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Expected the name of the constructor to call.");
    }

    if (bfParser_match(self, BIFROST_TOKEN_L_PAREN))
    {
      const uint16_t     ctor_sym     = parserGetSymbol(self, ctor_name);
      const VariableInfo function_var = VariableInfo_temp(clz_loc);
      const VariableInfo return_var   = VariableInfo_temp(BIFROST_VM_INVALID_SLOT);

      bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_LOAD_SYMBOL, clz_loc, clz_loc, ctor_sym);

      FunctionCall_finish(self, function_var, return_var, expr->write_loc);
    }

    bfFuncBuilder_popTemp(self->fn_builder, clz_loc);
  }
}

static void Expr_parseSuper(BifrostParser* const self, ExprInfo* expr, const bfToken* token)
{
  (void)token;

  if (self->current_clz)
  {
    if (self->current_clz->base_clz)
    {
      bfParser_loadConstant(self, expr, bfVMValue_fromPointer(self->current_clz->base_clz));
    }
    else
    {
      Parser_EmitError(self, "'super' keyword can only be used on Classes with a Base Class.\n");
    }
  }
  else
  {
    Parser_EmitError(self, "'super' keyword can only be used in class methods.\n");
  }
}

static void Expr_parseVariable(BifrostParser* const self, ExprInfo* expr, const bfToken* token)
{
  const string_range var_name = token->str_range;
  VariableInfo       var      = expr->var;

  if (var.location == BIFROST_VM_INVALID_SLOT)
  {
    var = VariableInfo_LocalOrSymbol(self, var_name);
  }

  if (var.location != BIFROST_VM_INVALID_SLOT)
  {
    parserVariableLoad(self, var, expr->write_loc);
    (*expr) = exprMake(expr->write_loc, var);
  }
  else
  {
    Parser_EmitError(self, "Error invalid var(%.*s)", (int)(var_name.str_len), var_name.str_bgn);
  }
}

static void Expr_parseBinOp(BifrostParser* const self, ExprInfo* expr_info, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  bfInstructionOp inst   = BIFROST_VM_OP_CMP_EE;
  const char      bin_op = token->str_range.str_bgn[0];

  switch (bin_op)
  {
    case '=': inst = BIFROST_VM_OP_CMP_EE; break;
    case '!': inst = BIFROST_VM_OP_CMP_NE; break;
    case '+': inst = BIFROST_VM_OP_MATH_ADD; break;
    case '-': inst = BIFROST_VM_OP_MATH_SUB; break;
    case '*': inst = BIFROST_VM_OP_MATH_MUL; break;
    case '/': inst = BIFROST_VM_OP_MATH_DIV; break;
    case '%': inst = BIFROST_VM_OP_MATH_MOD; break;
    case '^': inst = BIFROST_VM_OP_MATH_POW; break;
    case '|': inst = BIFROST_VM_OP_CMP_OR; break;
    case '&': inst = BIFROST_VM_OP_CMP_AND; break;
    case '<': inst = token->type == BIFROST_TOKEN_CTRL_LE ? BIFROST_VM_OP_CMP_LE : BIFROST_VM_OP_CMP_LT; break;
    case '>': inst = token->type == BIFROST_TOKEN_CTRL_GE ? BIFROST_VM_OP_CMP_GE : BIFROST_VM_OP_CMP_GT; break;
    default:  Parser_EmitError(self, "Invalid Binary Operator. %.*s", (int)token->str_range.str_len, token->str_range.str_bgn); break;
  }

  const uint16_t rhs_loc  = bfFuncBuilder_pushTemp(self->fn_builder, 1);
  ExprInfo       rhs_expr = exprMake(rhs_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
  const uint32_t jmp      = bin_op == '&' || bin_op == '|' ? parserMakeJump(self) : BIFROST_VM_INVALID_SLOT;

  parseExpr(self, &rhs_expr, prec);

  bfFuncBuilder_addInstABC(self->fn_builder, inst, expr_info->write_loc, lhs->write_loc, rhs_loc);

  if (jmp != BIFROST_VM_INVALID_SLOT)
  {
    parserPatchJump(self, jmp, expr_info->write_loc, bin_op == '&');
  }

  bfFuncBuilder_popTemp(self->fn_builder, rhs_loc);
}

static void Expr_parseSubscript(BifrostParser* const self, ExprInfo* expr, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  (void)lhs;
  (void)token;
  (void)prec;

  // TODO(SR):
  //   Find some way to merge this into the main function call routine.
  const uint16_t subscript_op_loc = bfFuncBuilder_pushTemp(self->fn_builder, 3);
  const uint16_t self_loc         = subscript_op_loc + 1;
  const uint16_t temp_first       = subscript_op_loc + 2;
  const uint16_t subscript_sym    = parserGetSymbol(self, MakeString("[]"));
  uint16_t       num_args         = 1;

  parserVariableLoad(self, expr->var, self_loc);

  const uint32_t load_sym_inst = (uint32_t)bfVMArray_size(&self->fn_builder->instructions);

  bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_LOAD_SYMBOL, subscript_op_loc, self_loc, (uint16_t)subscript_sym);
  bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_STORE_MOVE, temp_first, self_loc);

  num_args = FunctionCall_parseParameters(self, temp_first, num_args, BIFROST_TOKEN_R_SQR_BOI);

  bfParser_eat(self, BIFROST_TOKEN_R_SQR_BOI, false, "Subscript call must end with a closing square bracket.");

  if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
  {
    const size_t   subscript_assign_sym = parserGetSymbol(self, MakeString("[]="));
    bfInstruction* inst                 = self->fn_builder->instructions + load_sym_inst;

    bfInst_patchX(inst, OP, BIFROST_VM_OP_LOAD_SYMBOL);
    bfInst_patchX(inst, RC, subscript_assign_sym);

    const uint16_t param_loc  = bfFuncBuilder_pushTemp(self->fn_builder, 1);
    ExprInfo       param_expr = exprMakeTemp(param_loc);
    parseExpr(self, &param_expr, PREC_NONE);
    ++num_args;
  }

  bfFuncBuilder_addInstABC(self->fn_builder, BIFROST_VM_OP_CALL_FN, temp_first, subscript_op_loc, num_args);
  bfFuncBuilder_addInstABx(self->fn_builder, BIFROST_VM_OP_STORE_MOVE, expr->write_loc, temp_first);

  bfFuncBuilder_popTemp(self->fn_builder, subscript_op_loc);
}

static void Expr_parseDotOp(BifrostParser* const self, ExprInfo* expr, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  // token               = Grammar Rule Association.
  // self->current_token = rhs token
  // expr->token         = lhs token

  static const bool isRightAssoc = true;

  if (self->current_token.type == BIFROST_TOKEN_IDENTIFIER)
  {
    const bfToken field = self->current_token;
    const size_t  sym   = parserGetSymbol(self, field.str_range);

    bfFuncBuilder_addInstABC(
     self->fn_builder,
     BIFROST_VM_OP_LOAD_SYMBOL,
     expr->write_loc,
     lhs->write_loc,
     (uint16_t)sym);

    const VariableInfo lhs_var = lhs->var;

    *expr              = exprMakeTemp(expr->write_loc);
    expr->var.location = expr->write_loc;

    parseExpr(self, expr, prec - (isRightAssoc ? 1 : 0));

    if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
    {
      const uint16_t rhs_loc = bfFuncBuilder_pushTemp(self->fn_builder, 2);
      const uint16_t var_loc = rhs_loc + 1;

      ExprInfo rhs_expr = exprMakeTemp(rhs_loc);

      parseExpr(self, &rhs_expr, PREC_ASSIGN);

      parserVariableLoad(self, lhs_var, var_loc);

      bfFuncBuilder_addInstABC(
       self->fn_builder,
       BIFROST_VM_OP_STORE_SYMBOL,
       var_loc,
       (uint16_t)sym,
       rhs_expr.write_loc);

      bfFuncBuilder_popTemp(self->fn_builder, rhs_loc);
    }
  }
  else
  {
    Parser_EmitError(self, "(%s) Cannot use the dot operator on non variables.\n", bfDbg_TokenTypeToString(token->type));
  }
}

static void Expr_parseAssign(BifrostParser* const self, ExprInfo* expr, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  (void)expr;
  (void)token;

  const uint16_t rhs_loc  = bfFuncBuilder_pushTemp(self->fn_builder, 1);
  ExprInfo       rhs_expr = exprMakeTemp(rhs_loc);
  parseExpr(self, &rhs_expr, prec);
  parserVariableStore(self, lhs->var, rhs_loc);
  bfFuncBuilder_popTemp(self->fn_builder, rhs_loc);
}

static void Expr_parseCall(BifrostParser* const self, ExprInfo* expr, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  (void)expr;
  (void)lhs;
  (void)token;
  (void)prec;

  const uint16_t function_loc      = bfFuncBuilder_pushTemp(self->fn_builder, 1);
  const uint16_t real_function_loc = lhs->var.kind == V_LOCAL ? lhs->var.location : function_loc;

  if (lhs->var.kind != V_LOCAL)
  {
    parserVariableLoad(self, lhs->var, function_loc);
  }

  const VariableInfo function_var = VariableInfo_temp(real_function_loc);
  const VariableInfo return_var   = VariableInfo_temp(expr->write_loc);

  FunctionCall_finish(self, function_var, return_var, BIFROST_VM_INVALID_SLOT);

  bfFuncBuilder_popTemp(self->fn_builder, function_loc);
}

static void Expr_parseMethodCall(BifrostParser* const self, ExprInfo* expr, const ExprInfo* lhs, const bfToken* token, Precedence prec)
{
  (void)self;
  (void)expr;
  (void)lhs;
  (void)token;
  (void)prec;

  const string_range method_name = self->current_token.str_range;

  bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Function call must be done on an identifier.");

  const uint16_t function_loc = bfFuncBuilder_pushTemp(self->fn_builder, 2);
  const uint16_t var_loc      = function_loc + 1;
  const size_t   sym          = parserGetSymbol(self, method_name);
  const uint16_t real_var_loc = (lhs->var.kind == V_LOCAL) ? lhs->var.location : var_loc;

  if (lhs->var.kind != V_LOCAL)
  {
    parserVariableLoad(self, lhs->var, var_loc);
  }

  bfFuncBuilder_addInstABC(
   self->fn_builder,
   BIFROST_VM_OP_LOAD_SYMBOL,
   function_loc,
   real_var_loc,
   (uint16_t)sym);

  const VariableInfo function_var = VariableInfo_temp(function_loc);
  const VariableInfo return_var   = VariableInfo_temp(expr->write_loc);

  bfParser_eat(self, BIFROST_TOKEN_L_PAREN, false, "Function call must start with an open parenthesis.");
  FunctionCall_finish(self, function_var, return_var, real_var_loc);

  bfFuncBuilder_popTemp(self->fn_builder, function_loc);
}

static bool parserIsConstexpr(const BifrostParser* const self)
{
  switch (self->current_token.type)
  {
    case BIFROST_TOKEN_CONST_REAL:
    case BIFROST_TOKEN_CONST_BOOL:
    case BIFROST_TOKEN_CONST_STR:
    case BIFROST_TOKEN_CONST_NIL:
      return true;
    default:
      return false;
  }
}

/* Class Parsing */

static void parseClassVarDecl(BifrostParser* const self, BifrostObjClass* clz, const bool is_static)
{
  /* GRAMMAR(SR):
      var <identifier> = <constexpr>;
      var <identifier>;
  */

  const string_range name_str = self->current_token.str_range;

  bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Expected name after var keyword.");

  bfVMValue initial_value = bfVMValue_fromNull();

  if (bfParser_match(self, BIFROST_TOKEN_EQUALS))
  {
    if (parserIsConstexpr(self))
    {
      initial_value = parserTokenConstexprValue(self, &self->current_token);
      bfParser_match(self, self->current_token.type);
    }
    else
    {
      Parser_EmitError(self, "Variable initializer must be a constant expression.");
    }
  }

  if (is_static)
  {
    bfVM_xSetVariable(&clz->symbols, self->vm, name_str, initial_value);
  }
  else
  {
    const size_t     symbol   = parserGetSymbol(self, name_str);
    BifrostVMSymbol* var_init = bfVMArray_emplace(self->vm, &clz->field_initializers);
    var_init->name            = self->vm->symbols[symbol];
    var_init->value           = initial_value;
  }

  bfParser_eat(self, BIFROST_TOKEN_SEMI_COLON, false, "Expected semi-colon after variable declaration.");
}

static void parseClassFunc(BifrostParser* const self, BifrostObjClass* clz, bool is_static)
{
  const string_range name_str = parserBeginFunction(self, true);
  int                arity    = !is_static;

  if (!is_static)
  {
    bfFuncBuilder_declVariable(self->fn_builder, "self", 4);
  }

  arity += parserParseFunction(self);

  // TODO(Shareef): This same line is used in 3 (or more) places and should be put in a helper.
  BifrostObjFn* fn = bfObj_NewFunction(self->vm, self->current_module);
  bfVM_xSetVariable(&clz->symbols, self->vm, name_str, bfVMValue_fromPointer(fn));
  bfParser_popBuilder(self, fn, arity);
}

static void parseClassDecl(BifrostParser* const self)
{
  /* GRAMMAR(SR):
      class <identifier> : <identifier> { <class-decls>... || <none> };
      class <identifier> { <class-decls>... || <none> };
  */

  bfParser_match(self, BIFROST_TOKEN_CLASS);

  const bfToken      name_token = self->current_token;
  const string_range name_str   = name_token.str_range;
  BifrostObjClass*   base_clz   = NULL;

  bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Class name expected after 'class' keyword");

  if (bfParser_match(self, BIFROST_TOKEN_COLON))
  {
    const bfToken      base_name_token = self->current_token;
    const string_range base_name_str   = base_name_token.str_range;

    if (bfParser_eat(self, BIFROST_TOKEN_IDENTIFIER, false, "Class name expected after ':' to specify the base class."))
    {
      const bfVMValue base_class_val = bfVM_stackFindVariable(self->current_module, base_name_str.str_bgn, string_range_length(base_name_str));

      if (bfVMValue_isPointer(base_class_val))
      {
        BifrostObj* const base_class_obj = BIFROST_AS_OBJ(base_class_val);

        if (base_class_obj->type == BIFROST_VM_OBJ_CLASS)
        {
          base_clz = (BifrostObjClass*)base_class_obj;
        }
        else
        {
          Parser_EmitError(self,
                           "'%.*s' cannot be used as a base class for '%.*s' (non class type).",
                           string_range_length(base_name_str),
                           base_name_str.str_bgn,
                           string_range_length(name_str),
                           name_str.str_bgn);
        }
      }
      else
      {
        Parser_EmitError(self,
                         "'%.*s' cannot be used as a base class for '%.*s' (non class type).",
                         string_range_length(name_str),
                         name_str.str_bgn,
                         string_range_length(base_name_str),
                         base_name_str.str_bgn);
      }
    }
  }

  bfParser_eat(self, BIFROST_TOKEN_L_CURLY, false, "Class definition must start with a curly brace.");

  BifrostObjClass* const clz = bfObj_NewClass(self->vm, self->current_module, name_str, base_clz, 0u);

  bfVM_xSetVariable(&self->current_module->variables, self->vm, name_str, bfVMValue_fromPointer(clz));

  self->current_clz = clz;
  {
    while (!bfParser_is(self, BIFROST_TOKEN_R_CURLY))
    {
      if (bfParser_match(self, BIFROST_TOKEN_VAR))
      {
        parseClassVarDecl(self, clz, false);
      }
      else if (bfParser_match(self, BIFROST_TOKEN_FUNC))
      {
        parseClassFunc(self, clz, false);
      }
      else if (bfParser_match(self, BIFROST_TOKEN_STATIC))
      {
        if (bfParser_match(self, BIFROST_TOKEN_FUNC))
        {
          parseClassFunc(self, clz, true);
        }
        else if (bfParser_match(self, BIFROST_TOKEN_VAR))
        {
          parseClassVarDecl(self, clz, true);
        }
        else
        {
          Parser_EmitError(self, "'static' keyword must be followed by either a function or variable declaration.");
        }
      }
      else
      {
        Parser_EmitError(self, "Invalid declaration in class. Currently only 'var' and 'func' are supported.");
        Parser_parseStatement(self);
      }
    }
  }
  self->current_clz = NULL;

  bfParser_eat(self, BIFROST_TOKEN_R_CURLY, false, "Class definition must end with a curly brace.");
  bfParser_eat(self, BIFROST_TOKEN_SEMI_COLON, false, "Class definition must have a semi colon at the end.");
}

static bool Parser_parseStatement(BifrostParser* const self)
{
  switch (self->current_token.type)
  {
    default:
      Parser_EmitError(self, "Unhandled Token (%s)\n", bfDbg_TokenTypeToString(self->current_token.type));
      bfParser_match(self, self->current_token.type);
    case BIFROST_TOKEN_EOP:
    {
      return false;
    }
    case BIFROST_TOKEN_SEMI_COLON:
    {
      bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);
      break;
    }
    case BIFROST_TOKEN_CTRL_BREAK:
    {
      if (self->loop_stack)
      {
        bfFuncBuilder_addInstBreak(self->fn_builder);
      }
      else
      {
        Parser_EmitError(self, "break cannot be used outside of loop.");
      }

      bfParser_match(self, BIFROST_TOKEN_CTRL_BREAK);
      bfParser_eat(self, BIFROST_TOKEN_SEMI_COLON, false, "Nothing must follow a 'break' statement.");

      // NOTE(Shareef):
      //   Same ending note for [BIFROST_TOKEN_RETURN].
      //   Search "@UnreachableCode"
      return false;
    }
    case BIFROST_TOKEN_RETURN:
    {
      bfParser_match(self, BIFROST_TOKEN_RETURN);

      const uint16_t expr_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);

      if (!bfParser_is(self, BIFROST_TOKEN_SEMI_COLON))
      {
        ExprInfo ret_expr = exprMake(expr_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
        parseExpr(self, &ret_expr, PREC_NONE);
      }

      bfFuncBuilder_addInstABx(
       self->fn_builder,
       BIFROST_VM_OP_RETURN,
       0,
       expr_loc);

      bfFuncBuilder_popTemp(self->fn_builder, expr_loc);

      bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);

      while (!bfParser_is(self, BIFROST_TOKEN_R_CURLY))
      {
        bfParser_match(self, self->current_token.type);
      }

      // NOTE(Shareef):
      //   @UnreachableCode
      //   Since nothing can be executed after a return we just
      //   keep going until we hit a closing curly brace.
      //   This optimizes away unreachable code.
      return false;
    }
    case BIFROST_TOKEN_CLASS:
    {
      parseClassDecl(self);
      break;
    }
    case BIFROST_TOKEN_CTRL_IF:
    {
      bfParser_match(self, BIFROST_TOKEN_CTRL_IF);

      bfParser_eat(self, BIFROST_TOKEN_L_PAREN, false, "If statements must have l paren after if keyword.");

      const uint16_t expr_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);

      ExprInfo expr = exprMake(expr_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
      parseExpr(self, &expr, PREC_NONE);

      bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "If statements must have r paren after condition.");

      const uint32_t if_jump = parserMakeJump(self);

      bfFuncBuilder_popTemp(self->fn_builder, expr_loc);

      parseBlock(self);

      if (bfParser_match(self, BIFROST_TOKEN_CTRL_ELSE))
      {
        const uint32_t else_jump = parserMakeJump(self);

        // NOTE(Shareef):
        //   [expr_loc] can be used here since the actual
        //   use is where the jump is NOT here.
        parserPatchJump(self, if_jump, expr_loc, true);

        Parser_parseStatement(self);

        parserPatchJump(self, else_jump, BIFROST_VM_INVALID_SLOT, false);
      }
      else
      {
        parserPatchJump(self, if_jump, expr_loc, true);
      }
      break;
    }
    case BIFROST_TOKEN_CTRL_WHILE:
    {
      /* GRAMMAR(SR):
           while (<expr>) { <statement>... }
       */

      bfParser_match(self, BIFROST_TOKEN_CTRL_WHILE);

      const uint16_t expr_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);
      const uint32_t jmp_back = parserMakeJumpRev(self);

      bfParser_eat(self, BIFROST_TOKEN_L_PAREN, false, "while statements must be followed by a left parenthesis.");
      ExprInfo expr = exprMake(expr_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
      parseExpr(self, &expr, PREC_NONE);
      bfParser_eat(self, BIFROST_TOKEN_R_PAREN, false, "while statement conditions must end with a right parenthesis.");

      const uint32_t jmp_skip = parserMakeJump(self);

      LoopInfo loop;
      loopPush(self, &loop);
      Parser_parseStatement(self);
      parserPatchJumpRev(self, jmp_back, BIFROST_VM_INVALID_SLOT, false);
      parserPatchJump(self, jmp_skip, expr_loc, true);

      bfFuncBuilder_popTemp(self->fn_builder, expr_loc);
      loopPop(self);
      break;
    }
    case BIFROST_TOKEN_STATIC:
    case BIFROST_TOKEN_VAR:
    {
      const bool is_static = bfParser_match(self, BIFROST_TOKEN_STATIC);

      parseVarDecl(self, is_static);
      break;
    }
    case BIFROST_TOKEN_FUNC:
    {
      parseFunctionDecl(self);
      break;
    }
    case BIFROST_TOKEN_IMPORT:
    {
      parseImport(self);
      break;
    }
    case BIFROST_TOKEN_CTRL_FOR:
    {
      bfParser_match(self, BIFROST_TOKEN_CTRL_FOR);
      parseForStatement(self);
      break;
    }
    case BIFROST_TOKEN_IDENTIFIER:
    {
      const uint16_t working_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);
      ExprInfo       expr        = exprMake(working_loc, VariableInfo_temp(BIFROST_VM_INVALID_SLOT));
      parseExpr(self, &expr, PREC_NONE);
      bfParser_match(self, BIFROST_TOKEN_SEMI_COLON);
      bfFuncBuilder_popTemp(self->fn_builder, working_loc);
      break;
    }
    case BIFROST_TOKEN_L_CURLY:
    {
      parseBlock(self);
      break;
    }
    case BIFROST_TOKEN_NEW:
    case BIFROST_TOKEN_SUPER:
    {
      const uint16_t expr_loc = bfFuncBuilder_pushTemp(self->fn_builder, 1);
      ExprInfo       expr     = exprMake(expr_loc, VariableInfo_temp(expr_loc));
      parseExpr(self, &expr, PREC_NONE);
      bfFuncBuilder_popTemp(self->fn_builder, expr_loc);
      break;
    }
  }

  return true;
}

static inline GrammarRule typeToRule(const bfTokenType type)
{
  static const GrammarRule s_Rules[] =
   {
#define BIFROST_TOKEN(TokenName, ParsePrefix, ParseInfix, ParsePrecedence) {ParsePrefix, ParseInfix, ParsePrecedence},
    BIFROST_TOKEN_XTABLE
#undef BIFROST_TOKEN
   };

  LibC_assert(type < bfCArraySize(s_Rules), "Invalid token type.");
  return s_Rules[type];
}
