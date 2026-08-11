#include "work_stealing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <inttypes.h>

int termux_work_stealing_init(work_stealing_scheduler_t *scheduler, uint32_t worker_count) {
  if (!scheduler || worker_count == 0 || worker_count > TERMUX_MAX_WORKERS) return -1;

  memset(scheduler, 0, sizeof(*scheduler));
  scheduler->worker_count = worker_count;
  scheduler->active_workers = worker_count;

  for (uint32_t i = 0; i < worker_count; i++) {
    work_queue_t *queue = &scheduler->queues[i];
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
  }

  return 0;
}

void termux_work_stealing_destroy(work_stealing_scheduler_t *scheduler) {
  if (!scheduler) return;
  memset(scheduler, 0, sizeof(*scheduler));
}

int termux_work_queue_push(work_queue_t *queue, const work_item_t *item) {
  if (!queue || !item) return -1;

  uint32_t tail = atomic_load((atomic_uint *)&queue->tail);
  uint32_t next_tail = (tail + 1) % TERMUX_WORK_QUEUE_SIZE;

  if (next_tail == atomic_load((atomic_uint *)&queue->head)) {
    return -2;
  }

  queue->items[tail] = *item;
  atomic_store((atomic_uint *)&queue->tail, next_tail);
  atomic_fetch_add((atomic_uint *)&queue->count, 1);

  return 0;
}

int termux_work_queue_pop(work_queue_t *queue, work_item_t *item) {
  if (!queue || !item) return -1;

  uint32_t head = atomic_load((atomic_uint *)&queue->head);
  uint32_t tail = atomic_load((atomic_uint *)&queue->tail);

  if (head == tail) {
    return -2;
  }

  *item = queue->items[head];
  atomic_store((atomic_uint *)&queue->head, (head + 1) % TERMUX_WORK_QUEUE_SIZE);
  atomic_fetch_sub((atomic_uint *)&queue->count, 1);

  return 0;
}

int termux_work_queue_try_steal(work_queue_t *from_queue, work_item_t *item) {
  if (!from_queue || !item) return -1;

  uint32_t head = atomic_load((atomic_uint *)&from_queue->head);
  uint32_t tail = atomic_load((atomic_uint *)&from_queue->tail);

  if (head == tail || (tail + TERMUX_WORK_QUEUE_SIZE - head) % TERMUX_WORK_QUEUE_SIZE < 2) {
    return -2;
  }

  uint32_t steal_idx = (head + ((tail - head) / 2)) % TERMUX_WORK_QUEUE_SIZE;
  *item = from_queue->items[steal_idx];

  return 0;
}

uint32_t termux_work_queue_size(const work_queue_t *queue) {
  if (!queue) return 0;
  return atomic_load((atomic_uint *)&queue->count);
}

uint32_t termux_work_queue_estimate_load(work_stealing_scheduler_t *scheduler, uint32_t worker_id) {
  if (!scheduler || worker_id >= scheduler->worker_count) return 0;

  uint32_t total_load = 0;
  for (uint32_t i = 0; i < scheduler->worker_count; i++) {
    total_load += termux_work_queue_size(&scheduler->queues[i]);
  }

  if (total_load == 0) return 0;

  uint32_t worker_load = termux_work_queue_size(&scheduler->queues[worker_id]);
  return (worker_load * 100) / total_load;
}

int termux_work_stealing_rebalance(work_stealing_scheduler_t *scheduler, uint32_t threshold) {
  if (!scheduler || threshold == 0) return -1;

  uint32_t total_work = 0;
  for (uint32_t i = 0; i < scheduler->worker_count; i++) {
    total_work += termux_work_queue_size(&scheduler->queues[i]);
  }

  if (total_work == 0) return 0;

  uint32_t ideal_per_worker = (total_work + scheduler->worker_count - 1) / scheduler->worker_count;
  uint32_t rebalance_count = 0;

  for (uint32_t i = 0; i < scheduler->worker_count; i++) {
    uint32_t queue_size = termux_work_queue_size(&scheduler->queues[i]);

    if (queue_size > ideal_per_worker + threshold) {
      for (uint32_t j = 0; j < scheduler->worker_count; j++) {
        if (i != j && termux_work_queue_size(&scheduler->queues[j]) < ideal_per_worker) {
          work_item_t item;
          if (termux_work_queue_try_steal(&scheduler->queues[i], &item) == 0) {
            if (termux_work_queue_push(&scheduler->queues[j], &item) == 0) {
              rebalance_count++;
              atomic_fetch_add((atomic_ulong *)&scheduler->total_steals, 1);
            }
          }
        }
      }
    }
  }

  return rebalance_count;
}

uint64_t termux_work_stealing_total_steals(const work_stealing_scheduler_t *scheduler) {
  if (!scheduler) return 0;
  return atomic_load((atomic_ulong *)&scheduler->total_steals);
}

uint64_t termux_work_stealing_total_processed(const work_stealing_scheduler_t *scheduler) {
  if (!scheduler) return 0;
  return atomic_load((atomic_ulong *)&scheduler->total_items_processed);
}

double termux_work_stealing_efficiency(const work_stealing_scheduler_t *scheduler) {
  if (!scheduler || scheduler->total_items_processed == 0) return 0.0;

  uint64_t steals = termux_work_stealing_total_steals(scheduler);
  uint64_t processed = termux_work_stealing_total_processed(scheduler);

  if (processed == 0) return 0.0;

  double steal_overhead = (double)steals / (double)processed;
  return 1.0 - (steal_overhead * 0.01);
}

void termux_work_stealing_print_stats(const work_stealing_scheduler_t *scheduler) {
  if (!scheduler) return;

  printf("\n");
  printf("================================================================================\n");
  printf("                    WORK STEALING SCHEDULER STATISTICS\n");
  printf("================================================================================\n");
  printf("Active Workers: %u\n", scheduler->active_workers);
  printf("Total Steals: %" PRIu64 "\n", termux_work_stealing_total_steals(scheduler));
  printf("Total Items Processed: %" PRIu64 "\n", termux_work_stealing_total_processed(scheduler));
  printf("Steal Efficiency: %.2f%%\n", termux_work_stealing_efficiency(scheduler) * 100.0);

  printf("\nPer-Worker Queue Status:\n");
  for (uint32_t i = 0; i < scheduler->worker_count; i++) {
    uint32_t load = termux_work_queue_estimate_load((work_stealing_scheduler_t *)scheduler, i);
    uint32_t queue_size = termux_work_queue_size(&scheduler->queues[i]);
    printf("  Worker %u: %u items, %u%% load\n", i, queue_size, load);
  }

  printf("\nLoad Balancing Metrics:\n");
  uint32_t min_load = 100, max_load = 0;
  for (uint32_t i = 0; i < scheduler->worker_count; i++) {
    uint32_t load = termux_work_queue_estimate_load((work_stealing_scheduler_t *)scheduler, i);
    if (load < min_load) min_load = load;
    if (load > max_load) max_load = load;
  }
  printf("  Min Load: %u%%\n", min_load);
  printf("  Max Load: %u%%\n", max_load);
  printf("  Load Imbalance: %u%%\n", max_load - min_load);

  printf("================================================================================\n\n");
}
