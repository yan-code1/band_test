#pragma once

#include "stats_collector.hpp"
#include "argument_parser.hpp"

#include <string>
#include <fstream>
#include <memory>

namespace nb {

/// Terminal text + JSON output formatting.
class Reporter {
public:
    explicit Reporter(const Config& cfg);
    ~Reporter();

    /// Print test start information.
    void report_start(const Config& cfg);

    /// Print a per-interval snapshot.
    void report_interval(const IntervalSnapshot& snap);

    /// Print the final test summary.
    void report_summary(const StatsSummary& local,
                        const StatsSummary* server);

    /// Print an error message.
    void report_error(const std::string& msg);

    /// Print an informational status message.
    void report_info(const std::string& msg);

private:
    bool     json_mode_ = false;
    bool     forceflush_ = false;
    std::unique_ptr<std::ofstream> logfile_;

    // JSON state
    std::string json_start_;
    std::string json_intervals_;
    std::string json_end_;

    void write_terminal(const std::string& line);
    void write_json(const std::string& fragment);
    void flush();
};

} // namespace nb
