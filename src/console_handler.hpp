#pragma once

#include <atomic>

namespace nb {

/// Global shutdown flag, set by Ctrl+C handler.
extern std::atomic<bool> g_shutdown;

/// Register the console control handler (Ctrl+C, Ctrl+Break, close).
/// Must be called from main() before starting any network operations.
void register_console_handler();

} // namespace nb
