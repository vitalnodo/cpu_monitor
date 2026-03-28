#pragma once

#include "cpu/ICpuReader.h"

#include <cstdint>
#include <vector>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// FreeBsdReader — FreeBSD sysctl kern.cp_times
//
// kern.cp_times returns CPUSTATES longs per core:
//   [CP_USER, CP_NICE, CP_SYS, CP_INTR, CP_IDLE] x numCores
//
// cur_ pre-allocated in init() — no heap allocation in read().
// ---------------------------------------------------------------------------
class FreeBsdReader final : public ICpuReader {
public:
    FreeBsdReader()  = default;
    ~FreeBsdReader() override = default;

    void         init()                                 override;
    unsigned int coreCount() const noexcept             override;
    void         read(uint8_t* buf, unsigned int count) override;
    const char*  name()       const noexcept            override { return "freebsd_sysctl"; }

private:
    struct CoreTick {
        unsigned long user{0}, nice{0}, sys{0}, intr{0}, idle{0};

        unsigned long active() const noexcept { return user + nice + sys + intr; }
        unsigned long total()  const noexcept { return active() + idle; }

        uint8_t loadSince(const CoreTick& prev) const noexcept {
            const auto dActive = active() - prev.active();
            const auto dTotal  = total()  - prev.total();
            if (dTotal == 0) { return 0; }
            const auto pct = (dActive * 100UL + dTotal / 2) / dTotal;
            return static_cast<uint8_t>(pct > 100 ? 100 : pct);
        }
    };

    bool readSysctl(std::vector<CoreTick>& out) const;

    unsigned int          numCores_{0};
    std::vector<CoreTick> prev_;
    std::vector<CoreTick> cur_;    // pre-allocated in init()
    std::vector<unsigned long> rawBuf_; // pre-allocated sysctl buffer
};

} // namespace cpu_monitor
