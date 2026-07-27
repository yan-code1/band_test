---
severity: medium
status: completed
category: cleanup
file: src/stats_collector.cpp
line: 87
---

# CR-005: Dead code — StatsCollector::next_interval()

**Severity:** Medium — should fix, non-blocking

## Problem

`next_interval()` (28 lines), per-interval counters `interval_bytes_`/`interval_packets_`, and `IntervalSnapshot` fields `jitter_min_ms`/`jitter_max_ms` are never called or populated. The sender thread computes intervals locally. The server never calls `next_interval()`.

## Location

`src/stats_collector.cpp:87-114` — entire function is dead code

## Fix

Delete `next_interval()` and remove `interval_bytes_` / `interval_packets_` from the class. Simplify `IntervalSnapshot` if no longer needed as a return type.

## Verification

- Grep for `next_interval` — zero call sites confirms deletion is safe

### Effort: Small
