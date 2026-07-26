#pragma once

#include "argument_parser.hpp"

namespace nb {

/// Run the client test.
/// Resolves host, sends traffic with pacing, receives results, reports.
void run_client(const Config& cfg);

} // namespace nb
