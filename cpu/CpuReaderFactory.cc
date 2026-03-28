#include "cpu/CpuReaderFactory.h"

#include "cpu/FreeBsdReader.h"
#include "cpu/IdleTaskReader.h"
#include "cpu/ProcStatReader.h"
#include "cpu/UnsupportedReader.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace cpu_monitor {

ReaderMethod readerMethodFromString(const std::string& s) {
    std::string lower(s.size(), '\0');
    std::transform(s.begin(), s.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "auto")      { return ReaderMethod::Auto;     }
    if (lower == "proc_stat") { return ReaderMethod::ProcStat; }
    if (lower == "freebsd")   { return ReaderMethod::FreeBsd;  }
    if (lower == "idle_task") { return ReaderMethod::IdleTask; }

    throw std::invalid_argument{
        "Unknown reader method: '" + s +
        "'. Valid: auto | proc_stat | freebsd | idle_task"};
}

std::unique_ptr<ICpuReader> CpuReaderFactory::create(ReaderMethod method) {
    switch (method) {
    case ReaderMethod::ProcStat:
        return std::make_unique<ProcStatReader>();

    case ReaderMethod::FreeBsd:
        return std::make_unique<FreeBsdReader>();

    case ReaderMethod::IdleTask:
        return std::make_unique<IdleTaskReader>();

    case ReaderMethod::Auto:
    default:
#if defined(__linux__)
        return std::make_unique<ProcStatReader>();
#elif defined(__FreeBSD__)
        return std::make_unique<FreeBsdReader>();
#elif defined(_POSIX_THREADS)
        return std::make_unique<IdleTaskReader>();
#else
        return std::make_unique<UnsupportedReader>();
#endif
    }
}

} // namespace cpu_monitor
