#pragma once

#include <cstdint>

namespace cpu_monitor {

class ICpuReader {
public:
    virtual ~ICpuReader() = default;

    ICpuReader(const ICpuReader&)            = delete;
    ICpuReader& operator=(const ICpuReader&) = delete;
    ICpuReader(ICpuReader&&)                 = delete;
    ICpuReader& operator=(ICpuReader&&)      = delete;

    virtual void         init()                                  = 0;
    virtual unsigned int coreCount() const noexcept              = 0;
    virtual void         read(uint8_t* buf, unsigned int count)  = 0;
    virtual const char*  name()       const noexcept             = 0;

protected:
    ICpuReader() = default;
};

} // namespace cpu_monitor
