#include <assert.h>
#include <err.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "helpers.h"
#include "lib.h"

#define MAX_CONNECTIONS 20

static void handle_sigchld(int signum);
static void handle_sigint(int signum);
static int serve(size_t i);

static jobs_container_t JOBS = {
    .job_id_to_pid = {0},                         //
    .job_id_to_group_id = {0},                    //
    .job_id_to_argv = {0},                        //
    .job_id_to_connection_id = {MAX_CONNECTIONS}, //
    .group_id_to_owned_buf = {0},                 //
    .group_id_to_shared_count = {0}               //
};

// int halt = 0;
// atomic
atomic_int halt = 0;

atomic_int sockfd = 0;

atomic_int connfd[MAX_CONNECTIONS] = {0};
atomic_int conn_exits[MAX_CONNECTIONS] = {0};

_Atomic uint32_t last_activity_at[MAX_CONNECTIONS] = {0};

// milliseconds
static uint32_t now_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void *server_thread(void *arg) {
  size_t i = (size_t)arg;
  assert(i < MAX_CONNECTIONS);
  printf("Accepted connection\n");
  serve(i);
  int connfd_i = atomic_exchange(&connfd[i], 0);
  if (connfd_i != 0) {
    close(connfd_i);
  }

  return NULL;
}

static void *watcher_thread(void *arg) {
  uint64_t timeout_ms = (uint64_t)arg;

  while (halt == 0) {
    for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
      uint32_t now = now_ms();
      uint32_t last_activity_at_i = atomic_load(&last_activity_at[i]);
      if (last_activity_at_i == 0) {
        continue;
      }
      if (now - last_activity_at_i > timeout_ms && timeout_ms != 0) {
        uint32_t expected = last_activity_at_i;
        if (atomic_compare_exchange_strong(&last_activity_at[i], &expected,
                                           0)) {

          atomic_store(&conn_exits[i], 1);
        }
      }
    }
    usleep(200 * 1000);
  }

  return NULL;
}

int server(const uint16_t port, const uint64_t timeout) {
  signal(SIGCHLD, handle_sigchld);
  signal(SIGINT, handle_sigint);

  pthread_t watcher_thread_id;
  if (pthread_create(&watcher_thread_id, NULL, watcher_thread,
                     (void *)(uintptr_t)timeout) != 0) {
    err(EXIT_FAILURE, "Failed to create watcher thread");
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    err(EXIT_FAILURE, "Failed to open socket");
  }

  int optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
      0) {
    err(EXIT_FAILURE, "Failed to set socket options");
  }

  struct sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    err(EXIT_FAILURE, "Failed to bind");
  }

  listen(sockfd, 5);

  printf("Listening on port %d\n", port);

  pthread_t server_threads[MAX_CONNECTIONS] = {0};

  while (halt == 0) {
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);

    // Set up the timeout (100ms)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000; // 100ms in microseconds

    // Set up the file descriptor set
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Wait for an incoming connection or a timeout
    const int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (ret == -1) {
      err(EXIT_FAILURE, "Failed to select");
    } else if (ret == 0) {
      // Timeout
      continue;
    }

    const int l_connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
    if (l_connfd == -1) {
      err(EXIT_FAILURE, "Failed to accept");
      continue;
    }

    if (l_connfd == -1) {
      err(EXIT_FAILURE, "Failed to accept");
      continue;
    }
    size_t i = 0;
    while (i < MAX_CONNECTIONS) {
      int expected = 0;
      if (atomic_compare_exchange_strong(&connfd[i], &expected, l_connfd)) {
        break;
      }
      i += 1;
    }
    if (i == MAX_CONNECTIONS) {
      printf("Too many connections\n");
      dprintf(l_connfd, "Too many connections\n");
      close(l_connfd);
      continue;
    }
    if (pthread_create(&server_threads[i], NULL, server_thread, (void *)i) !=
        0) {
      err(EXIT_FAILURE, "Failed to create server thread");
    }
  }

  int sockfd_loaded = atomic_exchange(&sockfd, 0);
  close(sockfd_loaded);

  return 0;
}

static int serve(size_t i) {
  char *buf = NULL;
  size_t buf_len = 0;
  ssize_t len = 0;

  while (atomic_exchange(&conn_exits[i], 0) == 0) {
    int fd = atomic_load(&connfd[i]);

    if (fd == 0) {
      break;
    }

    if (len != -2) {
      atomic_store(&last_activity_at[i], now_ms());
      prompt(fd);
    }

    len = getlinefd_with_timeout(&buf, &buf_len, fd, 100);
    if (len == -1) {
      continue;
    }
    if (len == -2) {
      continue;
    }

    // replace trailing newlines with null termitator
    while (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
      len -= 1;
    }

    printf("Received(%d): %.*s\n", (int)len, (int)len, buf);

    eval_ret_t e_rc = eval(i, fd, buf, &JOBS);

    if (e_rc == EvalExit) {
      break;
    }
    if (e_rc == EvalHalt) {
      halt = 1;
      break;
    }
  }

  free(buf);

  return 0;
}

static void handle_sigchld(int signum) {
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
      size_t connection_id = JOBS.job_id_to_connection_id[job_id];

      JOBS.group_id_to_shared_count[group_id] -= 1;

      free(JOBS.job_id_to_argv[job_id]);
      JOBS.job_id_to_argv[job_id] = NULL;

      if (JOBS.group_id_to_shared_count[group_id] == 0) {
        free(JOBS.group_id_to_owned_buf[group_id]);
        JOBS.group_id_to_owned_buf[group_id] = NULL;
        JOBS.job_id_to_group_id[job_id] = 0;
        JOBS.job_id_to_pid[job_id] = 0;
        JOBS.job_id_to_connection_id[job_id] = MAX_CONNECTIONS;
      }

      if (connection_id != MAX_CONNECTIONS) {
        int fd = atomic_load(&connfd[connection_id]);
        if (fd != 0) {
          dprintf(fd, "\n[%u] (%d) completed\n", job_id + 1, pid);
        }
      }
      return;
    }
    errx(EXIT_FAILURE, "Job with pid %d was not in the table", pid);
  }
}

static void handle_sigint(int signum) {
  if (signum != SIGINT) {
    return;
  }
  if (sockfd != 0) {
    close(sockfd);
  }
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    // swap with 0
    int connfd_i = atomic_exchange(&connfd[i], 0);
    if (connfd_i != 0) {
      close(connfd_i);
    }
  }
  printf("\nExitting...\n");
  exit(EXIT_SUCCESS);
}
