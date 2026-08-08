#ifndef RPC_COORDINATOR_H
#define RPC_COORDINATOR_H

/*
 * Phase 9.20: RPC Coordinator
 * Simple JSON over TCP protocol for master/worker communication
 * No gRPC dependency - just TCP sockets and JSON parsing
 */

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

/* ============================================================================
 * RPC Message Types
 * ============================================================================ */

typedef enum {
  RPC_MSG_REGISTER,           /* Worker registers with master */
  RPC_MSG_ASSIGN_LAYER,       /* Master assigns layer to worker */
  RPC_MSG_REPORT_COMPLETION,  /* Worker reports layer completion */
  RPC_MSG_HEARTBEAT,          /* Worker heartbeat */
  RPC_MSG_HEARTBEAT_ACK,      /* Master acknowledges heartbeat */
  RPC_MSG_ERROR,              /* Error response */
  RPC_MSG_SHUTDOWN            /* Shutdown signal */
} rpc_msg_type_t;

/* ============================================================================
 * RPC Message Structure (JSON representation)
 * ============================================================================ */

typedef struct {
  rpc_msg_type_t type;
  uint32_t sequence_id;
  char sender_id[64];
  char receiver_id[64];
  uint64_t timestamp_ms;

  /* Payload (type-specific) */
  union {
    struct {  /* REGISTER */
      char hostname[64];
      uint16_t port;
    } register_msg;

    struct {  /* ASSIGN_LAYER */
      uint32_t layer_id;
      uint32_t pkg_count;
      uint32_t *pkg_indices;
    } assign_msg;

    struct {  /* REPORT_COMPLETION */
      uint32_t layer_id;
      uint32_t packages_built;
      uint32_t packages_failed;
      uint64_t coherence_phi;
    } report_msg;

    struct {  /* HEARTBEAT */
      uint32_t current_layer;
      uint32_t current_pkg;
      uint32_t builds_completed;
      uint64_t mean_latency_us;
    } heartbeat_msg;

    struct {  /* ERROR */
      int error_code;
      char error_msg[256];
    } error_msg;
  } payload;
} rpc_message_t;

/* ============================================================================
 * RPC Connection Management
 * ============================================================================ */

typedef struct {
  int socket_fd;
  char remote_host[64];
  uint16_t remote_port;
  uint8_t is_connected;
  uint32_t next_sequence_id;
} rpc_connection_t;

/* Connect to remote RPC server */
int rpc_connect(rpc_connection_t *conn, const char *host, uint16_t port);

/* Disconnect from RPC server */
int rpc_disconnect(rpc_connection_t *conn);

/* ============================================================================
 * RPC Message Serialization (JSON)
 * ============================================================================ */

/* Serialize RPC message to JSON */
int rpc_serialize(const rpc_message_t *msg, char *buffer, size_t buffer_size);

/* Deserialize JSON to RPC message */
int rpc_deserialize(const char *json_buffer, size_t buffer_size, rpc_message_t *msg);

/* ============================================================================
 * RPC Send/Receive
 * ============================================================================ */

/* Send RPC message */
int rpc_send(rpc_connection_t *conn, const rpc_message_t *msg);

/* Receive RPC message (blocking) */
int rpc_receive(rpc_connection_t *conn, rpc_message_t *msg, uint32_t timeout_ms);

/* ============================================================================
 * RPC Server (Master Listener)
 * ============================================================================ */

typedef int (*rpc_handler_t)(const rpc_message_t *request, rpc_message_t *response);

typedef struct {
  int listen_socket;
  uint16_t listen_port;
  uint8_t is_running;
  rpc_handler_t message_handler;
} rpc_server_t;

/* Start RPC server (listen for connections) */
int rpc_server_start(rpc_server_t *server, uint16_t port, rpc_handler_t handler);

/* Accept incoming connection */
int rpc_server_accept(rpc_server_t *server, rpc_connection_t *client_conn);

/* Stop RPC server */
int rpc_server_stop(rpc_server_t *server);

/* ============================================================================
 * Retry & Timeout Strategy
 * ============================================================================ */

typedef struct {
  uint32_t retry_count;
  uint32_t retry_delays_ms[4];  /* Exponential backoff: 2s, 4s, 8s, 16s */
  uint32_t connect_timeout_ms;
  uint32_t send_timeout_ms;
  uint32_t recv_timeout_ms;
} rpc_retry_policy_t;

/* Send with retry policy */
int rpc_send_with_retry(rpc_connection_t *conn, const rpc_message_t *msg,
                        const rpc_retry_policy_t *policy);

/* ============================================================================
 * Health & Monitoring
 * ============================================================================ */

typedef struct {
  uint32_t messages_sent;
  uint32_t messages_received;
  uint32_t messages_failed;
  uint64_t total_bytes_sent;
  uint64_t total_bytes_received;
  double avg_roundtrip_ms;
} rpc_stats_t;

/* Get RPC statistics */
int rpc_get_stats(const rpc_connection_t *conn, rpc_stats_t *stats);

#endif  /* RPC_COORDINATOR_H */
