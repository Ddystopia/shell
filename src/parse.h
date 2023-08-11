#include <stddef.h>

enum term_terminator_kind_t { Pipe, Background, Block };

enum command_result_kind_t {
  Ok,
  None,
  ParseError
};

typedef struct term_t {
  enum term_terminator_kind_t kind;
  char **argv;
  size_t argc;
} term_t;

typedef struct command_t {
  enum command_result_kind_t kind;
  term_t *terms;
  size_t terms_count;
  char *tail;
} command_t;

command_t parse_cmd(char *buf);
