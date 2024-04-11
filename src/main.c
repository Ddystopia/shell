// TODO: Rewrite in zig

#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "helpers.h"
#include "lib.h"

/*
6  6p - base
10 4p - -d - demon
12 2p - &
14 2p - Makefile
19 5b - timeout
20 1p - -t <timeout>
25 5p - Signals
*/

void help(void) {
  printf("Usage: ./main [-h] [-p <port>] [-c [-e <command>]] [-s]\n");
  printf("Options:\n");
  printf("  -h\t\t\tShow this help message and exit\n");
  printf("  -p <port>\t\tPort number to listen on\n");
  printf("  -c\t\t\tRun as a client\n");
  printf("  -e <command>\t\tCommand to run\n");
  printf("  -s\t\t\tRun as a server\n");
  printf("  -d\t\t\tDaemonize\n");
  printf("  -t <timeout>\t\tTimeout (in milliseconds)\n");
}

void sighup_handler(int signum) {
  if (signum != SIGHUP) {
    return;
  }
}

void daemonize() {
  pid_t pid;

  /* Fork off the parent process */
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* On success: The child process becomes session leader */
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  /* Catch, ignore and handle signals */
  signal(SIGHUP, sighup_handler);

  /* Fork off for the second time*/
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* Set new file permissions */
  umask(0);

  /* Change the working directory to the root directory */
  /* or another appropriated directory */
  chdir("/");

  /* Close all open file descriptors */
  int x;
  for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
    close(x);
  }

  FILE *fp_stdout = fopen("/tmp/shell_daemon_stdout.log", "a+");
  FILE *fp_stderr = fopen("/tmp/shell_daemon_stderr.log", "a+");

  if (fp_stdout != NULL) {
    dup2(fileno(fp_stdout), STDOUT_FILENO);
    setvbuf(stdout, NULL, _IOLBF, 0); // Optional: Line buffering
  }

  if (fp_stderr != NULL) {
    dup2(fileno(fp_stderr), STDERR_FILENO);
    setvbuf(stderr, NULL, _IOLBF, 0); // Optional: Line buffering
  }
}

/**
Main function

cli: `./main [-h] [-p <port>] [-c [-e <command>]] [-s]`
*/
int main(int argc, char *argv[]) {
  bool is_client = false;
  bool is_server = false;
  bool is_daemon = false;
  uint16_t port = 0;
  uint64_t timeout = 0;

  // search -h
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      help();
      exit(EX_OK);
    } else if (strcmp(argv[i], "-d") == 0) {
      is_daemon = true;
    } else if (strcmp(argv[i], "-c") == 0) {
      is_client = true;
    } else if (strcmp(argv[i], "-s") == 0) {
      is_server = true;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (i + 1 >= argc) {
        help();
        errx(EX_USAGE, "Invalid usage: -t requires an argument\n");
      }
      timeout = (uint64_t)atoll(argv[i + 1]);
      if (timeout == 0) {
        help();
        errx(EX_USAGE, "Invalid usage: -t requires a positive argument\n");
      }
      i++;
    } else if (strcmp(argv[i], "-p") == 0) {
      if (i + 1 >= argc) {
        help();
        errx(EX_USAGE, "Invalid usage: -p requires an argument\n");
      }
      port = (u_int16_t)atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "-e") == 0) {
      if (i + 1 >= argc || !is_client) {
        help();
        errx(EX_USAGE, "Invalid usage: -e requires an argument and -c\n");
      }
      i++;
      todo();
    } else {
      help();
      errx(EX_USAGE, "Invalid usage: unknown option %s\n", argv[i]);
    }
  }

  if (port == 0) {
    help();
    errx(EX_USAGE, "Invalid usage: -p is required\n");
  }

  if (is_server && is_client) {
    errx(EX_USAGE, "Invalid usage: -c and -s are mutually exclusive\n");
  }

  if (is_daemon && is_client) {
    errx(EX_USAGE, "Invalid usage: -d and -c are mutually exclusive\n");
  }

  if (is_client) {
    return client(port);
  }

  if (is_daemon) {
    daemonize();
  }

  return server(port, timeout);
}
