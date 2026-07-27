---
severity: medium
status: pending
category: correctness
file: src/client_handler.cpp
line: 71
---

# CR-012: high_resolution_clock::time_since_epoch() is boot-relative on MSVC

**Severity:** Medium — should fix, non-blocking

## Problem

On MSVC, `high_resolution_clock` is an alias for `steady_clock`. `time_since_epoch().count()` returns time since last system boot, not the Unix epoch. The packet header timestamps (`timestamp_sec`/`timestamp_nsec`) are therefore boot-relative, not wall-clock times.

This is functionally correct for RFC 3550 jitter (which uses transit time deltas, canceling systematic offsets), but the absolute timestamps in the JSON output have no wall-clock meaning, and the code is misleading to readers.

## Location

`src/client_handler.cpp:71` and `server_handler.cpp:65-66`

```cpp
auto now_ns = []() -> uint64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
};
```

## Fix

Either:
- Switch to `std::chrono::system_clock` for wall-clock timestamps (using `duration_cast` plus known epoch), keeping `steady_clock` only for `Pacer::wait_until`
- Or add a comment explaining that on MSVC this is boot-relative and valid only for delta calculations

## Verification

- Compare JSON timestamp values against system clock

### Effort: Small
