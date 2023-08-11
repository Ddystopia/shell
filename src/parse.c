#include "parse.h"
#include "helpers.h"
#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum _term_terminator_kind_t {
  P_TERM_NONE,
  P_TERM_PARSE_ERROR,
  P_TERM_PIPE,
  P_TERM_BACKGROUND,
  P_TERM_BLOCK
};

typedef struct parsed_term_t {
  char *tail; // borrowing from a buf
  enum _term_terminator_kind_t kind;
  /// INVARIANT: if kind is `Background` `&argv[0][0]` should be equal to the
  /// pointer from `malloc`.
  /// OWNERSHIP1: struct has ownership of argv array;
  /// OWNERSHIP2: if kind is
  ///    `Block` - then it and all piped terms are borrowing from buf.
  ///    `Background` - that struct has ownership.
  ///    `Pipe` - borrowing from original buf or background's cloned buf.
  char **argv;
  size_t argc;
} parsed_term_t;

inline bool is_terminator(char c);
parsed_term_t parse_term(char *buf, parsed_term_t *parsed_array,
                         size_t parsed_count);

/*

input = cmd { ';' cmd }
cmd = expr '&'?
expr = term { '|' term }
term = word term_tail?
term_tail = fact { fact }
fact = word | "[^"]"
word = [^\s|;&"]+

*/

command_t parse_cmd(char *buf) {
  command_t command = {
      .kind = None,
      .terms = NULL,
      .terms_count = 0,
      .tail = NULL,
  };

  size_t cap = 1;
  parsed_term_t *parsed_terms = malloc(sizeof(parsed_term_t) * cap);
  if (parsed_terms == NULL) {
    errx(EXIT_FAILURE, "Malloc error.");
  }
  size_t parsed_count = 0;

  while (buf != NULL) {
    const parsed_term_t term = parse_term(buf, parsed_terms, parsed_count);
    buf = term.tail;
    command.tail = buf;

    if (term.kind == P_TERM_NONE && parsed_count == 0) {
      buf = NULL;
      free(parsed_terms);
      command.kind = None;
      return command;
    }

    if (term.kind == P_TERM_PARSE_ERROR || term.kind == P_TERM_NONE) {
      for (size_t i = 0; i < parsed_count; i++) {
        assert(parsed_terms[i].kind == P_TERM_PIPE);
        free(parsed_terms[i].argv);
      }
      free(parsed_terms);

      command.kind = ParseError;
      return command;
    }

    parsed_count += 1;
    if (parsed_count > cap) {
      cap *= 2;
      parsed_terms = reallocarray(parsed_terms, cap, sizeof(parsed_term_t));
    }
    parsed_terms[parsed_count - 1] = term;

    if (term.kind == P_TERM_BLOCK || term.kind == P_TERM_BACKGROUND) {
      command.terms = (term_t *)malloc(sizeof(term_t) * parsed_count);
      if (command.terms == NULL) {
        errx(EXIT_FAILURE, "Malloc error.");
      }
      command.terms_count = parsed_count;
      for (size_t i = 0; i < parsed_count - 1; i++) {
        assert(parsed_terms[i].kind == P_TERM_PIPE);
        command.terms[i].argv = parsed_terms[i].argv;
        command.terms[i].argc = parsed_terms[i].argc;
        command.terms[i].kind = Pipe;
      }

      const int last = parsed_count - 1;
      command.terms[last].argv = term.argv;
      command.terms[last].argc = term.argc;
      if (term.kind == P_TERM_BACKGROUND) {
        command.terms[last].kind = Background;
      } else {
        command.terms[last].kind = Block;
      }

      free(parsed_terms);
      command.kind = Ok;
      return command;
    }
  }

  todo();
  command.kind = ParseError;
  return command;
}

bool is_terminator(char c) {
  char terminators[] = ";&|\0";
  for (size_t i = 0; i < sizeof(terminators) / sizeof(terminators[0]); i++) {
    if (terminators[i] == c) {
      return true;
    }
  }
  return false;
}

parsed_term_t parse_term(char *buffer, parsed_term_t *parsed_array,
                         size_t parsed_count) {
  parsed_term_t parsed = {
      .tail = NULL, .kind = P_TERM_NONE, .argv = NULL, .argc = 1};

  while (buffer[0] == ' ') {
    buffer += 1;
  }
  char *const buf = buffer;

  const size_t command_len = strcspn(buf, " ;|&\0");

  if (command_len == 0) {
    parsed.kind = P_TERM_NONE;
    return parsed;
  }

  const bool has_tail = !is_terminator(buf[command_len]);

  char *const command = buf;
  char *const command_tail = has_tail ? buf + command_len + 1 : NULL;

  size_t i = 0;

  // count argc
  while (has_tail && !is_terminator(command_tail[i])) {
    if (command_tail[i] != ' ' && command_tail[i - 1] == ' ') {
      parsed.argc += 1;
    }

    if (command_tail[i] == '"') {
      do {
        i += 1;
      } while (command_tail[i] != '"' && !is_terminator(command_tail[i]));
    }

    i += 1;
  }

  parsed.argv = (char **)malloc(sizeof(char *) * (parsed.argc + 1));
  if (parsed.argv == NULL) {
    errx(EXIT_FAILURE, "Malloc failed.\n");
  }

  size_t arg_idx = 1;
  parsed.argv[0] = command;
  i = 0;

  // fill in argv and separate args
  while (has_tail && !is_terminator(command_tail[i])) {
    if (command_tail[i] != ' ' && command_tail[i - 1] == ' ') {
      parsed.argv[arg_idx++] = &command_tail[i];
    }

    if (command_tail[i] == '"') {
      parsed.argv[arg_idx - 1] = &command_tail[i + 1];
      do {
        i += 1;
      } while (command_tail[i] != '"' && !is_terminator(command_tail[i]));
    } else if (command_tail[i] == ' ') {
      command_tail[i] = '\0';
    }

    i += 1;
  }

  char *terminator = has_tail ? &command_tail[i] : &command[command_len];
  parsed.argv[parsed.argc] = NULL;

  if (has_tail) {
    command[command_len] = '\0';
  }

  switch (*terminator) {
  case '&': {
    *terminator = '\0';

    do {
      terminator += 1;
    } while (*terminator == ' ');

    if (*terminator == '\0' || *terminator == ';') {
      parsed.kind = P_TERM_BACKGROUND;
      parsed.tail = *terminator == ';' ? terminator + 1 : NULL;
      // len already with & -> \0 (with null terminator)
      size_t len =
          terminator - (parsed_count == 0 ? buf : parsed_array[0].argv[0]);

      char *new_buf = malloc(sizeof(char) * len);
      if (new_buf == NULL) {
        errx(EXIT_FAILURE, "Malloc error.");
      }
      memcpy(new_buf, buf, len);

      ptrdiff_t shift = new_buf - parsed.argv[0];
      for (size_t j = 0; j < parsed.argc - 1; j++) {
        parsed.argv[j] += shift;
      }
      for (size_t i = 0; i < parsed_count; i++) {
        for (size_t j = 0; j < parsed_array[i].argc - 1; j++) {
          parsed_array[i].argv[j] += shift;
        }
      }

      return parsed;
    } else {
      parsed.kind = P_TERM_PARSE_ERROR;
      free(parsed.argv);
      return parsed;
    }
    break;
  }
  case '|': {
    parsed.kind = P_TERM_PIPE;
    parsed.tail = terminator + 1;
    *terminator = '\0';
    return parsed;
  }
  case ';': {
    parsed.kind = P_TERM_BLOCK;
    parsed.tail = terminator + 1;
    *terminator = '\0';
    return parsed;
  }
  case '\0': {
    parsed.kind = P_TERM_BLOCK;
    parsed.tail = NULL;
    return parsed;
  }
  }

  errx(EXIT_FAILURE, "Unreachable\n");
}
