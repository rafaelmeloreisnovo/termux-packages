"""Bounded tests for the source-built RAFCODEPHI compatibility aliases."""

import unittest

from scripts.rafcodephi_bootstrap_aliases import normalize_bootstrap_aliases


TARGETS = {"bin/bash", "libexec/termux-api-broadcast"}


class BootstrapAliasesTest(unittest.TestCase):
    def test_adds_only_verified_aliases(self):
        text, links = normalize_bootstrap_aliases(TARGETS, "busybox←bin/busybox\n")
        self.assertEqual(
            text,
            "busybox←bin/busybox\n"
            "bash←bin/sh\n"
            "termux-api-broadcast←libexec/termux-api\n",
        )
        self.assertIn("bin/sh", links)
        self.assertIn("libexec/termux-api", links)

    def test_idempotent_with_valid_upstream_links(self):
        original = "bash←bin/sh\ntermux-api-broadcast←libexec/termux-api\n"
        self.assertEqual(normalize_bootstrap_aliases(TARGETS, original)[0], original)

    def test_rejects_missing_real_executable(self):
        with self.assertRaisesRegex(ValueError, "missing source-built target"):
            normalize_bootstrap_aliases({"bin/bash"}, "")

    def test_rejects_wrong_target(self):
        with self.assertRaisesRegex(ValueError, "unexpected target"):
            normalize_bootstrap_aliases(TARGETS, "busybox←bin/sh\n")

    def test_rejects_conflicting_duplicates(self):
        with self.assertRaisesRegex(ValueError, "conflicting symlink"):
            normalize_bootstrap_aliases(TARGETS, "bash←bin/sh\nbusybox←bin/sh\n")

    def test_rejects_unsafe_destination(self):
        with self.assertRaisesRegex(ValueError, "unsafe symlink destination"):
            normalize_bootstrap_aliases(TARGETS, "bash←../../unsafe\n")

    def test_does_not_replace_real_archive_entry(self):
        text, links = normalize_bootstrap_aliases(TARGETS | {"bin/sh"}, "")
        self.assertNotIn("bin/sh", links)
        self.assertIn("termux-api-broadcast←libexec/termux-api\n", text)


if __name__ == "__main__":
    unittest.main()
