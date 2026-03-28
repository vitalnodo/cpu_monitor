#pragma once

#include "cpu/ICpuReader.h"
#include <stdexcept>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// UnsupportedReader — terminal stub used by the factory when no concrete
// reader can be constructed for the current platform.
// init() always throws, so the application fails fast with a clear message.
// ---------------------------------------------------------------------------
class UnsupportedReader final : public ICpuReader {
public:
    UnsupportedReader() = default;
    ~UnsupportedReader() override = default;

    void init() override {
        throw std::runtime_error(
            "No supported CPU reader available on this platform.\n"
            "Supported backends: proc_stat | freebsd | idle_task | unsupported.");
    }

    unsigned int coreCount() const noexcept override { return 0; }

    void read(uint8_t* /*buf*/, unsigned int /*count*/) override {
        // Should never be called — init() throws before Run state is entered
    }

    const char* name() const noexcept override { return "unsupported"; }
};

} // namespace cpu_monitor
