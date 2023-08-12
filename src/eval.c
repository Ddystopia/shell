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
#include "lib.h"

void eval(char *buffer, jobs_container_t jobs) {
  if (buffer == NULL) {
    return;
  }

  const command_t command = parse_cmd(buffer);
  char *const buf = command.tail;

  if (command.result_kind == None) {
    return;
  }

  if (command.result_kind == ParseError) {
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
      if (command.terminator_kind == Background) {
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

  if (command.terminator_kind == Block) {
    for (size_t i = 0; i < command.terms_count; i++) {
      int wstatus;
      do {
        const pid_t w = waitpid(forks[i], &wstatus, WUNTRACED);
        assert(w != -1 && "Wait failed");
      } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
    }
  } else if (command.terms_count > 0) {
    int job_id = -1;
    for (size_t i = 0, j = 0; i < command.terms_count; j++) {
      if (j >= JOBS_COUNT) {
        errx(EXIT_FAILURE, "Too many jobs.");
      }
      if (jobs.jobs[j] == 0) {
        jobs.jobs[j] = forks[i];
        job_id = j;
        i += 1;
      }
    }
    assert(job_id != -1 && "Too many jobs.");
    for (size_t group_id = 0; group_id < JOBS_COUNT; group_id++) {
      if (jobs.group_id_to_shared_count[group_id] == 0) {
        jobs.group_id_to_shared_count[group_id] = command.terms_count;
        jobs.group_id_to_owned_buf[group_id] = command.terms[0].argv[0];
        jobs.jobs_to_group[job_id] = group_id;
        break;
      }
      assert(group_id != JOBS_COUNT - 1 && "Too many groups");
    }
  }

  free(command.terms);
  return eval(buf, jobs);
}
