---
severity: medium
status: pending
category: performance
file: src/pacer.cpp
line: 61
---

# CR-013: Spin-wait consumes full CPU core for high bitrates

**Severity:** Medium — should fix, non-blocking

## Problem

For inter-burst gaps under 2ms (e.g., 1 Gbps with 1470B packets: burst=171, burst gap=2.01ms, actual send time ~300-800μs, leaving 1.2-1.7ms pure spin-wait), the tool consumes 60-85% of one CPU core. This is expected for a benchmark tool but should be documented.

## Location

`src/pacer.cpp:61-63`

## Fix

1. Document in README: "High bitrate tests consume close to 100% of one CPU core due to spin-wait pacing"
2. Optionally implement a `CreateWaitableTimer`/`SetWaitableTimer` path for sub-2ms waits, but the current approach is valid for a benchmarking tool

## Verification

- N/A (documentation only)

### Effort: Small (documentation)
