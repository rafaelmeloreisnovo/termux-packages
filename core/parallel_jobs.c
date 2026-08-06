#include "parallel_jobs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

static struct termux_job_pool global_job_pool;

struct termux_job_pool* termux_job_pool_create(uint8_t max_jobs) {
  if (max_jobs > TERMUX_MAX_JOBS) {
    max_jobs = TERMUX_MAX_JOBS;
  }

  struct termux_job_pool *pool = &global_job_pool;
  memset(pool, 0, sizeof(*pool));
  pool->max_jobs = max_jobs;
  pool->active_jobs = 0;

  return pool;
}

int termux_job_pool_submit(struct termux_job_pool *pool,
                           const char *job_name,
                           const char *executable,
                           char *const argv[],
                           char *const envp[]) {
  if (!pool || !job_name || !executable || !argv) {
    return -1;
  }

  if (pool->active_jobs >= pool->max_jobs) {
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    execve(executable, argv, envp);
    _exit(127);
  }

  struct termux_job *job = &pool->jobs[pool->active_jobs];
  strncpy(job->job_name, job_name, TERMUX_JOB_NAME_LEN - 1);
  job->job_name[TERMUX_JOB_NAME_LEN - 1] = '\0';
  job->pid = pid;
  job->status = TERMUX_JOB_RUNNING;
  job->exit_code = 0;

  pool->active_jobs++;
  return 0;
}

int termux_job_pool_wait_any(struct termux_job_pool *pool, int *out_exit_code) {
  if (!pool || pool->active_jobs == 0) {
    return -1;
  }

  int status;
  pid_t completed_pid = waitpid(-1, &status, 0);
  if (completed_pid < 0) {
    return -1;
  }

  int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  for (size_t i = 0; i < pool->active_jobs; i++) {
    struct termux_job *job = &pool->jobs[i];
    if (job->pid == completed_pid) {
      job->status = TERMUX_JOB_COMPLETED;
      job->exit_code = exit_code;

      if (out_exit_code) {
        *out_exit_code = exit_code;
      }

      if (i < (size_t)pool->active_jobs - 1) {
        pool->jobs[i] = pool->jobs[pool->active_jobs - 1];
      }
      pool->active_jobs--;

      return 0;
    }
  }

  return -1;
}

int termux_job_pool_wait_all(struct termux_job_pool *pool) {
  if (!pool) return -1;

  while (pool->active_jobs > 0) {
    int status;
    pid_t completed_pid = waitpid(-1, &status, 0);
    if (completed_pid < 0) {
      return -1;
    }

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    for (size_t i = 0; i < pool->active_jobs; i++) {
      struct termux_job *job = &pool->jobs[i];
      if (job->pid == completed_pid) {
        job->status = TERMUX_JOB_COMPLETED;
        job->exit_code = exit_code;

        if (i < (size_t)pool->active_jobs - 1) {
          pool->jobs[i] = pool->jobs[pool->active_jobs - 1];
        }
        pool->active_jobs--;
        break;
      }
    }
  }

  return 0;
}

int termux_job_pool_get_status(struct termux_job_pool *pool,
                               struct termux_job_status *out_status) {
  if (!pool || !out_status) return -1;

  out_status->total_jobs = pool->active_jobs;
  out_status->completed_jobs = 0;

  for (size_t i = 0; i < pool->active_jobs; i++) {
    const struct termux_job *job = &pool->jobs[i];
    if (job->status == TERMUX_JOB_COMPLETED) {
      out_status->completed_jobs++;
    }
  }

  return 0;
}

void termux_job_pool_print_status(struct termux_job_pool *pool) {
  if (!pool) return;

  printf("Job Pool Status:\n");
  printf("  Max jobs: %u\n", pool->max_jobs);
  printf("  Active jobs: %u\n", pool->active_jobs);

  for (size_t i = 0; i < pool->active_jobs; i++) {
    const struct termux_job *job = &pool->jobs[i];
    const char *status_str = job->status == TERMUX_JOB_RUNNING ? "RUNNING" : "COMPLETED";
    printf("  [%zu] %s (PID %d) - %s (exit: %d)\n",
           i, job->job_name, job->pid, status_str, job->exit_code);
  }
}

void termux_job_pool_reset(struct termux_job_pool *pool) {
  if (!pool) return;
  memset(pool, 0, sizeof(*pool));
}
