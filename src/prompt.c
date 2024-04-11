#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int prompt(int fd) {
  // Obtain the current time
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  if (tm_now == NULL) {
    perror("localtime failed");
    exit(EXIT_FAILURE);
  }

  // Format the time as "HH:MM"
  char time_str[6]; // HH:MM\0
  if (strftime(time_str, sizeof(time_str), "%H:%M", tm_now) == 0) {
    fprintf(stderr, "Failed to format time\n");
    exit(EXIT_FAILURE);
  }

  // Get the real username
  char *username = getlogin();
  if (username == NULL) {
    // Fall back to getpwuid if getlogin fails
    struct passwd *pw = getpwuid(geteuid());
    if (pw == NULL) {
      perror("Failed to get username");
      exit(EXIT_FAILURE);
    }
    username = pw->pw_name;
  }

  // Get the hostname
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    perror("Failed to get hostname");
    exit(EXIT_FAILURE);
  }

  // Prompt symbol
  char *prompt = "#";

  // Calculate the total length of the final string
  size_t total_length = strlen(time_str) + strlen(username) + strlen(hostname) +
                        strlen(prompt) +
                        4; // Includes spaces and null terminator

  // Allocate memory for the final string
  char final_str[total_length];

  // Format the final string
  snprintf(final_str, total_length, "%s %s@%s %s", time_str, username, hostname,
           prompt);

  // Write the final string to the file descriptor
  ssize_t bytes_written = write(fd, final_str, strlen(final_str));
  if (bytes_written == -1) {
    perror("write failed");
    exit(EXIT_FAILURE);
  }

  return 0;
}
