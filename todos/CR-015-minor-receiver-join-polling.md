---
severity: minor
status: completed
category: cleanup
file: src/client_handler.cpp
line: 232
---

# CR-015: Receiver join uses polling loop

**Severity:** Minor — optional

## Problem

The main thread polls every 50ms for up to 1s waiting for the receiver to finish. 20 wakeups that could be avoided with a condition variable.

## Location

`src/client_handler.cpp:232-239`

## Fix

Either:
- Use `std::condition_variable` for zero-wakeup waiting
- Or simplify to just `receiver.join()` since the receiver will exit within its 100ms select timeout

## Effort: Small
