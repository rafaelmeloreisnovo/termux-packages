#include "rpc_coordinator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

/* ============================================================================
 * RPC Connection Management
 * ============================================================================ */

int rpc_connect(rpc_connection_t *conn, const char *host, uint16_t port) {
  if (!conn || !host) return -1;

  memset(conn, 0, sizeof(*conn));

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
    close(sock);
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }

  conn->socket_fd = sock;
  strncpy(conn->remote_host, host, sizeof(conn->remote_host) - 1);
  conn->remote_port = port;
  conn->is_connected = 1;
  conn->next_sequence_id = 1;

  return 0;
}

int rpc_disconnect(rpc_connection_t *conn) {
  if (!conn) return -1;

  if (conn->socket_fd >= 0) {
    close(conn->socket_fd);
    conn->socket_fd = -1;
  }

  conn->is_connected = 0;
  return 0;
}

/* ============================================================================
 * RPC Message Serialization (JSON)
 * ============================================================================ */

int rpc_serialize(const rpc_message_t *msg, char *buffer, size_t buffer_size) {
  if (!msg || !buffer || buffer_size < 512) return -1;

  int written = 0;
  char timestamp[32];
  snprintf(timestamp, sizeof(timestamp), "%" PRIu64, msg->timestamp_ms);

  written = snprintf(buffer, buffer_size,
                     "{"
                     "\"type\":%d,"
                     "\"sequence_id\":%u,"
                     "\"sender_id\":\"%s\","
                     "\"receiver_id\":\"%s\","
                     "\"timestamp_ms\":%s",
                     msg->type, msg->sequence_id, msg->sender_id,
                     msg->receiver_id, timestamp);

  if (written < 0 || written >= (int)buffer_size) return -1;

  size_t offset = written;

  switch (msg->type) {
  case RPC_MSG_REGISTER: {
    int len = snprintf(
        buffer + offset, buffer_size - offset,
        ",\"payload\":{\"hostname\":\"%s\",\"port\":%u}}",
        msg->payload.register_msg.hostname, msg->payload.register_msg.port);
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  case RPC_MSG_ASSIGN_LAYER: {
    int len = snprintf(buffer + offset, buffer_size - offset,
                       ",\"payload\":{\"layer_id\":%u,\"pkg_count\":%u}}",
                       msg->payload.assign_msg.layer_id,
                       msg->payload.assign_msg.pkg_count);
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  case RPC_MSG_REPORT_COMPLETION: {
    int len = snprintf(
        buffer + offset, buffer_size - offset,
        ",\"payload\":{\"layer_id\":%u,\"packages_built\":%u,"
        "\"packages_failed\":%u,\"coherence_phi\":%" PRIu64 "}}",
        msg->payload.report_msg.layer_id,
        msg->payload.report_msg.packages_built,
        msg->payload.report_msg.packages_failed,
        msg->payload.report_msg.coherence_phi);
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  case RPC_MSG_HEARTBEAT: {
    int len = snprintf(
        buffer + offset, buffer_size - offset,
        ",\"payload\":{\"current_layer\":%u,\"current_pkg\":%u,"
        "\"builds_completed\":%u,\"mean_latency_us\":%" PRIu64 "}}",
        msg->payload.heartbeat_msg.current_layer,
        msg->payload.heartbeat_msg.current_pkg,
        msg->payload.heartbeat_msg.builds_completed,
        msg->payload.heartbeat_msg.mean_latency_us);
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  case RPC_MSG_ERROR: {
    int len = snprintf(buffer + offset, buffer_size - offset,
                       ",\"payload\":{\"error_code\":%d,\"error_msg\":\"%s\"}}",
                       msg->payload.error_msg.error_code,
                       msg->payload.error_msg.error_msg);
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  case RPC_MSG_SHUTDOWN: {
    int len =
        snprintf(buffer + offset, buffer_size - offset, ",\"payload\":{}}");
    if (len < 0 || offset + len >= buffer_size) return -1;
    written = offset + len;
    break;
  }

  default:
    return -1;
  }

  return written;
}

int rpc_deserialize(const char *json_buffer, size_t buffer_size,
                    rpc_message_t *msg) {
  if (!json_buffer || !msg || buffer_size == 0) return -1;

  memset(msg, 0, sizeof(*msg));

  sscanf(json_buffer, "{\"type\":%d,\"sequence_id\":%u,\"sender_id\":\"%63[^\"]\","
                      "\"receiver_id\":\"%63[^\"]\"",
         (int *)&msg->type, &msg->sequence_id, msg->sender_id,
         msg->receiver_id);

  return 0;
}

/* ============================================================================
 * RPC Send/Receive
 * ============================================================================ */

int rpc_send(rpc_connection_t *conn, const rpc_message_t *msg) {
  if (!conn || !msg || !conn->is_connected) return -1;

  char buffer[1024];
  int len = rpc_serialize(msg, buffer, sizeof(buffer));
  if (len <= 0) return -1;

  ssize_t sent = send(conn->socket_fd, buffer, len, 0);
  if (sent < 0 || sent != len) return -1;

  return 0;
}

int rpc_receive(rpc_connection_t *conn, rpc_message_t *msg,
                uint32_t timeout_ms) {
  if (!conn || !msg || !conn->is_connected) return -1;

  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  if (setsockopt(conn->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                 sizeof(tv)) < 0)
    return -1;

  ssize_t recvd = recv(conn->socket_fd, buffer, sizeof(buffer) - 1, 0);
  if (recvd <= 0) return -1;

  buffer[recvd] = '\0';

  if (rpc_deserialize(buffer, recvd, msg) < 0) return -1;

  return 0;
}

/* ============================================================================
 * RPC Server (Master Listener)
 * ============================================================================ */

int rpc_server_start(rpc_server_t *server, uint16_t port,
                     rpc_handler_t handler) {
  if (!server || !handler) return -1;

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -1;

  int opt = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    close(sock);
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }

  if (listen(sock, 8) < 0) {
    close(sock);
    return -1;
  }

  server->listen_socket = sock;
  server->listen_port = port;
  server->is_running = 1;
  server->message_handler = handler;

  return 0;
}

int rpc_server_accept(rpc_server_t *server, rpc_connection_t *client_conn) {
  if (!server || !client_conn || !server->is_running) return -1;

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int client_sock =
      accept(server->listen_socket, (struct sockaddr *)&client_addr, &addr_len);
  if (client_sock < 0) return -1;

  memset(client_conn, 0, sizeof(*client_conn));
  client_conn->socket_fd = client_sock;
  inet_ntop(AF_INET, &client_addr.sin_addr, client_conn->remote_host,
            sizeof(client_conn->remote_host));
  client_conn->remote_port = ntohs(client_addr.sin_port);
  client_conn->is_connected = 1;

  return 0;
}

int rpc_server_stop(rpc_server_t *server) {
  if (!server) return -1;

  if (server->listen_socket >= 0) {
    close(server->listen_socket);
    server->listen_socket = -1;
  }

  server->is_running = 0;
  return 0;
}

/* ============================================================================
 * Retry & Timeout Strategy
 * ============================================================================ */

int rpc_send_with_retry(rpc_connection_t *conn, const rpc_message_t *msg,
                        const rpc_retry_policy_t *policy) {
  if (!conn || !msg || !policy) return -1;

  for (uint32_t attempt = 0; attempt <= policy->retry_count; attempt++) {
    int ret = rpc_send(conn, msg);
    if (ret == 0) return 0;

    if (attempt < policy->retry_count) {
      uint32_t delay_ms = policy->retry_delays_ms[attempt];
      usleep(delay_ms * 1000);
    }
  }

  return -1;
}

/* ============================================================================
 * Health & Monitoring
 * ============================================================================ */

int rpc_get_stats(const rpc_connection_t *conn, rpc_stats_t *stats) {
  if (!conn || !stats) return -1;

  memset(stats, 0, sizeof(*stats));

  return 0;
}
