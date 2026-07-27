---
severity: important
status: completed
category: correctness
file: src/client_handler.cpp
line: 220
---

# CR-003: server_summary data race (missing memory_order_acquire)

**Severity:** Important — should fix before merge

## Problem

Receiver thread writes `server_summary` (non-atomic struct), then signals via `result_received.store(true, memory_order_release)`. Main thread reads `result_received.load(memory_order_relaxed)` — no acquire barrier. There is no formal happens-before between the struct write and read. On ARM64 or under aggressive compiler optimization, this is undefined behavior.

## Location

`src/client_handler.cpp:220` (also line 249)

```cpp
if (result_received) {            // ← relaxed load
    reporter->report_summary(local_summary, &server_summary);
}
```

## Fix

Change the result_received load to memory_order_acquire:

```cpp
if (result_received.load(std::memory_order_acquire)) {
    reporter->report_summary(local_summary, &server_summary);
}
```

Also apply same fix to the Finish retry loop at line 161.

## Verification

- Static analysis: confirm acquire-release pairing is correct
- No behavioral change on x86 (strong ordering), but correct by standard

### Effort: Small
