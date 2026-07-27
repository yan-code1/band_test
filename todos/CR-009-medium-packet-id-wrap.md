---
severity: medium
status: completed
category: correctness
file: src/client_handler.cpp
line: 160
---

# CR-009: packet_id wraps to 0 (reserved for control messages)

**Severity:** Medium — should fix, non-blocking

## Problem

When `packet_id` reaches `UINT32_MAX`, `packet_id++` wraps to 0. The protocol reserves packet_id=0 for control messages. A data packet with id=0 violates this convention and may be treated as a control message or mis-detected by the server.

## Location

`src/client_handler.cpp:160` (also `stats_collector.cpp` depends on sequential IDs)

## Fix

After increment, check if `packet_id == 0` and either:
- Stop the test (for typical benchmark durations, this won't trigger)
- Wrap to 1 instead of 0

## Verification

- Artificially start packet_id near UINT32_MAX and verify it wraps to 1

### Effort: Small
