---
severity: medium
status: completed
category: cleanup
file: src/reporter.cpp
line: 193
---

# CR-006: Duplicate JSON end/server sections

**Severity:** Medium — should fix, non-blocking

## Problem

When server data is available, `json_["end"]` and `json_["server"]` are populated with nearly identical data. This doubles JSON output size with no differentiating purpose.

## Location

`src/reporter.cpp:193-205`

## Fix

Remove the `json_["server"]` section. Keep only `json_["end"]` which already contains the same data.

## Verification

- Run with `-J` flag, verify JSON output has only `end` section with all required fields

### Effort: Small
