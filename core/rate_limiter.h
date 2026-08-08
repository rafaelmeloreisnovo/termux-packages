#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

/*
 * Phase 9.21: Rate Limiting
 * Token bucket algorithm with per-client enforcement
 * Prevents DoS, brute-force, and abuse
 */

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>

/* ============================================================================
 * Rate Limiter Structures
 * ============================================================================ */

typedef struct {
  char client_id[64];        /* Unique client identifier */
  uint64_t tokens;           /* Current token count */
  uint64_t max_tokens;       /* Capacity (tokens per second × window) */
  uint64_t refill_rate;      /* Tokens per second */
  uint64_t last_refill_ms;   /* Last refill timestamp */
  uint32_t request_count;    /* Total requests from this client */
  uint32_t denied_count;     /* Requests denied (rate limit exceeded) */
  uint8_t is_blocked;        /* Temporary block flag */
} rate_limiter_client_t;

typedef struct {
  rate_limiter_client_t *clients;
  uint32_t client_count;
  uint32_t max_clients;
  uint64_t global_refill_rate;  /* Tokens/sec for all clients */
  uint32_t block_duration_ms;   /* How long to block violators */
} rate_limiter_t;

/* ============================================================================
 * Rate Limiter Operations
 * ============================================================================ */

/* Initialize rate limiter */
int rate_limiter_init(rate_limiter_t *limiter, uint32_t max_clients,
                      uint64_t global_refill_rate);

/* Check if request is allowed (consumes 1 token) */
int rate_limiter_allow(rate_limiter_t *limiter, const char *client_id,
                       uint32_t tokens_required);

/* Get current token count for client */
uint64_t rate_limiter_get_tokens(rate_limiter_t *limiter,
                                 const char *client_id);

/* Refill tokens based on elapsed time */
int rate_limiter_refill(rate_limiter_t *limiter, const char *client_id);

/* Block client temporarily */
int rate_limiter_block_client(rate_limiter_t *limiter, const char *client_id);

/* Unblock client */
int rate_limiter_unblock_client(rate_limiter_t *limiter, const char *client_id);

/* ============================================================================
 * Adaptive Rate Limiting
 * ============================================================================ */

/* Reduce rate limit if error rate > threshold */
int rate_limiter_reduce_limit(rate_limiter_t *limiter, const char *client_id,
                              double reduction_factor);

/* Increase rate limit if client behaves well */
int rate_limiter_increase_limit(rate_limiter_t *limiter, const char *client_id,
                                double increase_factor);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

typedef struct {
  uint32_t total_clients;
  uint32_t active_clients;
  uint32_t blocked_clients;
  uint64_t total_requests;
  uint64_t denied_requests;
  double denial_rate;
  uint64_t mean_tokens_per_client;
} rate_limiter_stats_t;

/* Get rate limiter statistics */
int rate_limiter_get_stats(rate_limiter_t *limiter,
                           rate_limiter_stats_t *stats);

/* Report rate limiter status */
void rate_limiter_report(FILE *out, const rate_limiter_stats_t *stats);

/* Free rate limiter */
void rate_limiter_free(rate_limiter_t *limiter);

#endif /* RATE_LIMITER_H */
