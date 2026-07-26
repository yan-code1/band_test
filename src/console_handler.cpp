#include "console_handler.hpp"
#include "platform.hpp"

#include <cstdlib>

namespace nb {

std::atomic<bool> g_shutdown{false};

static BOOL WINAPI handler_routine(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        g_shutdown.store(true, std::memory_order_relaxed);
        return TRUE;  // handled

    default:
        return FALSE; // pass to next handler
    }
}

void register_console_handler() {
    SetConsoleCtrlHandler(handler_routine, TRUE);
}

} // namespace nb
