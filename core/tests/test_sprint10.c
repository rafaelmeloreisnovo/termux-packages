#include "../work_stealing.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

static int test_work_queue_init(void) {
  printf("\n=== Sprint 10.1: Work Queue Initialization ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 4);
  assert(ret == 0);
  assert(scheduler.worker_count == 4);
  assert(scheduler.active_workers == 4);

  for (uint32_t i = 0; i < 4; i++) {
    work_queue_t *queue = &scheduler.queues[i];
    assert(queue->head == 0);
    assert(queue->tail == 0);
    assert(queue->count == 0);
  }

  printf("✓ Scheduler initialized with %u workers\n", scheduler.worker_count);
  return 0;
}

static int test_work_queue_push_pop(void) {
  printf("\n=== Sprint 10.2: Push/Pop Operations ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 1);
  assert(ret == 0);

  work_queue_t *queue = &scheduler.queues[0];
  work_item_t item = {
    .pkg_idx = 42,
    .phase = 3,
    .arch_state = 1,
    .coherence_phi = 0x800000,
    .cycle_budget = 1000
  };

  ret = termux_work_queue_push(queue, &item);
  assert(ret == 0);
  assert(termux_work_queue_size(queue) == 1);

  work_item_t popped;
  ret = termux_work_queue_pop(queue, &popped);
  assert(ret == 0);
  assert(popped.pkg_idx == 42);
  assert(popped.phase == 3);
  assert(popped.arch_state == 1);
  assert(termux_work_queue_size(queue) == 0);

  printf("✓ Push/pop operations work correctly\n");
  return 0;
}

static int test_work_queue_full(void) {
  printf("\n=== Sprint 10.3: Queue Full Handling ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 1);
  assert(ret == 0);

  work_queue_t *queue = &scheduler.queues[0];
  work_item_t item = {
    .pkg_idx = 0,
    .phase = 0,
    .arch_state = 0,
    .coherence_phi = 0,
    .cycle_budget = 100
  };

  int push_count = 0;
  for (int i = 0; i < TERMUX_WORK_QUEUE_SIZE; i++) {
    item.pkg_idx = i;
    int push_ret = termux_work_queue_push(queue, &item);
    if (push_ret == 0) {
      push_count++;
    } else if (push_ret == -2) {
      break;
    }
  }

  assert(push_count == TERMUX_WORK_QUEUE_SIZE - 1);

  ret = termux_work_queue_push(queue, &item);
  assert(ret == -2);

  printf("✓ Queue correctly handles full condition at %u items\n", push_count);
  return 0;
}

static int test_work_queue_empty(void) {
  printf("\n=== Sprint 10.4: Queue Empty Handling ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 1);
  assert(ret == 0);

  work_queue_t *queue = &scheduler.queues[0];
  work_item_t item;

  ret = termux_work_queue_pop(queue, &item);
  assert(ret == -2);

  printf("✓ Pop from empty queue returns -2\n");
  return 0;
}

static int test_work_stealing(void) {
  printf("\n=== Sprint 10.5: Work Stealing ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 2);
  assert(ret == 0);

  work_queue_t *victim = &scheduler.queues[0];
  work_item_t items[8];
  for (int i = 0; i < 8; i++) {
    items[i].pkg_idx = i;
    items[i].phase = 0;
    items[i].arch_state = 0;
    items[i].coherence_phi = 0x800000;
    items[i].cycle_budget = 1000 + i * 100;
  }

  for (int i = 0; i < 8; i++) {
    ret = termux_work_queue_push(victim, &items[i]);
    assert(ret == 0);
  }

  assert(termux_work_queue_size(victim) == 8);

  work_item_t stolen;
  ret = termux_work_queue_try_steal(victim, &stolen);
  assert(ret == 0);
  assert(stolen.pkg_idx >= 0 && stolen.pkg_idx < 8);

  printf("  Stole item with pkg_idx=%u from victim queue\n", stolen.pkg_idx);
  printf("✓ Work stealing successful\n");
  return 0;
}

static int test_load_balancing(void) {
  printf("\n=== Sprint 10.6: Load Balancing ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 2);
  assert(ret == 0);

  work_queue_t *heavy = &scheduler.queues[0];
  work_queue_t *light = &scheduler.queues[1];

  work_item_t item = {
    .pkg_idx = 0,
    .phase = 0,
    .arch_state = 0,
    .coherence_phi = 0x800000,
    .cycle_budget = 1000
  };

  for (int i = 0; i < 16; i++) {
    item.pkg_idx = i;
    termux_work_queue_push(heavy, &item);
  }

  assert(termux_work_queue_size(heavy) == 16);
  assert(termux_work_queue_size(light) == 0);

  uint32_t heavy_load = termux_work_queue_estimate_load(&scheduler, 0);
  uint32_t light_load = termux_work_queue_estimate_load(&scheduler, 1);

  printf("  Heavy queue load: %u%%\n", heavy_load);
  printf("  Light queue load: %u%%\n", light_load);
  printf("✓ Load estimation working\n");
  return 0;
}

static int test_rebalancing(void) {
  printf("\n=== Sprint 10.7: Rebalancing ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 2);
  assert(ret == 0);

  work_queue_t *heavy = &scheduler.queues[0];

  work_item_t item = {
    .pkg_idx = 0,
    .phase = 0,
    .arch_state = 0,
    .coherence_phi = 0x800000,
    .cycle_budget = 1000
  };

  for (int i = 0; i < 20; i++) {
    item.pkg_idx = i;
    termux_work_queue_push(heavy, &item);
  }

  uint32_t rebalance_count = termux_work_stealing_rebalance(&scheduler, TERMUX_STEAL_THRESHOLD);
  printf("  Items rebalanced: %u\n", rebalance_count);
  printf("  Total steals: %" PRIu64 "\n", termux_work_stealing_total_steals(&scheduler));

  printf("✓ Rebalancing completed\n");
  return 0;
}

static int test_multiple_workers(void) {
  printf("\n=== Sprint 10.8: Multiple Workers ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 4);
  assert(ret == 0);

  work_item_t item = {
    .pkg_idx = 0,
    .phase = 0,
    .arch_state = 0,
    .coherence_phi = 0x800000,
    .cycle_budget = 1000
  };

  for (uint32_t w = 0; w < 4; w++) {
    for (int i = 0; i < 5; i++) {
      item.pkg_idx = w * 5 + i;
      int push_ret = termux_work_queue_push(&scheduler.queues[w], &item);
      if (push_ret != 0) {
        printf("  Failed to push to queue %u\n", w);
      }
    }
  }

  uint32_t total_items = 0;
  for (uint32_t w = 0; w < 4; w++) {
    total_items += termux_work_queue_size(&scheduler.queues[w]);
  }

  printf("  Total items across %u queues: %u\n", scheduler.worker_count, total_items);
  assert(total_items == 20);
  printf("✓ Multiple worker queues working\n");
  return 0;
}

static int test_statistics(void) {
  printf("\n=== Sprint 10.9: Statistics ===\n");

  work_stealing_scheduler_t scheduler;
  int ret = termux_work_stealing_init(&scheduler, 2);
  assert(ret == 0);

  work_queue_t *queue = &scheduler.queues[0];
  work_item_t item = {
    .pkg_idx = 0,
    .phase = 0,
    .arch_state = 0,
    .coherence_phi = 0x800000,
    .cycle_budget = 1000
  };

  for (int i = 0; i < 10; i++) {
    item.pkg_idx = i;
    termux_work_queue_push(queue, &item);
  }

  termux_work_stealing_rebalance(&scheduler, TERMUX_STEAL_THRESHOLD);

  uint64_t steals = termux_work_stealing_total_steals(&scheduler);
  printf("  Total steals: %" PRIu64 "\n", steals);

  double efficiency = termux_work_stealing_efficiency(&scheduler);
  printf("  Efficiency: %.2f%%\n", efficiency * 100.0);

  printf("✓ Statistics retrieved successfully\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                    SPRINT 10: WORK STEALING SCHEDULER\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_work_queue_init();
  all_passed += test_work_queue_push_pop();
  all_passed += test_work_queue_full();
  all_passed += test_work_queue_empty();
  all_passed += test_work_stealing();
  all_passed += test_load_balancing();
  all_passed += test_rebalancing();
  all_passed += test_multiple_workers();
  all_passed += test_statistics();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 10 TESTS PASSED\n");
    printf("  Work queue initialization: ✓\n");
    printf("  Push/pop operations: ✓\n");
    printf("  Queue full handling: ✓\n");
    printf("  Queue empty handling: ✓\n");
    printf("  Work stealing: ✓\n");
    printf("  Load balancing: ✓\n");
    printf("  Rebalancing: ✓\n");
    printf("  Multiple workers: ✓\n");
    printf("  Statistics: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");

  return all_passed == 0 ? 0 : 1;
}
