#include "manifest.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

static ssize_t termux_read_exact(int fd, char *buf, size_t buflen) {
  size_t total = 0;
  while (total < buflen) {
    ssize_t ret = read(fd, buf + total, buflen - total);
    if (ret < 0) return -1;
    if (ret == 0) break;
    total += ret;
  }
  return (ssize_t)total;
}

ssize_t termux_read_file(const char *path, char *buf, size_t buflen) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  ssize_t ret = termux_read_exact(fd, buf, buflen - 1);
  close(fd);
  if (ret >= 0) {
    buf[ret] = '\0';
  }
  return ret;
}

ssize_t termux_write_file(const char *path, const char *buf, size_t buflen) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return -1;
  }
  size_t total = 0;
  while (total < buflen) {
    ssize_t ret = write(fd, buf + total, buflen - total);
    if (ret < 0) {
      close(fd);
      return -1;
    }
    total += ret;
  }
  close(fd);
  return (ssize_t)total;
}

ssize_t termux_append_file(const char *path, const char *buf, size_t buflen) {
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    return -1;
  }
  size_t total = 0;
  while (total < buflen) {
    ssize_t ret = write(fd, buf + total, buflen - total);
    if (ret < 0) {
      close(fd);
      return -1;
    }
    total += ret;
  }
  close(fd);
  return (ssize_t)total;
}

int termux_execve_capture(const char *path, char *const argv[], char *const envp[],
                         char *output_buf, size_t output_size, size_t *output_len) {
  if (!output_buf || output_size == 0) {
    return -1;
  }

  int pipefd[2];
  if (pipe(pipefd) < 0) {
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }

  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    execve(path, argv, envp);
    exit(127);
  }

  close(pipefd[1]);

  size_t bytes_read = 0;
  while (bytes_read < output_size - 1) {
    ssize_t ret = read(pipefd[0], output_buf + bytes_read, output_size - 1 - bytes_read);
    if (ret < 0) {
      close(pipefd[0]);
      waitpid(pid, NULL, 0);
      return -1;
    }
    if (ret == 0) {
      break;
    }
    bytes_read += ret;
  }

  output_buf[bytes_read] = '\0';
  close(pipefd[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }

  if (output_len) {
    *output_len = bytes_read;
  }

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

pid_t termux_spawn_job(const char *path, char *const argv[], char *const envp[]) {
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    execve(path, argv, envp);
    exit(127);
  }

  return pid;
}

int termux_wait_job(pid_t pid) {
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int termux_wait_job_nohang(pid_t pid, int *exited) {
  int status = 0;
  pid_t ret = waitpid(pid, &status, WNOHANG);

  if (ret < 0) {
    return -1;
  }

  if (ret == 0) {
    *exited = 0;
    return 0;
  }

  *exited = 1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int termux_path_exists(const char *path) {
  return access(path, F_OK) == 0;
}

int termux_file_is_readable(const char *path) {
  return access(path, R_OK) == 0;
}

int termux_dir_create(const char *path) {
  if (mkdir(path, 0755) == 0) {
    return 0;
  }
  if (errno == EEXIST) {
    return 0;
  }
  return -1;
}

int termux_dir_create_recursive(const char *path) {
  char buf[512];
  if (strlen(path) >= sizeof(buf)) {
    return -1;
  }

  strcpy(buf, path);

  for (char *p = buf + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }

  return mkdir(buf, 0755) == 0 || errno == EEXIST ? 0 : -1;
}
