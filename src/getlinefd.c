#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

ssize_t getlinefd(char **lineptr, size_t *n, int fd) {
  if (*lineptr == NULL) {
    *n = 0;
  }
  size_t pos = 0;
  ssize_t read_bytes;
  while (1) {
    if (pos + 1 >= *n) {
      *n = *n == 0 ? 128 : *n * 2;
      *lineptr = realloc(*lineptr, *n);
      if (*lineptr == NULL) {
        return -1;
      }
    }
    read_bytes = read(fd, *lineptr + pos, *n - pos);
    if (read_bytes == -1) {
      return (size_t)-1;
    }
    if (read_bytes == 0) {
      return pos == 0 ? (size_t)-1 : pos;
    }
    pos += read_bytes;
    if ((*lineptr)[pos - 1] == '\n') {
      return pos;
    }
  }
}

#include <stdlib.h>     // For realloc
#include <sys/select.h> // For select
#include <unistd.h>     // For read

ssize_t getlinefd_with_timeout(char **lineptr, size_t *n, int fd,
                               uint32_t timeout_ms) {
  if (*lineptr == NULL) {
    *n = 0;
  }
  size_t pos = 0;
  ssize_t read_bytes;
  struct timeval timeout;
  fd_set readfds;

  while (1) {
    if (pos + 1 >= *n) {
      *n = *n == 0 ? 128 : *n * 2;
      *lineptr = realloc(*lineptr, *n);
      if (*lineptr == NULL) {
        return -1; // Memory allocation failed
      }
    }

    // Set up timeout
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    // Initialize fd_set
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    // Wait for the file descriptor to become ready or for the timeout period
    int select_result = select(fd + 1, &readfds, NULL, NULL, &timeout);

    if (select_result == -1) {
      return -1; // Error occurred in select
    } else if (select_result == 0) {
      return -2; // Timeout occurred
    }

    // Data is available for reading
    read_bytes = read(fd, *lineptr + pos, 1); // Read one byte at a time

    if (read_bytes == -1) {
      return -1; // Error occurred in read
    } else if (read_bytes == 0) {
      // End of file or no data available
      return pos == 0 ? (size_t)-1 : pos;
    }

    pos += read_bytes;

    if ((*lineptr)[pos - 1] == '\n') {
      return pos; // New line character found, return the length read so far
    }
  }
}
