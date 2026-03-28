#pragma once

#include "cpu/ICpuReader.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <pthread.h>

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// CoreWorker — synthetic idle task pinned to one CPU core.
//
// The worker runs a tight counter-increment loop. A periodic snapshot
// measures how many iterations were completed in SAMPLE_MS milliseconds.
// Fewer iterations → more preemption → higher load.
//
//   Phase 1 (Init/calibration): WARMUP_SAMPLES windows → baseline_
//   Phase 2 (Run):              continuous windows → load_  [0..100]
//
// currentLoad() is a single atomic read — never blocks the caller.
// ---------------------------------------------------------------------------
class CoreWorker {
public:
    static constexpr int  WARMUP_SAMPLES = 3;
    static constexpr long SAMPLE_MS      = 200;

    explicit CoreWorker(unsigned int coreId);
    ~CoreWorker();

    CoreWorker(const CoreWorker&)            = delete;
    CoreWorker& operator=(const CoreWorker&) = delete;
    CoreWorker(CoreWorker&&)                 = delete;
    CoreWorker& operator=(CoreWorker&&)      = delete;

    void    start();      // spawn thread, pin to core   — Init
    void    calibrate();  // wait for baseline to settle — Init
    void    stop();       // signal + join               — destructor

    uint8_t currentLoad() const noexcept;  // atomic read — Run, no blocking

private:
    static void* threadEntry(void* arg);
    void         idleLoop();
    void         setLowestPriority() const;

    unsigned int                    coreId_;
    pthread_t                       thread_{};
    std::atomic<bool>               stopFlag_{false};
    bool                            started_{false};

    // counter_ written by worker in tight loop; also read by worker for delta.
    // atomic so calibrate() on main thread can safely read for sanity checks.
    std::atomic<unsigned long long> counter_{0};

    // baseline_ written once in idleLoop() phase 1, read-only in phase 2.
    // No synchronisation needed beyond the happens-before of phase 1 → phase 2.
    unsigned long long              baseline_{1};

    // load_ written by worker thread, read by main thread via currentLoad().
    std::atomic<uint8_t>            load_{0};
};

// ---------------------------------------------------------------------------
// IdleTaskReader — owns one CoreWorker per logical CPU.
// ---------------------------------------------------------------------------
class IdleTaskReader final : public ICpuReader {
public:
    IdleTaskReader()  = default;
    ~IdleTaskReader() override = default;

    void         init()                                 override;
    unsigned int coreCount() const noexcept             override;
    void         read(uint8_t* buf, unsigned int count) override;
    const char*  name()       const noexcept            override { return "idle_task"; }

private:
    static unsigned int detectCoreCount();

    unsigned int                             numCores_{0};
    std::vector<std::unique_ptr<CoreWorker>> workers_;
};

} // namespace cpu_monitor
