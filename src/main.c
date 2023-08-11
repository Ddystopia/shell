// TODO: Rewrite in zig

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdatomic.h>
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

#define JOBS_COUNT 10

void eval(char *buf);
void handle_sigchld(int signum);

static pid_t JOBS[JOBS_COUNT] = {0};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, handle_sigchld);

  if (argc > 2 && strcmp(argv[1], "-c") == 0) {
    eval(argv[2]);
    exit(EX_OK);
  } else if (argc != 1) {
    err(EX_USAGE, "Wrong arguments\n");
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

// NOTE: pipe to connect stdouts to stdins

// TODO: check out exec(3), wait(2), pipe(2), fork(2), pause(2)

void eval(char *buf) {
  if (buf == NULL) {
    return;
  }

  command_t command = parse_cmd(buf);
  buf = command.tail;

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
    char **argv = command.terms[i].argv;
    size_t argc = command.terms[i].argc;
    char *command_name = argv[0];

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

    const pid_t forked = fork();
    assert(forked != -1 && "Fork failed");
    forks[i] = forked;

    if (forked == 0) {
      dup2(in, STDIN_FILENO);
      close(in);
      if (i != command.terms_count - 1) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
      }
      execvp(command_name, argv);
      err(EXIT_FAILURE, "execvp\n");
    }

    if (i != command.terms_count - 1) {
      in = pipefd[0];
      close(pipefd[1]);
    }
  }

  if (command.terms[command.terms_count - 1].kind == Block) {
    for (size_t i = 0; i < command.terms_count; i++) {
      int wstatus;
      const pid_t w = waitpid(forks[i], &wstatus, 0);
      assert(w != -1 && "Wait failed");
    }
  } else {
    for (size_t i = 0, j = 0; i < command.terms_count; j++) {
      if (j >= JOBS_COUNT) {
        err(EXIT_FAILURE, "Too many jobs.");
      }
      if (JOBS[j] == 0) {
        JOBS[j] = forks[i];
        i += 1;
      }
    }
  }

  free(command.terms);
  return eval(buf);
}

void handle_sigchld(int signum) {
  assert(signum == SIGCHLD);
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (size_t i = 0; i < JOBS_COUNT; i++) {
      if (JOBS[i] == pid) {
        JOBS[i] = 0;
        /* printf("[%lu] (%d) completed\n", i + 1, pid); */
        break;
      }
    }
  }
}
