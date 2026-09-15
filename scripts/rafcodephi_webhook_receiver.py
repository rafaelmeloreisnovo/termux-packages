#!/usr/bin/env python3
"""Loopback HTTP relay. Put behind a TLS/rate-limiting proxy for external use."""
from __future__ import annotations

import argparse
from contextlib import closing
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import re
import sqlite3
import time
import urllib.error
import urllib.request

from validate_rafcodephi_webhook_event import (
    DEFAULT_POLICY, InvalidEvent, canonical, load_policy, require, strict_json, validate_event,
)


class DuplicateDelivery(Exception):
    pass


class DispatchUncertain(Exception):
    pass


def verify_signature(raw, signature, secret):
    require(isinstance(signature, str) and re.fullmatch(r"sha256=[0-9a-f]{64}", signature), "signature_format")
    expected = "sha256=" + hmac.new(secret, raw, hashlib.sha256).hexdigest()
    require(hmac.compare_digest(expected, signature), "signature_mismatch")


class ReplayLedger:
    """Atomic durable reservation before dispatch. Ambiguous attempts stay consumed.

    Deliveries and transitions are append-only. No TTL deletion: delayed redelivery,
    process restarts and clock rollback cannot erase a previously used identifier.
    A single SQLite database is required across all relay workers on one host.
    """

    def __init__(self, path):
        self.path = Path(path)
        require(str(path) != ":memory:", "persistent_ledger_required")
        self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        require(not self.path.is_symlink(), "ledger_symlink")
        if not self.path.exists():
            fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
            os.close(fd)
        require(self.path.is_file() and self.path.stat().st_mode & 0o077 == 0, "ledger_permissions")
        with closing(self.connect()) as db, db:
            db.execute("PRAGMA journal_mode=WAL")
            db.execute("CREATE TABLE IF NOT EXISTS deliveries (delivery_id TEXT PRIMARY KEY, body_sha256 TEXT NOT NULL UNIQUE, received_at INTEGER NOT NULL)")
            db.execute("CREATE TABLE IF NOT EXISTS transitions (id INTEGER PRIMARY KEY, delivery_id TEXT NOT NULL, state TEXT NOT NULL, observed_at INTEGER NOT NULL)")

    def connect(self):
        db = sqlite3.connect(self.path, timeout=2)
        db.execute("PRAGMA synchronous=FULL")
        return db

    def reserve(self, delivery_id, digest, now):
        try:
            with closing(self.connect()) as db, db:
                db.execute("BEGIN IMMEDIATE")
                db.execute("INSERT INTO deliveries VALUES (?, ?, ?)", (delivery_id, digest, now))
                db.execute("INSERT INTO transitions(delivery_id,state,observed_at) VALUES (?, 'RESERVED', ?)", (delivery_id, now))
        except sqlite3.IntegrityError as error:
            raise DuplicateDelivery from error

    def record(self, delivery_id, state):
        with closing(self.connect()) as db, db:
            db.execute("INSERT INTO transitions(delivery_id,state,observed_at) VALUES (?, ?, ?)",
                       (delivery_id, state, int(time.time())))


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise DispatchUncertain("redirect_refused")


def read_private_file(path):
    path = Path(path)
    require(not path.is_symlink() and path.is_file(), "private_file_type")
    require(path.stat().st_mode & 0o077 == 0, "private_file_permissions")
    value = path.read_bytes().rstrip(b"\r\n")
    require(32 <= len(value) <= 4096, "private_file_length")
    return value


class GitHubDispatcher:
    def __init__(self, repository, token_file):
        # The repository comes from validated local policy, never an incoming URL.
        require(repository == "rafaelmeloreisnovo/termux-packages", "dispatch_target")
        self.url = f"https://api.github.com/repos/{repository}/dispatches"
        self.token_file = token_file
        self.opener = urllib.request.build_opener(NoRedirect())

    def __call__(self, payload):
        try:
            token = read_private_file(self.token_file).decode("ascii")
            require(re.fullmatch(r"[A-Za-z0-9_]+", token), "token_format")
            request = urllib.request.Request(self.url, data=canonical(payload), method="POST", headers={
                "Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
                "Content-Type": "application/json", "User-Agent": "rafcodephi-webhook-relay/v1",
                "X-GitHub-Api-Version": "2026-03-10",
            })
            with self.opener.open(request, timeout=6) as response:
                if response.status != 204:
                    raise DispatchUncertain("non_204")
        except (OSError, ValueError, urllib.error.URLError) as error:
            # Do not echo token, exception, GitHub response body, or payload.
            raise DispatchUncertain("dispatch_failed_or_unknown") from error


class Relay:
    def __init__(self, policy, secret, ledger, dispatch, clock=time.time):
        require(isinstance(secret, bytes) and len(secret) >= 32, "secret_too_short")
        self.policy, self.secret, self.ledger = policy, secret, ledger
        self.dispatch, self.clock = dispatch, clock

    def accept(self, raw, signature, delivery_header, event_header):
        require(0 < len(raw) <= self.policy["max_body_bytes"], "body_size")
        verify_signature(raw, signature, self.secret)
        now = int(self.clock())
        event = validate_event(strict_json(raw, self.policy["max_body_bytes"]), self.policy, now)
        # GitHub-style headers are not covered by HMAC. Bind them to signed fields.
        require(event["delivery_id"] == delivery_header, "delivery_header_mismatch")
        require(event["event_type"] == event_header, "event_header_mismatch")
        digest = hashlib.sha256(raw).hexdigest()
        self.ledger.reserve(event["delivery_id"], digest, now)
        payload = {"event_type": self.policy["event_type"], "client_payload": {
            "candidate": event, "relay": {"raw_body_sha256": digest, "received_at": now}}}
        try:
            self.dispatch(payload)
        except Exception as error:
            self.ledger.record(event["delivery_id"], "DISPATCH_UNCERTAIN")
            raise DispatchUncertain("dispatch_failed_or_unknown") from error
        self.ledger.record(event["delivery_id"], "DISPATCH_ACCEPTED")
        return {"status": "DISPATCH_ACCEPTED", "delivery_id": event["delivery_id"],
                "raw_body_sha256": digest, "claim_allowed": False}


def handler_for(relay):
    class Handler(BaseHTTPRequestHandler):
        server_version = "RAFCODEPHI-Relay/1"
        sys_version = ""

        def setup(self):
            super().setup()
            self.connection.settimeout(5)

        def log_message(self, *_args):
            pass  # Do not log raw URL, headers or bodies.

        def reply(self, status, value):
            body = canonical(value) + b"\n"
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            self.close_connection = True

        def single(self, key):
            values = self.headers.get_all(key, [])
            require(len(values) == 1, "required_single_header")
            return values[0]

        def do_POST(self):
            try:
                require(self.path == "/webhook", "path")
                require(not self.headers.get_all("Transfer-Encoding") and not self.headers.get_all("Content-Encoding"), "body_encoding")
                require(self.single("Content-Type").lower() == "application/json", "content_type")
                length = self.single("Content-Length")
                require(re.fullmatch(r"[0-9]{1,5}", length), "content_length")
                require(0 < int(length) <= relay.policy["max_body_bytes"], "body_size")
                signature = self.single("X-Hub-Signature-256")
                delivery = self.single("X-GitHub-Delivery")
                event = self.single("X-GitHub-Event")
                body = self.rfile.read(int(length))
                require(len(body) == int(length), "body_truncated")
                result = relay.accept(body, signature, delivery, event)
                self.reply(202, result)
            except InvalidEvent:
                self.reply(400, {"status": "REJECTED", "claim_allowed": False})
            except DuplicateDelivery:
                self.reply(409, {"status": "DELIVERY_ALREADY_CONSUMED", "claim_allowed": False})
            except DispatchUncertain:
                self.reply(502, {"status": "DISPATCH_UNCERTAIN_DO_NOT_RETRY_AUTOMATICALLY", "claim_allowed": False})
            except (OSError, sqlite3.Error):
                self.reply(503, {"status": "RELAY_UNAVAILABLE", "claim_allowed": False})

        def do_GET(self):
            self.reply(405, {"status": "METHOD_NOT_ALLOWED"})

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--secret-file", type=Path, required=True)
    parser.add_argument("--token-file", type=Path, required=True)
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()
    try:
        os.umask(0o077)
        policy = load_policy(args.policy)
        secret = read_private_file(args.secret_file)
        read_private_file(args.token_file)  # Validate availability before serving.
        relay = Relay(policy, secret, ReplayLedger(args.ledger), GitHubDispatcher(policy["target_repository"], args.token_file))
        with HTTPServer(("127.0.0.1", args.port), handler_for(relay)) as server:
            print("RELAY_LISTEN=127.0.0.1 TLS=EXTERNAL_PROXY_REQUIRED claim_allowed=false", flush=True)
            server.serve_forever()
    except (InvalidEvent, OSError, sqlite3.Error):
        print("RELAY_START=BLOCKED configuration_or_storage_failure", flush=True)
        return 1
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
