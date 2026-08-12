#ifndef TERMUX_WORK_STEALING_H
#define TERMUX_WORK_STEALING_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdatomic.h>

#define TERMUX_WORK_QUEUE_SIZE 256
#define TERMUX_MAX_WORKERS 8
#define TERMUX_STEAL_THRESHOLD 4

typedef struct {
  uint16_t pkg_idx;
  uint32_t phase;
  uint32_t arch_state;
  uint64_t coherence_phi;
  uint32_t cycle_budget;
} work_item_t;

typedef struct {
  _Atomic uint32_t head;
  _Atomic uint32_t tail;
  _Atomic uint32_t count;
  work_item_t items[TERMUX_WORK_QUEUE_SIZE];
  uint8_t _pad[64];
} __attribute__((aligned(128))) work_queue_t;

typedef struct {
  work_queue_t queues[TERMUX_MAX_WORKERS];
  _Atomic uint64_t total_steals;
  _Atomic uint64_t total_items_processed;
  _Atomic uint32_t active_workers;
  uint32_t worker_count;
  uint8_t _pad[32];
} work_stealing_scheduler_t;

int termux_work_stealing_init(work_stealing_scheduler_t *scheduler, uint32_t worker_count);

void termux_work_stealing_destroy(work_stealing_scheduler_t *scheduler);

int termux_work_queue_push(work_queue_t *queue, const work_item_t *item);

int termux_work_queue_pop(work_queue_t *queue, work_item_t *item);

int termux_work_queue_try_steal(work_queue_t *from_queue, work_item_t *item);

uint32_t termux_work_queue_size(const work_queue_t *queue);

uint32_t termux_work_queue_estimate_load(work_stealing_scheduler_t *scheduler, uint32_t worker_id);

int termux_work_stealing_rebalance(work_stealing_scheduler_t *scheduler, uint32_t threshold);

uint64_t termux_work_stealing_total_steals(const work_stealing_scheduler_t *scheduler);

uint64_t termux_work_stealing_total_processed(const work_stealing_scheduler_t *scheduler);

double termux_work_stealing_efficiency(const work_stealing_scheduler_t *scheduler);

void termux_work_stealing_print_stats(const work_stealing_scheduler_t *scheduler);

#endif
