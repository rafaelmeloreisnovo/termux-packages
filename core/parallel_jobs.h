#ifndef TERMUX_PARALLEL_JOBS_H
#define TERMUX_PARALLEL_JOBS_H

#include <stdint.h>
#include <sys/types.h>

#define TERMUX_MAX_JOBS 8
#define TERMUX_JOB_NAME_LEN 64

typedef enum {
  TERMUX_JOB_RUNNING = 0,
  TERMUX_JOB_COMPLETED = 1,
  TERMUX_JOB_FAILED = 2,
} termux_job_status_t;

struct termux_job {
  char job_name[TERMUX_JOB_NAME_LEN];
  pid_t pid;
  termux_job_status_t status;
  int exit_code;
};

struct termux_job_status {
  uint32_t total_jobs;
  uint32_t completed_jobs;
};

struct termux_job_pool {
  struct termux_job jobs[TERMUX_MAX_JOBS];
  uint8_t max_jobs;
  uint8_t active_jobs;
};

struct termux_job_pool* termux_job_pool_create(uint8_t max_jobs);
int termux_job_pool_submit(struct termux_job_pool *pool,
                           const char *job_name,
                           const char *executable,
                           char *const argv[],
                           char *const envp[]);
int termux_job_pool_wait_any(struct termux_job_pool *pool, int *out_exit_code);
int termux_job_pool_wait_all(struct termux_job_pool *pool);
int termux_job_pool_get_status(struct termux_job_pool *pool,
                               struct termux_job_status *out_status);
void termux_job_pool_print_status(struct termux_job_pool *pool);
void termux_job_pool_reset(struct termux_job_pool *pool);

#endif
