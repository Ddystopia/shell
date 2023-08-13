#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.h"
#include "lib.h"
#include <stdio.h>

enum _term_execution_mode_t {
  P_TERM_NONE,
  P_TERM_PARSE_ERROR,
  P_TERM_PIPE,
  P_TERM_BACKGROUND,
  P_TERM_BLOCK
};

typedef struct parsed_term_t {
  char *tail; // borrowing from a buf
  enum _term_execution_mode_t kind;
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

input = pipeline { ';' pipeline } ;
pipeline = raw_pipeline , [ '&' ] ;
raw_pipeline = term { '|' term } ;
term = word , [ args ] ;
args = fact , { fact } ;
fact = word | '"' , { char - '"' } , '"' ;
word = (char - not_in_word) + ;
not_in_word = ' ' | '|' | ';' | '&' | '"' ;
char = ? aschii char ? ;

*/

command_pipeline_t parse_pipeline(char *buf) {
  command_pipeline_t pipeline = {
      .result_kind = None,
      .execution_mode = Block,
      .terms = NULL,
      .terms_count = 0,
  };

  size_t cap = 0;
  size_t parsed_count = 0;
  parsed_term_t *parsed_terms = NULL;

  while (buf != NULL) {
    const parsed_term_t term = parse_term(buf, parsed_terms, parsed_count);
    buf = term.tail;
    pipeline.tail = buf;

    if (term.kind == P_TERM_NONE) {
      if (buf == NULL && parsed_count == 0) {
        pipeline.result_kind = None;
        return pipeline;
      }
      continue;
    }

    if (term.kind == P_TERM_PARSE_ERROR) {
      break;
    }

    parsed_count += 1;
    if (parsed_count > cap) {
      cap = cap == 0 ? 1 : cap * 2;
      parsed_terms = reallocarray(parsed_terms, cap, sizeof(parsed_term_t));
      if (parsed_terms == NULL) {
        errx(EXIT_FAILURE, "`reallocarray` error.");
      }
    }
    parsed_terms[parsed_count - 1] = term;

    if (term.kind == P_TERM_BLOCK || term.kind == P_TERM_BACKGROUND) {
      pipeline.terms = (term_t *)malloc(sizeof(term_t) * parsed_count);
      if (pipeline.terms == NULL) {
        errx(EXIT_FAILURE, "Malloc error.");
      }
      pipeline.terms_count = parsed_count;
      for (size_t i = 0; i < parsed_count - 1; i++) {
        assert(parsed_terms[i].kind == P_TERM_PIPE);
        pipeline.terms[i].argv = parsed_terms[i].argv;
        pipeline.terms[i].argc = parsed_terms[i].argc;
      }

      const int last = parsed_count - 1;
      pipeline.terms[last].argv = term.argv;
      pipeline.terms[last].argc = term.argc;
      if (term.kind == P_TERM_BACKGROUND) {
        pipeline.execution_mode = Background;
      } else {
        pipeline.execution_mode = Block;
      }

      free(parsed_terms);
      pipeline.result_kind = Ok;
      return pipeline;
    }
  }

  for (size_t i = 0; i < parsed_count; i++) {
    assert(parsed_terms[i].kind == P_TERM_PIPE);
    free(parsed_terms[i].argv);
  }
  free(parsed_terms);

  pipeline.result_kind = ParseError;
  return pipeline;
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

  if (buf[0] == ';') {
    parsed.tail = buf + 1;
    return parsed;
  }

  if (buf[0] == '\0') {
    return parsed;
  }

  const size_t command_len = strcspn(buf, " ;|&\0");

  if (command_len == 0) {
    parsed.kind = P_TERM_PARSE_ERROR;
    return parsed;
  }

  const bool has_tail = !is_terminator(buf[command_len]);

  char *const command = buf;
  char *const command_pipeline_tail = has_tail ? buf + command_len + 1 : NULL;

  size_t i = 0;

  // count argc
  while (has_tail && !is_terminator(command_pipeline_tail[i])) {
    if (command_pipeline_tail[i] != ' ' && command_pipeline_tail[i - 1] == ' ') {
      parsed.argc += 1;
    }

    if (command_pipeline_tail[i] == '"') {
      do {
        i += 1;
      } while (command_pipeline_tail[i] != '"' && !is_terminator(command_pipeline_tail[i]));
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
  while (has_tail && !is_terminator(command_pipeline_tail[i])) {
    if (command_pipeline_tail[i] != ' ' && command_pipeline_tail[i - 1] == ' ') {
      parsed.argv[arg_idx++] = &command_pipeline_tail[i];
      command_pipeline_tail[i - 1] = '\0';
    }

    if (command_pipeline_tail[i] == '"') {
      parsed.argv[arg_idx - 1] = &command_pipeline_tail[i + 1];
      do {
        i += 1;
      } while (command_pipeline_tail[i] != '"' && !is_terminator(command_pipeline_tail[i]));
    } else if (command_pipeline_tail[i] == ' ' && command_pipeline_tail[i + 1] == ' ') {
      command_pipeline_tail[i] = '\0';
    }

    i += 1;
  }

  if (has_tail && command_pipeline_tail[i - 1] == ' ') {
    command_pipeline_tail[i - 1] = '\0';
  }

  char *terminator = has_tail ? &command_pipeline_tail[i] : &command[command_len];
  parsed.argv[parsed.argc] = NULL;

  switch (*terminator) {
  case '&': {
    *terminator = '\0';

    do {
      terminator += 1;
    } while (*terminator == ' ');

    if (*terminator == '\0' || *terminator == ';') {
      parsed.kind = P_TERM_BACKGROUND;
      if (terminator[0] != ';' || terminator[1] == '\0') {
        parsed.tail = NULL;
      } else {
        parsed.tail = terminator + 1;
      }

      char *shared_buf = parsed_count == 0 ? buf : parsed_array[0].argv[0];

      const size_t len = terminator - shared_buf;

      char *const new_buf = malloc(sizeof(char) * len);
      if (new_buf == NULL) {
        errx(EXIT_FAILURE, "Malloc error.");
      }
      memcpy(new_buf, shared_buf, len);

      ptrdiff_t shift = new_buf - shared_buf;
      for (size_t j = 0; j < parsed.argc; j++) {
        parsed.argv[j] += shift;
      }
      for (size_t i = 0; i < parsed_count; i++) {
        for (size_t j = 0; j < parsed_array[i].argc; j++) {
          parsed_array[i].argv[j] += shift;
        }
      }
      assert(parsed_count == 0 ? true : parsed_array[0].argv[0] == new_buf);

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
    if (parsed.tail[0] == '\0') {
      parsed.tail = NULL;
    }
    return parsed;
  }
  case ';': {
    parsed.kind = P_TERM_BLOCK;
    parsed.tail = terminator + 1;
    *terminator = '\0';
    if (parsed.tail[0] == '\0') {
      parsed.tail = NULL;
    }
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

#ifdef TEST
#include <stdio.h>

void test_parse_term__empty() {
  char buf[] = "";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_NONE);
}

void test_parse_term__only_semicolon() {
  char buf[] = ";";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_NONE);
}

void test_parse_term__only_pipe() {
  char buf[] = "|";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_PARSE_ERROR);
}

void test_parse_term__only_amp() {
  char buf[] = "&";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_PARSE_ERROR);
}

void test_parse_term__one_char_name() {
  char buf[] = "e";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_BLOCK);
  assert(pterm.tail == NULL);
  assert(pterm.argc == 1);
  assert(strcmp(pterm.argv[0], "e") == 0);
  assert(pterm.argv[1] == NULL);
  free(pterm.argv);
}

void test_parse_term__no_arg() {
  char buf[] = "echo";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_BLOCK);
  assert(pterm.tail == NULL);
  assert(pterm.argc == 1);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(pterm.argv[1] == NULL);
  free(pterm.argv);
}

void test_parse_term__one_char_arg() {
  char buf[] = "echo a";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.tail == NULL);
  assert(pterm.kind == P_TERM_BLOCK);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);
  free(pterm.argv);
}

void test_parse_term__one_arg_semi() {
  char buf[] = "echo a;";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.tail == NULL);
  assert(pterm.kind == P_TERM_BLOCK);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);
  free(pterm.argv);
}

void test_parse_term__pipe__error() {
  char buf[] = "echo a|";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.tail == NULL);
  assert(pterm.kind == P_TERM_PIPE);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);
  free(pterm.argv);
}

void test_parse_term__space_amp() {
  char buf[] = " echo&";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.tail == NULL);
  assert(pterm.kind == P_TERM_BACKGROUND);
  assert(pterm.argc == 1);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(pterm.argv[1] == NULL);

  free(pterm.argv[0]);
  free(pterm.argv);
}


void test_parse_term__bad_terminated_amp() {
  char buf[] = "ls&ls&ls";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_PARSE_ERROR);
}

void test_parse_term__basic_pipe() {
  char buf[] = "echo a | echo b";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(strcmp(pterm.tail, " echo b") == 0);
  assert(pterm.kind == P_TERM_PIPE);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);

  parsed_term_t pterm2 = parse_term(pterm.tail, &pterm, 1);
  assert(pterm2.tail == NULL);
  assert(pterm2.kind == P_TERM_BLOCK);
  assert(pterm2.argc == 2);
  assert(strcmp(pterm2.argv[0], "echo") == 0);
  assert(strcmp(pterm2.argv[1], "b") == 0);
  assert(pterm2.argv[2] == NULL);

  free(pterm.argv);
  free(pterm2.argv);
}

void test_parse_term__basic_pipe_with_amp_nospace() {
  char buf[] = "echo a | echo b&";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(strcmp(pterm.tail, " echo b&") == 0);
  assert(pterm.kind == P_TERM_PIPE);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);

  parsed_term_t pterm2 = parse_term(pterm.tail, &pterm, 1);
  assert(pterm2.tail == NULL);
  assert(pterm2.kind == P_TERM_BACKGROUND);
  assert(pterm2.argc == 2);
  assert(strcmp(pterm2.argv[0], "echo") == 0);
  assert(strcmp(pterm2.argv[1], "b") == 0);
  assert(pterm2.argv[2] == NULL);

  free(pterm.argv[0]);
  free(pterm.argv);
  free(pterm2.argv);
}

void test_parse_term__basic_pipe_with_amp_space() {
  char buf[] = "echo a | ls b &";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(strcmp(pterm.tail, " ls b &") == 0);
  assert(pterm.kind == P_TERM_PIPE);
  assert(pterm.argc == 2);
  assert(strcmp(pterm.argv[0], "echo") == 0);
  assert(strcmp(pterm.argv[1], "a") == 0);
  assert(pterm.argv[2] == NULL);

  parsed_term_t pterm2 = parse_term(pterm.tail, &pterm, 1);
  assert(pterm2.tail == NULL);
  assert(pterm2.kind == P_TERM_BACKGROUND);
  assert(pterm2.argc == 2);
  assert(strcmp(pterm2.argv[0], "ls") == 0);
  assert(strcmp(pterm2.argv[1], "b") == 0);
  assert(pterm2.argv[2] == NULL);

  free(pterm.argv[0]);
  free(pterm.argv);
  free(pterm2.argv);
}

void test_parse_term__amp_not_terminal() {
  char buf[] = "echo & echo";
  parsed_term_t pterm = parse_term(buf, NULL, 0);
  assert(pterm.kind == P_TERM_PARSE_ERROR);
}

void test_parse_pipeline__basic_echo() {
  char buf[] = "echo";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == Ok);
  assert(cmd.execution_mode == Block);
  assert(cmd.terms_count == 1);
  assert(strcmp(cmd.terms[0].argv[0], "echo") == 0);
  assert(cmd.tail == NULL);
  free(cmd.terms[0].argv);
  free(cmd.terms);
}

void test_parse_pipeline__2_echos() {
  char buf[] = "echo | echo";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == Ok);
  assert(cmd.execution_mode == Block);
  assert(cmd.terms_count == 2);
  assert(strcmp(cmd.terms[0].argv[0], "echo") == 0);
  assert(strcmp(cmd.terms[1].argv[0], "echo") == 0);
  assert(cmd.tail == NULL);
  free(cmd.terms[0].argv);
  free(cmd.terms[1].argv);
  free(cmd.terms);
}

void test_parse_pipeline__pipe_as_last() {
  char buf[] = "echo | ";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == ParseError);
}

void test_parse_pipeline__anything_after_semicolon() {
  char buf[] = "echo ;&&&&&&askjf9203ur ";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == Ok);
  free(cmd.terms[0].argv);
  free(cmd.terms);
}

void test_parse_pipeline__pipe_after_semi() {
  char buf[] = "echo ;| ";
  command_pipeline_t first = parse_pipeline(buf);
  free(first.terms[0].argv);
  free(first.terms);
  command_pipeline_t cmd = parse_pipeline(first.tail);
  assert(cmd.result_kind == ParseError);
}

void test_parse_pipeline__pipe_after_pipe() {
  char buf[] = "echo || echo";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == ParseError);
}

void test_parse_pipeline__amp_after_pipe() {
  char buf[] = "echo |& echo";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == ParseError);
}

void test_parse_pipeline__2_echos_amp() {
  char buf[] = "echo | echo&";
  command_pipeline_t cmd = parse_pipeline(buf);
  assert(cmd.result_kind == Ok);
  assert(cmd.execution_mode == Background);
  assert(cmd.terms_count == 2);
  assert(cmd.terms[0].argc == 1);
  assert(cmd.terms[1].argc == 1);
  assert(strcmp(cmd.terms[0].argv[0], "echo") == 0);
  assert(strcmp(cmd.terms[1].argv[0], "echo") == 0);
  assert(cmd.tail == NULL);

  free(cmd.terms[0].argv[0]);
  free(cmd.terms[0].argv);
  free(cmd.terms[1].argv);
  free(cmd.terms);
}

#endif
