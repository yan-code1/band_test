---
severity: medium
status: completed
category: correctness
file: src/stats_collector.cpp
line: 72
---

# CR-010: jitter_min tracks smoothed average not instant minimum

**Severity:** Medium — should fix, non-blocking

## Problem

`jitter_min_` is calculated as `min(jitter_min_, jitter_)` where `jitter_` is the exponential moving average (not the instantaneous transit deviation). The minimum of an EMA does not reflect actual latency variation lows — it reflects the lowest observed average.

## Location

`src/stats_collector.cpp:72-73`

```cpp
jitter_ += (delta_ms - jitter_) / 16.0;
jitter_min_ = std::min(jitter_min_, jitter_);  // ← wrong: compares EMA against itself
```

## Fix

Track raw transit deltas separately:

```cpp
int64_t delta = transit - prev_transit_;
if (delta < 0) delta = -delta;
double delta_ms = static_cast<double>(delta) / 1'000'000.0;
jitter_ += (delta_ms - jitter_) / 16.0;     // EMA for average
jitter_min_ = std::min(jitter_min_, delta_ms);  // raw delta for min
jitter_max_ = std::max(jitter_max_, delta_ms);  // raw delta for max
```

## Verification

- Compare jitter_min output before/after with known delay patterns

### Effort: Small
