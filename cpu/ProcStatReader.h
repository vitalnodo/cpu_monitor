#pragma once

#include "cpu/ICpuReader.h"

#include <cstdint>
#include <vector>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// CoreTick — snapshot of one core's /proc/stat counters.
// ---------------------------------------------------------------------------
struct CoreTick {
    unsigned long long user{0}, nice{0}, system{0}, idle{0},
                       iowait{0}, irq{0}, softirq{0}, steal{0};

    unsigned long long active() const noexcept {
        return user + nice + system + irq + softirq + steal;
    }
    unsigned long long total() const noexcept {
        return active() + idle + iowait;
    }
    uint8_t loadSince(const CoreTick& prev) const noexcept {
        const auto dActive = active() - prev.active();
        const auto dTotal  = total()  - prev.total();
        if (dTotal == 0) { return 0; }
        const auto pct = (dActive * 100ULL + dTotal / 2) / dTotal;
        return static_cast<uint8_t>(pct > 100 ? 100 : pct);
    }
};

// ---------------------------------------------------------------------------
// ProcStatReader — Linux /proc/stat, delta between two snapshots.
// cur_ pre-allocated in init() — no allocation in read().
// ---------------------------------------------------------------------------
class ProcStatReader final : public ICpuReader {
public:
    ProcStatReader()  = default;
    ~ProcStatReader() override = default;

    void         init()                                 override;
    unsigned int coreCount() const noexcept             override;
    void         read(uint8_t* buf, unsigned int count) override;
    const char*  name()       const noexcept            override { return "proc_stat"; }

private:
    bool parseProcStat(std::vector<CoreTick>& out) const;

    unsigned int          numCores_{0};
    std::vector<CoreTick> prev_;   // previous snapshot
    std::vector<CoreTick> cur_;    // current snapshot — pre-allocated in init()
};

} // namespace cpu_monitor
