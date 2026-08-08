#!/usr/bin/env python3
"""
Phase 9.16: Performance Regression Detection

Monitors key performance indicators (latency, overhead, cache efficiency)
and alerts when thresholds are exceeded or trends degrade.

Tracked Metrics:
  - Per-package latency (target: < 10 seconds)
  - Heap overhead (target: < 5%)
  - L1D cache miss rate (target: < 3%)
  - Total build time (target: < 90 minutes for 2057 packages)
  - Coherence metric Φ (target: ≥ 0.85)
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass


@dataclass
class PerformanceThresholds:
    """Performance regression thresholds."""
    coherence_phi_min: float = 0.80      # Minimum acceptable
    coherence_phi_target: float = 0.85   # Target value
    latency_max_per_pkg: float = 10.0    # Seconds
    overhead_heap_max: float = 0.05      # 5%
    cache_miss_rate_max: float = 0.03    # 3%
    build_time_max: float = 5400.0       # 90 minutes in seconds
    regression_threshold: float = 0.05   # 5% degradation


class RegressionDetector:
    """Detect performance regressions in build system."""

    def __init__(self, thresholds: Optional[PerformanceThresholds] = None,
                 history_file: str = ".performance_history.json"):
        self.thresholds = thresholds or PerformanceThresholds()
        self.history_file = Path(history_file)
        self.history = self._load_history()

    def _load_history(self) -> List[Dict]:
        """Load performance history."""
        if self.history_file.exists():
            try:
                with open(self.history_file, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return []
        return []

    def _save_history(self) -> None:
        """Save performance history."""
        with open(self.history_file, 'w') as f:
            json.dump(self.history, f, indent=2)

    def detect_latency_regression(self, current_latency: float) -> Dict[str, bool]:
        """Detect latency regressions."""
        alerts = {
            'exceeds_threshold': current_latency > self.thresholds.latency_max_per_pkg,
            'degraded_from_baseline': False,
            'significant_spike': False,
        }

        if len(self.history) > 0:
            recent = self.history[-5:]  # Last 5 runs
            baseline_latency = sum(h.get('avg_latency', 0) for h in recent) / len(recent)

            if baseline_latency > 0:
                degradation = (current_latency - baseline_latency) / baseline_latency
                alerts['degraded_from_baseline'] = degradation > self.thresholds.regression_threshold
                alerts['significant_spike'] = degradation > 0.20  # 20% spike

        return alerts

    def detect_overhead_regression(self, current_overhead: float) -> Dict[str, bool]:
        """Detect heap overhead regressions."""
        alerts = {
            'exceeds_threshold': current_overhead > self.thresholds.overhead_heap_max,
            'degraded_from_baseline': False,
        }

        if len(self.history) > 0:
            recent = self.history[-5:]
            baseline_overhead = sum(h.get('heap_overhead', 0) for h in recent) / len(recent)

            if baseline_overhead > 0:
                degradation = (current_overhead - baseline_overhead) / baseline_overhead
                alerts['degraded_from_baseline'] = degradation > self.thresholds.regression_threshold

        return alerts

    def detect_cache_regression(self, current_miss_rate: float) -> Dict[str, bool]:
        """Detect cache efficiency regressions."""
        alerts = {
            'exceeds_threshold': current_miss_rate > self.thresholds.cache_miss_rate_max,
            'degraded_from_baseline': False,
        }

        if len(self.history) > 0:
            recent = self.history[-5:]
            baseline_miss_rate = sum(h.get('l1_miss_rate', 0) for h in recent) / len(recent)

            if baseline_miss_rate > 0:
                degradation = (current_miss_rate - baseline_miss_rate) / baseline_miss_rate
                alerts['degraded_from_baseline'] = degradation > self.thresholds.regression_threshold

        return alerts

    def detect_coherence_regression(self, current_phi: float) -> Dict[str, bool]:
        """Detect coherence metric regressions."""
        alerts = {
            'below_minimum': current_phi < self.thresholds.coherence_phi_min,
            'below_target': current_phi < self.thresholds.coherence_phi_target,
            'degraded_from_baseline': False,
        }

        if len(self.history) > 0:
            recent = self.history[-5:]
            baseline_phi = sum(h.get('coherence_phi', 0) for h in recent) / len(recent)

            if baseline_phi > 0:
                degradation_pct = ((baseline_phi - current_phi) / baseline_phi) * 100
                alerts['degraded_from_baseline'] = degradation_pct > 2.0  # 2% degradation

        return alerts

    def analyze_all_metrics(self, metrics: Dict) -> Dict[str, any]:
        """Analyze all performance metrics."""
        analysis = {
            'timestamp': metrics.get('timestamp'),
            'latency_alerts': self.detect_latency_regression(
                metrics.get('avg_latency', 0)
            ),
            'overhead_alerts': self.detect_overhead_regression(
                metrics.get('heap_overhead', 0)
            ),
            'cache_alerts': self.detect_cache_regression(
                metrics.get('l1_miss_rate', 0)
            ),
            'coherence_alerts': self.detect_coherence_regression(
                metrics.get('coherence_phi', 0)
            ),
            'overall_status': 'PASS',
        }

        # Determine overall status
        all_alerts = (
            analysis['latency_alerts'].values() |
            analysis['overhead_alerts'].values() |
            analysis['cache_alerts'].values() |
            analysis['coherence_alerts'].values()
        )

        if any(all_alerts):
            critical_alerts = [
                analysis['coherence_alerts'].get('below_minimum', False),
                analysis['latency_alerts'].get('significant_spike', False),
                analysis['overhead_alerts'].get('exceeds_threshold', False),
            ]
            analysis['overall_status'] = 'CRITICAL' if any(critical_alerts) else 'WARN'

        return analysis

    def generate_alert_report(self, analysis: Dict) -> str:
        """Generate regression alert report."""
        report = []
        report.append("=" * 70)
        report.append("Performance Regression Detection Report")
        report.append("=" * 70)
        report.append("")

        # Status summary
        status = analysis['overall_status']
        status_symbol = '✗' if status == 'CRITICAL' else ('⚠' if status == 'WARN' else '✓')
        report.append(f"Overall Status: {status_symbol} {status}")
        report.append("")

        # Coherence alerts
        coh_alerts = analysis['coherence_alerts']
        if coh_alerts.get('below_minimum'):
            report.append("✗ CRITICAL: Coherence below minimum (0.80)")
        elif coh_alerts.get('below_target'):
            report.append("⚠ WARNING: Coherence below target (0.85)")
        elif coh_alerts.get('degraded_from_baseline'):
            report.append("⚠ WARNING: Coherence degraded from baseline")
        else:
            report.append("✓ Coherence metric acceptable")

        report.append("")

        # Latency alerts
        lat_alerts = analysis['latency_alerts']
        if lat_alerts.get('significant_spike'):
            report.append("✗ CRITICAL: Significant latency spike detected")
        elif lat_alerts.get('exceeds_threshold'):
            report.append("✗ CRITICAL: Latency exceeds threshold (>10s/pkg)")
        elif lat_alerts.get('degraded_from_baseline'):
            report.append("⚠ WARNING: Latency degraded from baseline")
        else:
            report.append("✓ Latency within acceptable range")

        report.append("")

        # Overhead alerts
        over_alerts = analysis['overhead_alerts']
        if over_alerts.get('exceeds_threshold'):
            report.append("✗ CRITICAL: Heap overhead exceeds limit (>5%)")
        elif over_alerts.get('degraded_from_baseline'):
            report.append("⚠ WARNING: Heap overhead degraded from baseline")
        else:
            report.append("✓ Heap overhead within acceptable range")

        report.append("")

        # Cache alerts
        cache_alerts = analysis['cache_alerts']
        if cache_alerts.get('exceeds_threshold'):
            report.append("✗ CRITICAL: Cache miss rate exceeds limit (>3%)")
        elif cache_alerts.get('degraded_from_baseline'):
            report.append("⚠ WARNING: Cache efficiency degraded from baseline")
        else:
            report.append("✓ Cache efficiency within acceptable range")

        report.append("")
        report.append("=" * 70)

        return '\n'.join(report)

    def add_metrics(self, metrics: Dict) -> None:
        """Add metrics to history."""
        self.history.append(metrics)
        self._save_history()

    def get_summary_stats(self) -> Dict:
        """Get summary statistics over history."""
        if not self.history:
            return {
                'samples': 0,
                'avg_coherence': 0.0,
                'avg_latency': 0.0,
                'avg_overhead': 0.0,
                'avg_cache_miss': 0.0,
            }

        return {
            'samples': len(self.history),
            'avg_coherence': sum(h.get('coherence_phi', 0) for h in self.history) / len(self.history),
            'avg_latency': sum(h.get('avg_latency', 0) for h in self.history) / len(self.history),
            'avg_overhead': sum(h.get('heap_overhead', 0) for h in self.history) / len(self.history),
            'avg_cache_miss': sum(h.get('l1_miss_rate', 0) for h in self.history) / len(self.history),
        }


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: detect_regressions.py <metrics_json_file>")
        sys.exit(1)

    metrics_file = sys.argv[1]

    try:
        with open(metrics_file, 'r') as f:
            metrics = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"Error reading metrics file: {e}")
        sys.exit(1)

    # Detect regressions
    detector = RegressionDetector()
    analysis = detector.analyze_all_metrics(metrics)

    # Add to history
    detector.add_metrics(metrics)

    # Print report
    report = detector.generate_alert_report(analysis)
    print(report)

    # Print summary
    stats = detector.get_summary_stats()
    print("\nHistorical Statistics:")
    print(f"  Samples: {stats['samples']}")
    print(f"  Avg Coherence φ: {stats['avg_coherence']:.4f}")
    print(f"  Avg Latency: {stats['avg_latency']:.2f}s/pkg")
    print(f"  Avg Overhead: {stats['avg_overhead']:.2%}")
    print(f"  Avg Cache Miss Rate: {stats['avg_cache_miss']:.2%}")

    # Exit with appropriate code
    if analysis['overall_status'] == 'CRITICAL':
        sys.exit(1)
    elif analysis['overall_status'] == 'WARN':
        sys.exit(0)  # Warning but not failure
    else:
        print("\n✓ No regressions detected")
        sys.exit(0)


if __name__ == '__main__':
    main()
