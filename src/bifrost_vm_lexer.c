/******************************************************************************/
/*!
 * @file   bifrost_vm_lexer.c
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @brief
 *   Tokenizing helpers for strings.
 *
 * @copyright Copyright (c) 2020
 */
/******************************************************************************/
#include "bifrost_vm_lexer.h"

#include "bifrost/bifrost_vm.h"  // bfVM_SetLastError

static const char BTS_COMMENT_CHARACTER = '/';

static const char* bfLexer_peekStr(const BifrostLexer* self, size_t amt)
{
  const char* target_str = self->source_bgn + self->cursor + amt;

  if (target_str < self->source_end)
  {
    return target_str;
  }

  return self->source_end;
}

static char bfLexer_peek(const BifrostLexer* self, size_t amt)
{
  return *bfLexer_peekStr(self, amt);
}

static string_range bfLexer_currentLine(BifrostLexer* self)
{
  return (string_range){
   .str_bgn = self->source_bgn + self->line_pos_bgn,
   .str_len = self->line_pos_end,
  };
}

static bool bfLexer_isNewline(char c)
{
  return c == '\n' || c == '\r' || c == '\0';
}

static void bfLexer_advance(BifrostLexer* self, size_t amt)
{
  self->cursor += amt;

  /* NOTE(Shareef):
      On windows a newline is '\r\n' rather than just the
      Unix '\n' so we need to skip another character
      to make sure the line count is correct.
  */

  const char curr = bfLexer_peek(self, 0);

  if (bfLexer_isNewline(curr) || amt == 0)
  {
    const size_t source_length = self->source_end - self->source_bgn;

    ++self->current_line_no;
    self->line_pos_bgn = self->cursor + (curr == '\n');
    self->line_pos_end = self->line_pos_bgn + (curr == '\n');

    while (!bfLexer_isNewline(self->source_bgn[self->line_pos_end]) && self->line_pos_end < source_length)
    {
      ++self->line_pos_end;
    }

    self->line_pos_end = self->line_pos_end < source_length ? self->line_pos_end + 1 : source_length;
  }
}

static void bfLexer_reset(BifrostLexer* self)
{
  self->cursor          = 0;
  self->current_line_no = 1;
  self->line_pos_bgn    = 0;
  self->line_pos_end    = 0;
  bfLexer_advance(self, 0);
}

static void bfLexer_skipWhile(BifrostLexer* self, bool (*condition)(char c))
{
  while (condition(bfLexer_peek(self, 0)) && self->source_bgn + self->cursor < self->source_end)
  {
    bfLexer_advance(self, 1);
  }
}

static void bfLexer_skipWhitespace(BifrostLexer* self)
{
  bfLexer_skipWhile(self, &LibC_isspace);
}

static bool bfLexer_isNotNewline(char c)
{
  return !bfLexer_isNewline(c);
}

static void bfLexer_skipLineComment(BifrostLexer* self)
{
  bfLexer_advance(self, 2); /* // */
  bfLexer_skipWhile(self, &bfLexer_isNotNewline);
}

static void bfLexer_skipBlockComment(BifrostLexer* self)
{
  const size_t line_no = self->current_line_no;
  bfLexer_advance(self, 2); /* / * */

  while (bfLexer_peek(self, 0) != '*' || bfLexer_peek(self, 1) != '/')
  {
    if (bfLexer_peek(self, 0) == '\0')
    {
      bfVM_SetLastError(BIFROST_VM_ERROR_LEXER,
                        self->vm,
                        (int)self->current_line_no,
                        "Unfinished block comment starting on line(%zu)",
                        line_no);
      break;
    }

    bfLexer_advance(self, 1);
  }

  bfLexer_advance(self, 2); /* * / */
}

static void bfLexer_advanceLine(BifrostLexer* self)
{
  bfLexer_skipWhile(self, &bfLexer_isNotNewline);
}

static bool bfLexer_isFollowedByDigit(BifrostLexer* self, char c, char m)
{
  return c == m && LibC_isdigit(bfLexer_peek(self, 1));
}

static bfToken bfLexer_parseNumber(BifrostLexer* self)
{
  const char*  bgn   = bfLexer_peekStr(self, 0);
  char*        end   = NULL;
  const double value = LibC_strtod(bgn, &end);
  bfLexer_advance(self, end - bgn);

  const char current = bfLexer_peek(self, 0);

  if (current == 'f' || current == 'F')
  {
    bfLexer_advance(self, 1);
  }

  return BIFROST_TOKEN_MAKE_NUM(value);
}

static bool bfLexer_isID(char c)
{
  return LibC_isalpha(c) || c == '_' || LibC_isdigit(c);
}

static bfToken bfLexer_parseID(BifrostLexer* self)
{
  static const bfToken s_Keywords[] =
   {
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CONST_BOOL, "true"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CONST_BOOL, "false"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_RETURN, "return"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CTRL_IF, "if"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CTRL_ELSE, "else"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CTRL_FOR, "for"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CTRL_WHILE, "while"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_FUNC, "func"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_VAR, "var"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CONST_NIL, "nil"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CLASS, "class"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_IMPORT, "import"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_CTRL_BREAK, "break"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_NEW, "new"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_STATIC, "static"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_AS, "as"),
    BIFROST_TOKEN_MAKE_ARRAY_INIT(BIFROST_TOKEN_SUPER, "super"),
   };

  const char* bgn = bfLexer_peekStr(self, 0);
  bfLexer_skipWhile(self, &bfLexer_isID);
  const char*  end          = bfLexer_peekStr(self, 0);
  const size_t length       = end - bgn;
  const size_t num_keywords = bfCArraySize(s_Keywords);

  for (size_t i = 0; i < num_keywords; ++i)
  {
    const bfToken* keyword        = s_Keywords + i;
    const size_t   keyword_length = keyword->str_range.str_len;

    if (keyword_length == length && LibC_strncmp(keyword->str_range.str_bgn, bgn, length) == 0)
    {
      return *keyword;
    }
  }

  return BIFROST_TOKEN_MAKE_STR_RANGE(BIFROST_TOKEN_IDENTIFIER, bgn, length);
}

static bool bfLexer_isNotQuote(char c)
{
  return c != '\"';
}

// Not handling the escape sequences in the lexer anymore.
// it is now the parsers' job. (this simplifies the case where
// if I wanted to add some special language specific sequences EX: Variable based).
// This also makes it so I can make this Lexer non allocating which is awesome.
static bfToken bfLexer_parseString(BifrostLexer* self)
{
  bfLexer_advance(self, 1);  // '"'

  const char* bgn = bfLexer_peekStr(self, 0);

  while (bfLexer_isNotQuote(bfLexer_peek(self, 0)) && self->source_bgn + self->cursor < self->source_end)
  {
    bfLexer_advance(self, 1);

    if (bfLexer_peek(self, 0) == '\\' && bfLexer_peek(self, 1) == '\"')
    {
      bfLexer_advance(self, 2);
    }
  }

  const char* end = bfLexer_peekStr(self, 0);

  bfLexer_advance(self, 1);  // '"'

  return BIFROST_TOKEN_MAKE_STR_RANGE(BIFROST_TOKEN_CONST_STR, bgn, end - bgn);
}

BifrostLexer bfLexer_make(const BifrostLexerParams* params)
{
  BifrostLexer self;
  self.source_bgn = params->source;
  self.source_end = params->source + params->length;
  self.vm         = params->vm;
  bfLexer_reset(&self);
  return self;
}

bfToken bfLexer_nextToken(BifrostLexer* self)
{
  char current_char = bfLexer_peek(self, 0);

  while (current_char != '\0')
  {
    current_char = bfLexer_peek(self, 0);

    if (LibC_isspace(current_char))
    {
      bfLexer_skipWhitespace(self);
      continue;
    }

    if (current_char == BTS_COMMENT_CHARACTER)
    {
      const char next_char = bfLexer_peek(self, 1);

      if (next_char == BTS_COMMENT_CHARACTER)
      {
        bfLexer_skipLineComment(self);
      }
      else if (next_char == '*')
      {
        bfLexer_skipBlockComment(self);
      }
      else
      {
        bfLexer_advance(self, 1);
        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_DIV, "/");
      }
      continue;
    }

    if (LibC_isdigit(current_char) || bfLexer_isFollowedByDigit(self, current_char, '.'))
    {
      return bfLexer_parseNumber(self);
    }

    if (bfLexer_isID(current_char))
    {
      return bfLexer_parseID(self);
    }

    if (current_char == '"')
    {
      return bfLexer_parseString(self);
    }

    bfLexer_advance(self, 1);
    const char next_char = bfLexer_peek(self, 0);

    switch (current_char)
    {
      case '[': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_L_SQR_BOI, "[");
      case ']': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_R_SQR_BOI, "]");
      case '(': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_L_PAREN, "(");
      case ')': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_R_PAREN, ")");
      case ':': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_COLON, ":");
      case ';': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_SEMI_COLON, ";");
      case '{': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_L_CURLY, "{");
      case '}': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_R_CURLY, "}");
      case ',': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_COMMA, ",");
      case '.': return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_DOT, ".");

      case '<':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_LE, "<=");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_LT, "<");
      }
      case '>':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_GE, ">=");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_GT, ">");
      }
      case '=':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_EE, "==");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_EQUALS, "=");
      }
      case '+':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_PLUS_EQUALS, "+=");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_PLUS, "+");
      }
      case '-':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_MINUS_EQUALS, "-=");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_MINUS, "-");
      }
      case '*':
      {
        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_MULT, "*");
      }
      case '/':
      {
        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_DIV, "/");
      }
      case '!':
      {
        if (next_char == '=')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_NE, "!=");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_BANG, "!");
      }
      case '|':
      {
        if (next_char == '|')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_OR, "||");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_OR, "|");
      }
      case '&':
      {
        if (next_char == '&')
        {
          bfLexer_advance(self, 1);
          return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_AND, "&&");
        }

        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_CTRL_AND, "&");
      }
      case '#':
      {
        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_HASHTAG, "#");
      }
      case '@':
      {
        return BIFROST_TOKEN_MAKE_STR(BIFROST_TOKEN_AT_SIGN, "@");
      }
      case '\0':
      {
        break;
      }
      default:
      {
        const string_range line = bfLexer_currentLine(self);

        bfVM_SetLastError(BIFROST_VM_ERROR_LEXER,
                          self->vm,
                          (int)self->current_line_no,
                          "Invalid character ('%c') on line %u \"%.*s\"",
                          current_char,
                          (unsigned int)self->current_line_no,
                          (int)line.str_len,
                          line.str_bgn);

        bfLexer_advance(self, 1);
        break;
      }
    }
  }

  return (bfToken){.type = BIFROST_TOKEN_EOP, .str_range = MakeString("BIFROST_TOKEN_EOP")};
}