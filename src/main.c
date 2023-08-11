// TODO: Rewrite in zig

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "helpers.h"
#include "parse.h"

#define JOBS_COUNT 2

void eval(char *buf);
void handle_sigchld(int signum);

// JOB_ID -> PID
static pid_t JOBS[JOBS_COUNT] = {0};
// JOB_ID -> GROUP_ID
static size_t JOBS_TO_GROUP[JOBS_COUNT] = {0};
// GROUP_ID -> PID
static char *GROUP_ID_TO_OWNED_BUF[JOBS_COUNT] = {0};
// GROUP_ID -> How many jobs are left in the group
static size_t GROUP_ID_TO_SHARED_COUNT[JOBS_COUNT] = {0};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, handle_sigchld);

  if (argc > 2 && strcmp(argv[1], "-c") == 0) {
    eval(argv[2]);
    exit(EX_OK);
  } else if (argc != 1) {
    errx(EX_USAGE, "Invalid usage\n");
  }

  char *buf = NULL;
  size_t buf_len = 0;
  ssize_t len;

  while ((len = getline(&buf, &buf_len, stdin)) != -1) {
    assert((size_t)len == strlen(buf));

    while (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
      len -= 1;
    }

    eval(buf);
  }

  free(buf);

  return EX_OK;
}

void eval(char *buffer) {
  if (buffer == NULL) {
    return;
  }

  const command_t command = parse_cmd(buffer);
  char *const buf = command.tail;

  if (command.kind == None) {
    return;
  }

  if (command.kind == ParseError) {
    fprintf(stderr, "\nParse Error\n");
    return;
  }

  int forks[command.terms_count];

  // pipe terms them
  int in = STDIN_FILENO;
  for (size_t i = 0; i < command.terms_count; i++) {
    char **const argv = command.terms[i].argv;
    const size_t argc = command.terms[i].argc;
    char *const command_name = argv[0];

    if (strcmp(command_name, "exit") == 0) {
      int code = atoi(argv[1]);
      if (code == EX_OK) {
        printf("Exitting ok...\n");
      } else {
        printf("Exitting fail...\n");
      }

      exit(code);
    } else if (strcmp(command_name, "cd") == 0) {
      int r = chdir(argc == 1 ? getenv("HOME") : argv[1]);
      assert(r != -1 && "cd failed");
      todo("| and & for cd");
    }

    int pipefd[2];

    if (i != command.terms_count - 1) {
      assert(pipe(pipefd) != -1 && "Pipe failed");
    }
    const pid_t main_pid = getpid();
    const pid_t forked = fork();
    assert(forked != -1 && "Fork failed");
    forks[i] = forked;

    if (forked == 0) {
      if (command.terms[command.terms_count - 1].kind == Background) {
        setgid(main_pid > 0 ? main_pid : -main_pid);
      }
      dup2(in, STDIN_FILENO);
      close(in);
      if (i != command.terms_count - 1) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
      }
      execvp(command_name, argv);
      errx(EXIT_FAILURE, "execvp error\n");
    }

    if (i != command.terms_count - 1) {
      in = pipefd[0];
      close(pipefd[1]);
    }
  }

  if (command.terms[command.terms_count - 1].kind == Block) {
    for (size_t i = 0; i < command.terms_count; i++) {
      int wstatus;
      do {
        const pid_t w = waitpid(forks[i], &wstatus, WUNTRACED);
        assert(w != -1 && "Wait failed");
      } while (!(WIFEXITED(wstatus) || WIFSIGNALED(wstatus)));
    }
  } else if (command.terms_count > 0) {
    int job_id = -1;
    for (size_t i = 0, j = 0; i < command.terms_count; j++) {
      if (j >= JOBS_COUNT) {
        errx(EXIT_FAILURE, "Too many jobs.");
      }
      if (JOBS[j] == 0) {
        JOBS[j] = forks[i];
        job_id = j;
        i += 1;
      }
    }
    assert(job_id != -1 && "Too many jobs.");
    for (size_t group_id = 0; group_id < JOBS_COUNT; group_id++) {
      if (GROUP_ID_TO_SHARED_COUNT[group_id] == 0) {
        GROUP_ID_TO_SHARED_COUNT[group_id] = command.terms_count;
        GROUP_ID_TO_OWNED_BUF[group_id] = command.terms[0].argv[0];
        JOBS_TO_GROUP[job_id] = group_id;
        break;
      }
      assert(group_id != JOBS_COUNT - 1 && "Too many groups");
    }
  }

  free(command.terms);
  return eval(buf);
}

void handle_sigchld(int signum) {
  assert(signum == SIGCHLD);
  int wstatus;
  pid_t pid;

  while ((pid = waitpid(0, &wstatus, WNOHANG)) > 0) {
    if (!(WIFEXITED(wstatus) || WIFSIGNALED(wstatus))) {
      continue;
    }
    for (size_t job_id = 0; job_id < JOBS_COUNT; job_id++) {
      if (JOBS[job_id] != pid) {
        continue;
      }
      size_t group_id = JOBS_TO_GROUP[job_id];

      GROUP_ID_TO_SHARED_COUNT[group_id] -= 1;

      if (GROUP_ID_TO_SHARED_COUNT[group_id] == 0) {
        free(GROUP_ID_TO_OWNED_BUF[group_id]);
        GROUP_ID_TO_OWNED_BUF[group_id] = NULL;
        JOBS[job_id] = 0;
      }

      printf("[%lu] (%d) completed\n", job_id + 1, pid);
      break;
    }
  }
}
