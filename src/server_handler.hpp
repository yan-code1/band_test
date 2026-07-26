#pragma once

#include "argument_parser.hpp"

namespace nb {

/// Run the server state machine.
/// Blocks until shutdown (Ctrl+C, idle timeout, or single-shot completion).
void run_server(const Config& cfg);

} // namespace nb
