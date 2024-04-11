#include <arpa/inet.h>
#include <err.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "helpers.h"
#include "lib.h"

void read_prompt(int fd) {
  char buffer[1024] = {0};
  ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
  if (len == -1) {
    err(EX_IOERR, "Failed to read from socket");
  }
  buffer[len] = '\0';
  printf("%s", buffer);
}

void *sender_thread(void *arg) {
  int sockfd = *((int *)arg);

  char *buf = NULL;
  size_t buf_len = 0;
  ssize_t len;

  while ((len = getline(&buf, &buf_len, stdin)) != -1) {
    // Handling input as before, but now only sending data
    assert((size_t)len == strlen(buf));
    while (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
      len -= 1;
    }
    buf[len] = '\n';

    if (write(sockfd, buf, len + 1) == -1) {
      err(EX_IOERR, "Failed to write to socket");
    }
  }

  free(buf);

  return NULL;
}

int client(uint16_t port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    err(EX_OSERR, "Socket creation failed");
  }

  struct sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP

  if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
    errx(EX_OSERR, "Failed to connect");
  }

  pthread_t send_thread;
  if (pthread_create(&send_thread, NULL, sender_thread, &sockfd) != 0) {
    errx(EX_OSERR, "Failed to create receiver thread");
  }

  char buffer[1024];

  while (1) {
    memset(buffer, 0, sizeof(buffer));
    ssize_t len = read(sockfd, buffer, sizeof(buffer) - 1);
    if (len == -1) {
      err(EX_IOERR, "Failed to read from socket");
    } else if (len == 0) {
      printf("Server closed connection\n");
      break;
    }

    write(STDOUT_FILENO, buffer, len);
  }

  pthread_cancel(send_thread);

  return EX_OK;
}
