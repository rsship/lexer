#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define return_defer(value) do { result = (value); goto defer; } while(0)
#define ARRAY_LEN(arr) sizeof((arr)) / sizeof((arr[0]))

typedef struct {
  char *items;
  size_t count;
  size_t capacity;
} String_Builder;

bool read_entire_file(const char *file_path, String_Builder *sb)
{

  bool result = true; 
  FILE *f = fopen(file_path, "rb");
  if (f == NULL) return_defer(false);
  if (fseek(f, 0, SEEK_END) < 0)  return_defer(false);
  long m = ftell(f);
  if (m < 0)  return_defer(false);
  if (fseek(f, 0, SEEK_SET) < 0)  return_defer(false);

  size_t new_count = sb->count + m;
  if (new_count > sb->capacity) {
    sb->items = realloc(sb->items, new_count);
    assert(sb->items != NULL && "Not enough memory");
    sb->capacity = new_count;
  }

  fread(sb->items + sb->count, m, 1, f);
  if (ferror(f)) {
     return_defer(false);
  }

  sb->count = new_count;


defer:
  if (!result) printf("could not read file %s: %s", file_path, strerror(errno));
  if (f) fclose(f);
  return result;
}

void print_string_builder(String_Builder *sb)
{
    printf("%s", sb->items);
}

typedef enum {
  LEXER_INVALID,
  LEXER_END,
  LEXER_INT,
  LEXER_STRING,
  LEXER_KEYWORD,
  LEXER_SYMBOL,
  LEXER_PUNCT,
  LEXER_COUNT_KINDS
} Lexer_Kind;

typedef struct {
  const char *file_path;
  const char *content;
  size_t size;

  size_t bol;
  size_t row;
  size_t cur;

  const char **puncts;
  size_t puncts_count;
  const char **keyboards;
  size_t keyboard_counts;
} Lexer;

typedef struct {
  const char *file_path;
  size_t row;
  size_t col;
} Lexer_Loc;

Lexer_Loc lexer_loc(Lexer *l)
{
  return (Lexer_Loc) {
    .col = l->cur - l->bol + 1,
    .row = l->row + 1,
    .file_path = l->file_path,
  };
}

typedef struct {
  const char *begin;
  const char *end;
  Lexer_Loc loc;
  Lexer_Kind kind;
  size_t punct_index;
  size_t kw_index;
} Lexer_Token;

bool lexer_chop_char(Lexer *l)
{
  if (l->cur < l->size) {
    char x = l->content[l->cur];
    l->cur += 1;
    if (x == '\n') {
      l->row += 1;
      l->bol = l->cur;
      return true;
    }
  }
  return false;
}

void lexer_chop_chars(Lexer *l, size_t n)
{
  while (n --> 0 && lexer_chop_char(l));
}

void lexer_trim_left_ws(Lexer *l)
{
  while (l->cur < l->size && isspace(l->content[l->cur])) {
    lexer_chop_char(l);
  }
}

bool lexer_starts_with(Lexer *l, const char *prefix, size_t len)
{
  for (size_t i = 0; i + l->cur < l->size && i < len; ++i) {
    if (l->content[l->cur+ i] != prefix[i]) {
      return false;
    }
  }
  return true;
}

bool lexer_get_token(Lexer *l, Lexer_Token *t)
{
  lexer_trim_left_ws(l);

  memset(t, 0, sizeof(*t));
  t->loc = lexer_loc(l);
  t->begin = &l->content[l->cur];
  
  t->end = &l->content[l->cur];

  if (l->cur >= l->size) {
    t->kind = LEXER_END;
    return false;
  }

  // Puncts
  for (size_t i = 0; i < l->puncts_count; ++i) {
    size_t n = strlen(l->puncts[i]);
    if (lexer_starts_with(l, l->puncts[i], n)) {
      t->kind = LEXER_PUNCT;
      t->punct_index = i;
      t->end += n;
      lexer_chop_chars(l, n);
      return true;
    }
  }

  lexer_chop_char(l);
  t->end +=1;
  return false;
}

Lexer lexer_create(const char *file_path, char *items, size_t n)
{
  return (Lexer) {
    .file_path = file_path,
    .content   = items,
    .size      = n
  };
}

static_assert(LEXER_COUNT_KINDS == 7, "Amount of kinds have changed");
const char *lexer_kind_names[LEXER_COUNT_KINDS] = {
  [LEXER_INVALID] = "INVALID",
  [LEXER_END]     = "END",
  [LEXER_INT]     = "INT",
  [LEXER_SYMBOL]  = "SYMBOL",
  [LEXER_KEYWORD] = "KEYWORD",
  [LEXER_PUNCT]   = "PUNCT",
  [LEXER_STRING]  = "STRING",
};

void print_token(Lexer_Token *t)
{
  printf("token started %s and ended %s with %s\n", t->begin, t->end, lexer_kind_names[t->kind]);
}

typedef enum {
  PUNCT_BAR,       
  PUNCT_COMMA,     
  PUNCT_OPAREN,    
  PUNCT_CPAREN,    
  PUNCT_SEMICOLON, 
} Puncts;

static const char *puncts[] = {
  [PUNCT_BAR]       = "|",
  [PUNCT_COMMA]     = ",",
  [PUNCT_OPAREN]    = "(",
  [PUNCT_CPAREN]    = ")",
  [PUNCT_SEMICOLON] = ";",
};

int main()
{
  String_Builder sb = {0};
  const char *file_path = "rules.lex";
  if (!read_entire_file(file_path,  &sb)) {
    return 1;
  }

  Lexer l       = lexer_create(file_path, sb.items, sb.count);
  Lexer_Token t = {0};
  l.puncts = puncts;
  l.puncts_count = ARRAY_LEN(puncts);

  lexer_get_token(&l, &t);
  printf("kind -> %s\n", lexer_kind_names[t.kind]);
}
