#!/usr/bin/env python3
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

class ArchSafetyFollowupsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ws_h = (ROOT / "core/work_stealing.h").read_text(encoding="utf-8")
        cls.ws_c = (ROOT / "core/work_stealing.c").read_text(encoding="utf-8")
        cls.uo_h = (ROOT / "core/unified_orchestrator.h").read_text(encoding="utf-8")
        cls.perf_c = (ROOT / "core/perf_profiler.c").read_text(encoding="utf-8")

    def test_u64_counters_are_width_correct_atomics(self):
        self.assertRegex(self.ws_h, r"_Atomic\s+uint64_t\s+total_steals\s*;")
        self.assertRegex(self.ws_h, r"_Atomic\s+uint64_t\s+total_items_processed\s*;")
        self.assertNotIn("atomic_ulong *", self.ws_c)
        self.assertNotIn("(atomic_ulong *)&scheduler->total_steals", self.ws_c)
        self.assertNotIn("(atomic_ulong *)&scheduler->total_items_processed", self.ws_c)
        self.assertIn("atomic_fetch_add(&scheduler->total_steals", self.ws_c)

    def test_phase_storage_covers_all_nine_phases(self):
        match = re.search(r"#define\s+UNIFIED_PHASE_COUNT\s+(\d+)", self.uo_h)
        self.assertIsNotNone(match)
        self.assertEqual(int(match.group(1)), 9)
        self.assertIn("wall_time_phases[UNIFIED_PHASE_COUNT]", self.uo_h)
        self.assertNotIn("wall_time_phases[8]", self.uo_h)

    def test_profiler_uses_numeric_u64_comparator(self):
        self.assertRegex(self.perf_c, r"static\s+int\s+compare_u64\s*\(")
        self.assertIn("return (a > b) - (a < b);", self.perf_c)
        self.assertRegex(
            self.perf_c,
            r"qsort\s*\(times,\s*profile\.sample_count,\s*sizeof\(uint64_t\),\s*compare_u64\s*\)",
        )
        self.assertNotRegex(self.perf_c, r"qsort[\s\S]{0,200}strcmp")

if __name__ == "__main__":
    unittest.main()
