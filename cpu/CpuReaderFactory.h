#pragma once

#include "cpu/ICpuReader.h"

#include <memory>
#include <string>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
enum class ReaderMethod {
    Auto,       // platform-based selection (see factory chain below)
    ProcStat,   // Linux /proc/stat
    FreeBsd,    // FreeBSD sysctl kern.cp_times
    IdleTask,   // POSIX idle-task heuristic (cross-platform fallback)
};

// Parses "auto" | "proc_stat" | "freebsd" | "idle_task"
// Throws std::invalid_argument on unknown string
ReaderMethod readerMethodFromString(const std::string& s);

// ---------------------------------------------------------------------------
// CpuReaderFactory
//
// Auto selection chain:
//   #ifdef __linux__    → ProcStatReader
//   #ifdef __FreeBSD__  → FreeBsdReader
//   _POSIX_THREADS      → IdleTaskReader
//   else                → UnsupportedReader  (init() throws)
// ---------------------------------------------------------------------------
class CpuReaderFactory {
public:
    CpuReaderFactory()  = default;
    ~CpuReaderFactory() = default;

    CpuReaderFactory(const CpuReaderFactory&)            = delete;
    CpuReaderFactory& operator=(const CpuReaderFactory&) = delete;

    static std::unique_ptr<ICpuReader> create(ReaderMethod method = ReaderMethod::Auto);
};

} // namespace cpu_monitor
