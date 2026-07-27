---
severity: medium
status: pending
category: performance
file: src/pacer.cpp
line: 43
---

# CR-007: Windows default timer granularity causes sleep overshoot

**Severity:** Medium — should fix, non-blocking

## Problem

Windows default timer period is ~15.6 ms. `sleep_for(2ms)` rounds up to the next tick, resulting in ~15.6 ms actual sleep. For intervals just above 2ms (e.g., 5 Mbps with 1470B packets = 2.35ms interval), the pacer massively overshoots, then sends catch-up bursts.

## Location

`src/pacer.cpp:43-49`

## Fix

Either:
1. Document as known limitation
2. Call `timeBeginPeriod(1)` at startup (reduces timer to 1ms, slight power cost) with matching `timeEndPeriod(1)` on shutdown
3. Increase sleep threshold to 16ms to fully amortize the tick

## Verification

- Measure observed vs requested bitrate at 5-50 Mbps with and without `timeBeginPeriod(1)`

### Effort: Small
