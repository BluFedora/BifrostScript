/******************************************************************************/
/*!
 * @file   bifrost_vm_lexer.h
 * @author Shareef Raheem (https://blufedora.github.io/)
 * @brief
 *   Tokenizing helpers for strings.
 *
 * @copyright Copyright (c) 2020-2025 Shareef Abdoul-Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_LEXER_H
#define BIFROST_VM_LEXER_H

#include "bifrost_libc.h"

#if __cplusplus
extern "C" {
#endif

struct BifrostVM;

#define bfCArraySize(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct string_range
{
  const char* str_bgn;
  size_t      str_len;

} string_range;

inline string_range MakeStringLen(const char* const str, const size_t length)
{
  string_range result;
  result.str_bgn = str;
  result.str_len = length;

  return result;
}

inline string_range MakeString(const char* const str)
{
  string_range result;
  result.str_bgn = str;
  result.str_len = 0u;

  while (str[result.str_len] != '\0')
  {
    ++result.str_len;
  }

  return result;
}

/*!
 * @brief
 *   These are all of the token types that are used by the lexer.
 *
 *   // TODO(SR): Tokens: '/=', '*=', '%', '%=', '|', '&', '~', '>>', '<<'
 */
#define BIFROST_TOKEN_XTABLE                                                                                                     \
  BIFROST_TOKEN(BIFROST_TOKEN_L_PAREN, Expr_parseGroup, Expr_parseCall, PREC_CALL) /*!< (                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_R_PAREN, NULL, NULL, PREC_NONE)                      /*!< )                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_L_SQR_BOI, NULL, Expr_parseSubscript, PREC_CALL)     /*!< [                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_R_SQR_BOI, NULL, NULL, PREC_NONE)                    /*!< ]                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_L_CURLY, NULL, NULL, PREC_NONE)                      /*!< {                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_R_CURLY, NULL, NULL, PREC_NONE)                      /*!< }                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_HASHTAG, NULL, NULL, PREC_NONE)                      /*!< #                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_COLON, NULL, Expr_parseMethodCall, PREC_CALL)        /*!< :                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_SEMI_COLON, NULL, NULL, PREC_NONE)                   /*!< ;                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_COMMA, NULL, NULL, PREC_NONE)                        /*!< ,                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_EQUALS, NULL, Expr_parseAssign, PREC_ASSIGN)         /*!< =                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_PLUS, NULL, Expr_parseBinOp, PREC_TERM)              /*!< +                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_MINUS, NULL, Expr_parseBinOp, PREC_TERM)             /*!< -                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_MULT, NULL, Expr_parseBinOp, PREC_FACTOR)            /*!< *                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_DIV, NULL, Expr_parseBinOp, PREC_FACTOR)             /*!< /                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_PLUS_EQUALS, NULL, Expr_parseAssign, PREC_ASSIGN)    /*!< +=                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_MINUS_EQUALS, NULL, Expr_parseAssign, PREC_ASSIGN)   /*!< -=                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_INC, NULL, NULL, PREC_NONE)                          /*!< ++                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_DEC, NULL, NULL, PREC_NONE)                          /*!< --                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_DOT, NULL, Expr_parseDotOp, PREC_CALL)               /*!< .                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_IDENTIFIER, Expr_parseVariable, NULL, PREC_NONE)     /*!< abcdefghijklmnopqrstuvwxyz_0123456789 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_VAR, NULL, NULL, PREC_NONE)                          /*!< var                                   */ \
  BIFROST_TOKEN(BIFROST_TOKEN_IMPORT, NULL, NULL, PREC_NONE)                       /*!< import                                */ \
  BIFROST_TOKEN(BIFROST_TOKEN_FUNC, Expr_parseFunctionExpr, NULL, PREC_NONE)       /*!< func                                  */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CLASS, NULL, NULL, PREC_NONE)                        /*!< class                                 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_IF, NULL, NULL, PREC_NONE)                      /*!< if                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_ELSE, NULL, NULL, PREC_NONE)                    /*!< else                                  */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_EE, NULL, Expr_parseBinOp, PREC_EQUALITY)       /*!< ==                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_LT, NULL, Expr_parseBinOp, PREC_COMPARISON)     /*!< <                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_GT, NULL, Expr_parseBinOp, PREC_COMPARISON)     /*!< >                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_LE, NULL, Expr_parseBinOp, PREC_COMPARISON)     /*!< <=                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_GE, NULL, Expr_parseBinOp, PREC_COMPARISON)     /*!< >=                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_OR, NULL, Expr_parseBinOp, PREC_OR)             /*!< ||                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_AND, NULL, Expr_parseBinOp, PREC_AND)           /*!< &&                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_NE, NULL, Expr_parseBinOp, PREC_EQUALITY)       /*!< !=                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_WHILE, NULL, NULL, PREC_NONE)                   /*!< while                                 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_FOR, NULL, NULL, PREC_NONE)                     /*!< for                                   */ \
  BIFROST_TOKEN(BIFROST_TOKEN_RETURN, NULL, NULL, PREC_NONE)                       /*!< return                                */ \
  BIFROST_TOKEN(BIFROST_TOKEN_BANG, NULL, NULL, PREC_NONE)                         /*!< !                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CONST_STR, Expr_parseLiteral, NULL, PREC_NONE)       /*!< "..."                                 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CONST_REAL, Expr_parseLiteral, NULL, PREC_NONE)      /*!< 01234567890.0123456789                */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CONST_BOOL, Expr_parseLiteral, NULL, PREC_NONE)      /*!< true, false                           */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CONST_NIL, Expr_parseLiteral, NULL, PREC_NONE)       /*!< nil                                   */ \
  BIFROST_TOKEN(BIFROST_TOKEN_CTRL_BREAK, NULL, NULL, PREC_NONE)                   /*!< break                                 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_NEW, Expr_parseNew, NULL, PREC_NONE)                 /*!< new                                   */ \
  BIFROST_TOKEN(BIFROST_TOKEN_STATIC, NULL, NULL, PREC_NONE)                       /*!< static                                */ \
  BIFROST_TOKEN(BIFROST_TOKEN_AS, NULL, NULL, PREC_NONE)                           /*!< as                                    */ \
  BIFROST_TOKEN(BIFROST_TOKEN_SUPER, Expr_parseSuper, NULL, PREC_NONE)             /*!< super                                 */ \
  BIFROST_TOKEN(BIFROST_TOKEN_AT_SIGN, NULL, NULL, PREC_NONE)                      /*!< @                                     */ \
  BIFROST_TOKEN(BIFROST_TOKEN_EOP, NULL, NULL, PREC_NONE)                          /*!< End of Program                        */

typedef enum bfTokenType
{
#define BIFROST_TOKEN(TokenName, ParsePrefix, ParseInfix, ParsePrecedence) TokenName,
  BIFROST_TOKEN_XTABLE
#undef BIFROST_TOKEN
   BIFROST_TOKEN_COUNT,

} bfTokenType;

/*!
 * @brief
 *   An individual token for a program.
 */
typedef struct bfToken
{
  bfTokenType  type;
  string_range str_range;
  double       num;

} bfToken;

typedef struct BifrostLexerParams
{
  const char*       source;
  size_t            length;
  struct BifrostVM* vm;

} BifrostLexerParams;

typedef struct BifrostLexer
{
  const char*       source_bgn;
  const char*       source_end;
  size_t            cursor;
  size_t            current_line_no;
  size_t            line_pos_bgn;
  size_t            line_pos_end;
  struct BifrostVM* vm;

} BifrostLexer;

BifrostLexer bfLexer_make(const BifrostLexerParams* params);
bfToken      bfLexer_nextToken(BifrostLexer* self);

#define BIFROST_TOKEN_MAKE_STR_RANGE(t, s, e) \
  (bfToken)                                   \
  {                                           \
    .type      = t,                           \
    .str_range = {s, e},                      \
    .num       = 0.0,                         \
  }

#define BIFROST_TOKEN_MAKE_ARRAY_INIT(t, s) \
  {                                         \
   .type      = t,                          \
   .str_range = {s, sizeof(s) - 1},         \
   .num       = 0.0,                        \
}

#define BIFROST_TOKEN_MAKE_STR(t, s) \
  (bfToken) BIFROST_TOKEN_MAKE_ARRAY_INIT(t, s)

#define BIFROST_TOKEN_MAKE_NUM(v) \
  (bfToken) { .type = BIFROST_TOKEN_CONST_REAL, .num = v }

#if __cplusplus
}
#endif

#endif /* BIFROST_VM_LEXER_H */
