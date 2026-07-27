---
severity: minor
status: pending
category: cleanup
file: src/stats_collector.hpp
line: 68
---

# CR-017: StatsCollector mutex unnecessary (single-threaded usage)

**Severity:** Minor — optional

## Problem

`StatsCollector` is only used from the single-threaded server, so the `std::mutex` is never contended. It adds cognitive overhead (reader wonders: "who else calls this?").

## Location

`src/stats_collector.hpp:68` — `std::mutex mtx_`

## Fix

Remove `mtx_` and all `lock_guard` lines. Or keep with a comment: "Uncontended mutex — retained for future multi-threaded server support."

## Effort: Small
