---
severity: minor
status: pending
category: cleanup
file: src/reporter.cpp
line: 236
---

# CR-016: Terminal output not mutex-protected across threads

**Severity:** Minor — optional

## Problem

`write_terminal()` writes to `std::cout` and optional logfile from both sender and receiver threads without synchronization. While unlikely to crash on Windows, output lines may interleave.

## Location

`src/reporter.cpp:236-241`

## Fix

Add a `std::mutex` to `Reporter` and lock in `write_terminal()`.

## Effort: Small
