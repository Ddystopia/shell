#ifndef __MYSHELL_H
#define __MYSHELL_H

#include <sys/types.h>
#include <stddef.h>

// ===== Jobs stuff =====

#define JOBS_COUNT 2

static_assert(JOBS_COUNT < (1 << sizeof(pid_t)), "JOBS_COUNT is too big");

typedef uint job_id_t;
typedef uint group_id_t;

typedef struct jobs_container_t {
  pid_t job_id_to_pid[JOBS_COUNT];
  group_id_t job_id_to_group_id[JOBS_COUNT];
  char **job_id_to_argv[JOBS_COUNT];
  char *group_id_to_owned_buf[JOBS_COUNT];
  job_id_t group_id_to_shared_count[JOBS_COUNT];
} jobs_container_t;

// ===== Eval stuff =====

void eval(char *buffer_mut, jobs_container_t *jobs_mut);

// ===== Parse stuff =====

enum executing_mode_t { Background, Block };

enum command_result_kind_t {
  /// Parsed ok
  Ok,
  /// Nothing to parse
  None,
  /// Error while parsing
  ParseError
};

/// Represents single command - name and args, without  anything
/// That type should only be a part of `command_pipelite_t`
typedef struct term_t {
  /// Null terminated array of strings. All strings are pointing at the one allocation.
  /// If execution_mode is `Background`, then you are owner of an allocation at `argv[0]`.
  /// If execution_mode is `Block`, then it is pointing to the buffer passed
  /// to `parse_pipeline` function.
  char **argv;
  // Count of args in argv, without null terminator.
  size_t argc;
} term_t;

/// Represents simultaneously executable array of piped term_t.
/// If `execution_mode` is `Block`, then borrwos from the buffer passed to
/// `parse_pipeline` function.
typedef struct command_pipeline_t {
  enum command_result_kind_t result_kind;
  enum executing_mode_t execution_mode;
  term_t *terms;
  size_t terms_count;
  char *tail;
} command_pipeline_t;

/// Parses buffer to first ';' or '&' or end of string.
/// If `execution_mode` is `Block`, then borrwos from the buffer passed to
/// `parse_pipeline` function.
command_pipeline_t parse_pipeline(char *buf);

#endif
