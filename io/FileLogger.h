#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// FileLogger — RAII wrapper around a FILE* for periodic CSV-style logging.
//
// Format per line:
//   <unix_timestamp>,<core0_load>,<core1_load>,...\n
//
// Opened in Init state; write() called in Run state (no allocation).
// ---------------------------------------------------------------------------
class FileLogger {
public:
    explicit FileLogger(const std::string& path);

    // RAII — closes file in destructor
    ~FileLogger();

    // Non-copyable
    FileLogger(const FileLogger&)            = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    // Write one log line — called in Run state, no dynamic allocation.
    void write(const uint8_t* loads, unsigned int count);

private:
    FILE* file_{nullptr};
};

} // namespace cpu_monitor
