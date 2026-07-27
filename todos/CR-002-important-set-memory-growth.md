---
severity: important
status: pending
category: performance
file: src/stats_collector.hpp
line: 94
---

# CR-002: Unbounded std::set memory growth for duplicate detection

**Severity:** Important — should fix before merge

## Problem

`std::set<uint32_t> seen_ids_` stores every unique packet ID with no eviction. At 10 Gbps with 100B packets (~12.5M pkts in 10s), the set grows to ~400-500 MB. At 60s, exceeds 3 GB. Each insertion is O(log N) with severe cache-miss overhead.

## Location

`src/stats_collector.hpp:94`

```cpp
std::set<uint32_t> seen_ids_;  // duplicate detection
```

## Fix

Replace with a sliding-window approach: only track IDs within a configurable window behind `last_packet_id_` (e.g., `std::unordered_set<uint32_t>` with periodic pruning of IDs < `last_packet_id_ - window`). For low-loss scenarios, a bitmap covering the expected range is even more memory-efficient.

## Verification

- Run `-b 1000m -t 60` and measure peak memory
- Verify duplicate detection still works correctly

### Effort: Medium
