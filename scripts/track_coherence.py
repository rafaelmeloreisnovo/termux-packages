#!/usr/bin/env python3
"""
Phase 9.16: Coherence Metric Tracking

Extracts coherence (Φ) metrics from Phase 9.15 production hardening tests
and tracks trends across commits for regression detection.

Φ = (1 - overhead_ratio) × (1 - latency_ratio) × (1 - cache_miss_ratio)

Target: Φ ≥ 0.85 for production readiness
"""

import re
import json
import sys
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional


class CoherenceTracker:
    """Track and analyze coherence metrics across test runs."""

    def __init__(self, metrics_file: str = ".coherence_history.json"):
        self.metrics_file = Path(metrics_file)
        self.history: List[Dict] = self._load_history()

    def _load_history(self) -> List[Dict]:
        """Load coherence history from file."""
        if self.metrics_file.exists():
            try:
                with open(self.metrics_file, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return []
        return []

    def _save_history(self) -> None:
        """Save coherence history to file."""
        with open(self.metrics_file, 'w') as f:
            json.dump(self.history, f, indent=2)

    def parse_test_output(self, output: str) -> Optional[Dict]:
        """Parse test output and extract coherence metrics."""
        metrics = {
            'timestamp': datetime.utcnow().isoformat(),
            'tests_passed': 0,
            'tests_total': 0,
            'test_details': {},
            'coherence_phi': 0.0,
            'checkpoint_status': False,
            'cache_status': False,
            'error_recovery_status': False,
        }

        # Extract test results
        lines = output.split('\n')

        # Parse individual test results
        test_patterns = {
            'checkpoint': (r'Test 1:.*Checkpoint', r'Coherence φ: ([\d.]+)'),
            'resume': (r'Test 2:.*Checkpoint Resume', r'Coherence φ: ([\d.]+)'),
            'partial_build': (r'Test 3:.*Partial Build', r'✓ Partial build'),
            'error_recovery': (r'Test 4:.*Error Recovery', r'✓ Error recovery'),
            'cache': (r'Test 5:.*Cache', r'✓ Cache'),
            'parallel': (r'Test 6:.*Parallel', r'✓ Parallel'),
        }

        for test_name, (pattern, search_pattern) in test_patterns.items():
            if re.search(pattern, output):
                metrics['test_details'][test_name] = 'PASS'

        # Extract overall test summary
        summary_match = re.search(r'Passed: (\d+)/(\d+)', output)
        if summary_match:
            metrics['tests_passed'] = int(summary_match.group(1))
            metrics['tests_total'] = int(summary_match.group(2))

        # Extract coherence φ (from Test 1 checkpoint result)
        coherence_matches = re.findall(r'Coherence φ: ([\d.]+)', output)
        if coherence_matches:
            # Use average of coherence values found (typically from reports)
            metrics['coherence_phi'] = float(coherence_matches[0])

        # Check for capability activation
        metrics['checkpoint_status'] = 'LATENTE: Checkpoint & Resume' in output
        metrics['cache_status'] = 'LATENTE: Incremental Builds' in output
        metrics['error_recovery_status'] = 'URGENTE: Error Recovery' in output

        return metrics

    def add_metrics(self, metrics: Dict, commit: str = "unknown",
                    branch: str = "unknown") -> None:
        """Add new metrics to history."""
        metrics['commit'] = commit
        metrics['branch'] = branch
        self.history.append(metrics)
        self._save_history()

    def get_trend(self, window: int = 10) -> Dict:
        """Get coherence trend over last N commits."""
        recent = self.history[-window:] if len(self.history) > 0 else []

        if not recent:
            return {
                'samples': 0,
                'mean_phi': 0.0,
                'min_phi': 0.0,
                'max_phi': 0.0,
                'trend': 'UNKNOWN',
                'degradation_pct': 0.0,
            }

        phi_values = [m['coherence_phi'] for m in recent]
        mean_phi = sum(phi_values) / len(phi_values)

        # Calculate trend (improvement/degradation)
        if len(phi_values) > 1:
            first_phi = phi_values[0]
            last_phi = phi_values[-1]
            degradation = ((first_phi - last_phi) / first_phi) * 100
        else:
            degradation = 0.0

        trend = 'IMPROVING' if degradation < -2 else (
            'DEGRADING' if degradation > 2 else 'STABLE'
        )

        return {
            'samples': len(recent),
            'mean_phi': mean_phi,
            'min_phi': min(phi_values),
            'max_phi': max(phi_values),
            'trend': trend,
            'degradation_pct': degradation,
        }

    def check_regression(self, current: Dict) -> Dict[str, bool]:
        """Check if current metrics indicate regression."""
        alerts = {
            'phi_below_target': current['coherence_phi'] < 0.85,
            'phi_below_minimum': current['coherence_phi'] < 0.80,
            'tests_failed': current['tests_passed'] != current['tests_total'],
            'capabilities_missing': not (
                current['checkpoint_status'] and
                current['cache_status'] and
                current['error_recovery_status']
            ),
        }

        # Check trend-based regression
        if len(self.history) > 1:
            trend = self.get_trend(min(10, len(self.history)))
            alerts['trend_degrading'] = trend['trend'] == 'DEGRADING'
            alerts['significant_drop'] = trend['degradation_pct'] > 5.0
        else:
            alerts['trend_degrading'] = False
            alerts['significant_drop'] = False

        return alerts

    def generate_report(self, current: Dict) -> str:
        """Generate human-readable coherence report."""
        report = []
        report.append("=" * 60)
        report.append("Phase 9.16 Coherence Tracking Report")
        report.append("=" * 60)
        report.append("")

        # Current metrics
        report.append("Current Metrics:")
        report.append(f"  Coherence φ: {current['coherence_phi']:.4f}")
        report.append(f"  Tests: {current['tests_passed']}/{current['tests_total']}")
        report.append(f"  Timestamp: {current['timestamp']}")
        report.append("")

        # Trend analysis
        trend = self.get_trend()
        report.append("Trend Analysis (last 10 runs):")
        report.append(f"  Mean φ: {trend['mean_phi']:.4f}")
        report.append(f"  Min φ: {trend['min_phi']:.4f}")
        report.append(f"  Max φ: {trend['max_phi']:.4f}")
        report.append(f"  Trend: {trend['trend']}")
        if trend['degradation_pct'] != 0.0:
            direction = "↓" if trend['degradation_pct'] > 0 else "↑"
            report.append(f"  Change: {direction} {abs(trend['degradation_pct']):.1f}%")
        report.append("")

        # Status checks
        report.append("Status Checks:")
        status_ok = current['coherence_phi'] >= 0.85
        report.append(f"  Coherence Target (≥ 0.85): {'✓' if status_ok else '✗'}")

        all_tests_pass = current['tests_passed'] == current['tests_total']
        report.append(f"  All Tests Passed: {'✓' if all_tests_pass else '✗'}")

        capabilities_ok = (
            current['checkpoint_status'] and
            current['cache_status'] and
            current['error_recovery_status']
        )
        report.append(f"  Capabilities Activated: {'✓' if capabilities_ok else '✗'}")
        report.append("")

        # Regression analysis
        alerts = self.check_regression(current)
        if any(alerts.values()):
            report.append("⚠ Regression Alerts:")
            if alerts['phi_below_target']:
                report.append("  - Coherence below target (0.85)")
            if alerts['phi_below_minimum']:
                report.append("  - Coherence below minimum (0.80)")
            if alerts['tests_failed']:
                report.append("  - Some tests failed")
            if alerts['capabilities_missing']:
                report.append("  - Some capabilities not activated")
            if alerts['trend_degrading']:
                report.append("  - Trend is degrading")
            if alerts['significant_drop']:
                report.append("  - Significant performance drop detected")
        else:
            report.append("✓ No regressions detected")

        report.append("")
        report.append("=" * 60)

        return '\n'.join(report)


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: track_coherence.py <test_output_file> [commit] [branch]")
        sys.exit(1)

    test_file = sys.argv[1]
    commit = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    branch = sys.argv[3] if len(sys.argv) > 3 else "unknown"

    # Read test output
    try:
        with open(test_file, 'r') as f:
            output = f.read()
    except FileNotFoundError:
        print(f"Error: Test output file not found: {test_file}")
        sys.exit(1)

    # Parse and track
    tracker = CoherenceTracker()
    metrics = tracker.parse_test_output(output)

    if metrics is None:
        print("Error: Could not parse test output")
        sys.exit(1)

    # Add to history
    tracker.add_metrics(metrics, commit, branch)

    # Generate and print report
    report = tracker.generate_report(metrics)
    print(report)

    # Check for regressions
    alerts = tracker.check_regression(metrics)
    if alerts['phi_below_minimum'] or alerts['tests_failed']:
        print("\n✗ CRITICAL: Regression detected")
        sys.exit(1)
    elif any(alerts.values()):
        print("\n⚠ WARNING: Alerts detected")
        sys.exit(0)

    print("\n✓ All checks passed")
    sys.exit(0)


if __name__ == '__main__':
    main()
