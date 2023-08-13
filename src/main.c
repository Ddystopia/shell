// TODO: Rewrite in zig

#include <assert.h>
#include <err.h>
#include <signal.h>
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

void handle_sigchld(int signum);

static jobs_container_t JOBS = {
    .job_id_to_pid = {0}, //
    .job_id_to_group_id = {0},
    .job_id_to_argv = {0}, //
    .group_id_to_owned_buf = {0}, //
    .group_id_to_shared_count = {0} //
};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, handle_sigchld);

  if (argc > 2 && strcmp(argv[1], "-c") == 0) {
    eval(argv[2], &JOBS);
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

    eval(buf, &JOBS);
  }

  free(buf);

  return EX_OK;
}

void handle_sigchld(int signum) {
  assert(signum == SIGCHLD);
  int wstatus;
  pid_t pid;

  while ((pid = waitpid(0, &wstatus, WNOHANG)) > 0) {
    if (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus)) {
      continue;
    }
    for (job_id_t job_id = 0; job_id < JOBS_COUNT; job_id++) {
      if (JOBS.job_id_to_pid[job_id] != pid) {
        continue;
      }
      group_id_t group_id = JOBS.job_id_to_group_id[job_id];

      JOBS.group_id_to_shared_count[group_id] -= 1;

      free(JOBS.job_id_to_argv[job_id]);
      if (JOBS.group_id_to_shared_count[group_id] == 0) {
        free(JOBS.group_id_to_owned_buf[group_id]);
        JOBS.group_id_to_owned_buf[group_id] = NULL;
        JOBS.job_id_to_group_id[job_id] = 0;
        JOBS.job_id_to_pid[job_id] = 0;
      }

      printf("[%u] (%d) completed\n", job_id + 1, pid);
      return;
    }
    errx(EXIT_FAILURE, "Job with pid %d was not in the table", pid);
  }
}
