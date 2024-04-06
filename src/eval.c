#include <assert.h>
#include <err.h>
#include <math.h>
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

void eval(char *buffer_mut, jobs_container_t *jobs) {
  command_pipeline_t pipeline_mut;

  do {
    if (buffer_mut == NULL) {
      return;
    }
    pipeline_mut = parse_pipeline(buffer_mut);
    buffer_mut = pipeline_mut.tail;
  } while (pipeline_mut.result_kind == None);

  command_pipeline_t const pipeline = pipeline_mut;
  char *const buf = buffer_mut;

  if (pipeline.result_kind == None) {
    return;
  }

  if (pipeline.result_kind == ParseError) {
    fprintf(stderr, "\nParse Error\n");
    return;
  }

  pid_t forks[pipeline.terms_count];

  // pipe terms them
  int in = STDIN_FILENO;
  for (size_t i = 0; i < pipeline.terms_count; i++) {
    char **const argv = pipeline.terms[i].argv;
    size_t const argc = pipeline.terms[i].argc;
    char *const command_name = argv[0];

    if (strcmp(command_name, "exit") == 0) {
      todo();
      int code = argc == 0 ? EX_OK : atoi(argv[1]);
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
    const bool do_pipe = i != pipeline.terms_count - 1;

    if (do_pipe) {
      assert(pipe(pipefd) != -1 && "Pipe failed");
    }

    const pid_t main_pid = getpid();
    const pid_t forked = fork();
    assert(forked != -1 && "Fork failed");
    forks[i] = forked;

    if (forked == 0) {
      if (pipeline.execution_mode == Background) {
        setpgid(0, abs(main_pid));
      } else {
        setpgid(0, 0);
      }
      dup2(in, STDIN_FILENO);
      close(in);
      if (do_pipe) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
      }
      execvp(command_name, argv);
      errx(EXIT_FAILURE, "execvp error\n");
    }

    if (do_pipe) {
      in = pipefd[0];
      close(pipefd[1]);
    }
  }

  if (pipeline.execution_mode == Block) {
    for (size_t i = 0; i < pipeline.terms_count; i++) {
      int wstatus;
      do {
        const pid_t w = waitpid(forks[i], &wstatus, WUNTRACED);
        assert(w != -1 && "Wait failed");
      } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
      free(pipeline.terms[i].argv);
    }
  } else {
    job_id_t job_id = ~(job_id_t)0;
    for (size_t i = 0, j = 0; i < pipeline.terms_count; j++) {
      if (j >= JOBS_COUNT) {
        errx(EXIT_FAILURE, "Too many jobs.");
      }
      if (jobs->job_id_to_pid[(job_id_t)j] == 0) {
        job_id = (job_id_t)j;
        jobs->job_id_to_pid[job_id] = forks[i];
        i += 1;
      }
    }
    assert(job_id != ~(job_id_t)0 && "Too many jobs.");
    for (group_id_t group_id = 0; group_id < JOBS_COUNT; group_id++) {
      if (jobs->group_id_to_shared_count[group_id] == 0) {
        jobs->group_id_to_shared_count[group_id] = pipeline.terms_count;
        jobs->group_id_to_owned_buf[group_id] = pipeline.terms[0].argv[0];
        jobs->job_id_to_group_id[job_id] = group_id;
        jobs->job_id_to_argv[job_id] = pipeline.terms[0].argv;
        break;
      }
      assert(group_id != JOBS_COUNT - 1 && "Too many groups");
    }
  }

  free(pipeline.terms);
  return eval(buf, jobs);
}
