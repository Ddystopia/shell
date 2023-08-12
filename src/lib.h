#ifndef __MYSHELL_H
#define __MYSHELL_H

#include <sys/types.h>
#include <stddef.h>

// ===== Jobs stuff =====

#define JOBS_COUNT 2

typedef struct jobs_container_t {
  pid_t jobs[JOBS_COUNT];
  size_t jobs_to_group[JOBS_COUNT];
  char *group_id_to_owned_buf[JOBS_COUNT];
  size_t group_id_to_shared_count[JOBS_COUNT];
} jobs_container_t;

// ===== Eval stuff =====

void eval(char *buffer, jobs_container_t jobs);

// ===== Parse stuff =====

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

#endif
