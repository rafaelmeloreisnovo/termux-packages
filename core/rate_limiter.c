#include "rate_limiter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>

/* Forward declaration */
static uint64_t get_rate_limiter_time_ms(void);

/* ============================================================================
 * Rate Limiter Implementation
 * ============================================================================ */

int rate_limiter_init(rate_limiter_t *limiter, uint32_t max_clients,
                      uint64_t global_refill_rate) {
  if (!limiter || max_clients == 0) return -1;

  memset(limiter, 0, sizeof(*limiter));

  limiter->clients =
      (rate_limiter_client_t *)malloc(max_clients * sizeof(rate_limiter_client_t));
  if (!limiter->clients) return -1;

  memset(limiter->clients, 0, max_clients * sizeof(rate_limiter_client_t));

  limiter->max_clients = max_clients;
  limiter->global_refill_rate = global_refill_rate;
  limiter->block_duration_ms = 60000;

  return 0;
}

/* ============================================================================
 * Token Bucket Algorithm
 * ============================================================================ */

int rate_limiter_allow(rate_limiter_t *limiter, const char *client_id,
                       uint32_t tokens_required) {
  if (!limiter || !client_id || tokens_required == 0) return -1;

  rate_limiter_refill(limiter, client_id);

  rate_limiter_client_t *client = NULL;
  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      client = &limiter->clients[i];
      break;
    }
  }

  if (!client) {
    if (limiter->client_count >= limiter->max_clients) {
      return -1;
    }

    client = &limiter->clients[limiter->client_count];
    strcpy(client->client_id, client_id);
    client->max_tokens = limiter->global_refill_rate;
    client->refill_rate = limiter->global_refill_rate;
    client->tokens = limiter->global_refill_rate;
    client->last_refill_ms = get_rate_limiter_time_ms();
    limiter->client_count++;
  }

  if (client->is_blocked) {
    return -1;
  }

  if (client->tokens >= tokens_required) {
    client->tokens -= tokens_required;
    client->request_count++;
    return 0;
  }

  client->denied_count++;
  return -1;
}

/* ============================================================================
 * Token Refill
 * ============================================================================ */

int rate_limiter_refill(rate_limiter_t *limiter, const char *client_id) {
  if (!limiter || !client_id) return -1;

  rate_limiter_client_t *client = NULL;
  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      client = &limiter->clients[i];
      break;
    }
  }

  if (!client) return 0;

  uint64_t now = get_rate_limiter_time_ms();
  uint64_t elapsed_ms = now - client->last_refill_ms;
  uint64_t tokens_to_add = (elapsed_ms * client->refill_rate) / 1000;

  if (tokens_to_add > 0) {
    client->tokens = (client->tokens + tokens_to_add > client->max_tokens)
                        ? client->max_tokens
                        : client->tokens + tokens_to_add;
    client->last_refill_ms = now;
  }

  return 0;
}

/* ============================================================================
 * Token Query
 * ============================================================================ */

uint64_t rate_limiter_get_tokens(rate_limiter_t *limiter,
                                 const char *client_id) {
  if (!limiter || !client_id) return 0;

  rate_limiter_refill(limiter, client_id);

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      return limiter->clients[i].tokens;
    }
  }

  return 0;
}

/* ============================================================================
 * Client Blocking
 * ============================================================================ */

int rate_limiter_block_client(rate_limiter_t *limiter, const char *client_id) {
  if (!limiter || !client_id) return -1;

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      limiter->clients[i].is_blocked = 1;
      return 0;
    }
  }

  return -1;
}

int rate_limiter_unblock_client(rate_limiter_t *limiter,
                                const char *client_id) {
  if (!limiter || !client_id) return -1;

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      limiter->clients[i].is_blocked = 0;
      return 0;
    }
  }

  return -1;
}

/* ============================================================================
 * Adaptive Rate Limiting
 * ============================================================================ */

int rate_limiter_reduce_limit(rate_limiter_t *limiter, const char *client_id,
                              double reduction_factor) {
  if (!limiter || !client_id) return -1;

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      limiter->clients[i].max_tokens =
          (uint64_t)(limiter->clients[i].max_tokens * reduction_factor);
      if (limiter->clients[i].tokens > limiter->clients[i].max_tokens) {
        limiter->clients[i].tokens = limiter->clients[i].max_tokens;
      }
      return 0;
    }
  }

  return -1;
}

int rate_limiter_increase_limit(rate_limiter_t *limiter, const char *client_id,
                                double increase_factor) {
  if (!limiter || !client_id) return -1;

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (strcmp(limiter->clients[i].client_id, client_id) == 0) {
      limiter->clients[i].max_tokens =
          (uint64_t)(limiter->clients[i].max_tokens * increase_factor);
      return 0;
    }
  }

  return -1;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int rate_limiter_get_stats(rate_limiter_t *limiter,
                           rate_limiter_stats_t *stats) {
  if (!limiter || !stats) return -1;

  memset(stats, 0, sizeof(*stats));

  stats->total_clients = limiter->client_count;

  uint64_t total_requests = 0;
  uint64_t total_denied = 0;
  uint64_t total_tokens = 0;

  for (uint32_t i = 0; i < limiter->client_count; i++) {
    if (limiter->clients[i].is_blocked) {
      stats->blocked_clients++;
    } else {
      stats->active_clients++;
    }

    total_requests += limiter->clients[i].request_count;
    total_denied += limiter->clients[i].denied_count;
    total_tokens += limiter->clients[i].tokens;
  }

  stats->total_requests = total_requests;
  stats->denied_requests = total_denied;

  if (total_requests > 0) {
    stats->denial_rate = (double)total_denied / total_requests;
  }

  if (limiter->client_count > 0) {
    stats->mean_tokens_per_client = total_tokens / limiter->client_count;
  }

  return 0;
}

void rate_limiter_report(FILE *out, const rate_limiter_stats_t *stats) {
  if (!out || !stats) return;

  fprintf(out, "=== Rate Limiter Statistics ===\n");
  fprintf(out, "Total clients: %u\n", stats->total_clients);
  fprintf(out, "Active clients: %u\n", stats->active_clients);
  fprintf(out, "Blocked clients: %u\n", stats->blocked_clients);
  fprintf(out, "Total requests: %" PRIu64 "\n", stats->total_requests);
  fprintf(out, "Denied requests: %" PRIu64 "\n", stats->denied_requests);
  fprintf(out, "Denial rate: %.2f%%\n", stats->denial_rate * 100);
  fprintf(out, "Mean tokens per client: %" PRIu64 "\n",
          stats->mean_tokens_per_client);
}

void rate_limiter_free(rate_limiter_t *limiter) {
  if (!limiter) return;

  if (limiter->clients) {
    free(limiter->clients);
    limiter->clients = NULL;
  }

  memset(limiter, 0, sizeof(*limiter));
}

/* ============================================================================
 * Utility: Get current time in milliseconds
 * ============================================================================ */

static uint64_t get_rate_limiter_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
