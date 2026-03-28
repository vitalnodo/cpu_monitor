#include "cpu/IdleTaskReader.h"
#include "cpu/ThreadAffinity.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <time.h>
#include <unistd.h>
#include <sys/resource.h>

#if defined(__linux__)
#  include <sched.h>
#endif

namespace cpu_monitor {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static unsigned long long monotonicNs() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<unsigned long long>(ts.tv_sec) * 1'000'000'000ULL
           + static_cast<unsigned long long>(ts.tv_nsec);
}

static void sleepMs(long ms) noexcept {
    struct timespec req{};
    req.tv_sec  = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1'000'000L;
    ::nanosleep(&req, nullptr);
}

// ===========================================================================
// CoreWorker
// ===========================================================================

CoreWorker::CoreWorker(unsigned int coreId)
    : coreId_{coreId}
{}

CoreWorker::~CoreWorker() {
    stop();
}

void CoreWorker::start() {
    stopFlag_.store(false, std::memory_order_relaxed);
    counter_.store(0, std::memory_order_relaxed);

    const int rc = ::pthread_create(&thread_, nullptr,
                                    &CoreWorker::threadEntry, this);
    if (rc != 0) {
        throw std::runtime_error{
            std::string{"CoreWorker::start — pthread_create failed (core "}
            + std::to_string(coreId_) + "): " + std::strerror(rc)};
    }
    started_ = true;
}

// ---------------------------------------------------------------------------
// calibrate — called from main thread.
// Waits WARMUP_SAMPLES × SAMPLE_MS for the worker to establish baseline_.
// baseline_ is set inside idleLoop() — we just wait long enough.
// ---------------------------------------------------------------------------
void CoreWorker::calibrate() {
    // Wait for worker to complete warm-up phase (it does WARMUP_SAMPLES windows)
    sleepMs(static_cast<long>(WARMUP_SAMPLES + 1) * SAMPLE_MS);

    const uint8_t initialLoad = load_.load(std::memory_order_relaxed);
    if (initialLoad > 20) {
        std::cerr << "[CoreWorker " << coreId_
                  << "] warning: high load during calibration ("
                  << static_cast<unsigned>(initialLoad)
                  << "%) — baseline may be inaccurate\n";
    }
}

void CoreWorker::stop() {
    if (!started_) { return; }
    stopFlag_.store(true, std::memory_order_relaxed);
    ::pthread_join(thread_, nullptr);
    started_ = false;
}

uint8_t CoreWorker::currentLoad() const noexcept {
    return load_.load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// threadEntry
// ---------------------------------------------------------------------------
void* CoreWorker::threadEntry(void* arg) {
    auto* self = static_cast<CoreWorker*>(arg);
    ThreadAffinity::setAffinity(self->coreId_);
    self->setLowestPriority();
    self->idleLoop();
    return nullptr;
}

// ---------------------------------------------------------------------------
// idleLoop — correct idle-task measurement.
//
// The idle thread increments counter_ in a tight inner loop.
// A periodic snapshot measures delta over SAMPLE_MS wall-clock time.
//
// Phase 1 (WARMUP_SAMPLES windows): record max delta as baseline_
//   (represents 100% idle — as many iters as possible with no contention)
//
// Phase 2 (continuous): compare delta to baseline_ → compute load_
//
// counter_ is std::atomic so main thread can safely read it during calibrate.
// ---------------------------------------------------------------------------
void CoreWorker::idleLoop() {
    // --- Phase 1: calibration ---
    unsigned long long maxDelta = 0;

    for (int w = 0; w < WARMUP_SAMPLES; ++w) {
        const unsigned long long t0      = monotonicNs();
        const unsigned long long target  = t0 + static_cast<unsigned long long>(SAMPLE_MS) * 1'000'000ULL;

        const unsigned long long before = counter_.load(std::memory_order_relaxed);

        // Tight idle loop for exactly SAMPLE_MS
        while (monotonicNs() < target) {
            counter_.fetch_add(1, std::memory_order_relaxed);
        }

        const unsigned long long delta = counter_.load(std::memory_order_relaxed) - before;
        if (delta > maxDelta) { maxDelta = delta; }
    }

    baseline_ = (maxDelta > 0) ? maxDelta : 1;

    // --- Phase 2: continuous measurement ---
    while (!stopFlag_.load(std::memory_order_relaxed)) {
        const unsigned long long t0     = monotonicNs();
        const unsigned long long target = t0 + static_cast<unsigned long long>(SAMPLE_MS) * 1'000'000ULL;

        const unsigned long long before = counter_.load(std::memory_order_relaxed);

        while (monotonicNs() < target
               && !stopFlag_.load(std::memory_order_relaxed)) {
            counter_.fetch_add(1, std::memory_order_relaxed);
        }

        const unsigned long long delta = counter_.load(std::memory_order_relaxed) - before;

        long long loadPct = 100LL
            - static_cast<long long>((delta * 100ULL) / baseline_);
        if (loadPct < 0)   { loadPct = 0; }
        if (loadPct > 100) { loadPct = 100; }

        load_.store(static_cast<uint8_t>(loadPct), std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// setLowestPriority
// ---------------------------------------------------------------------------
void CoreWorker::setLowestPriority() const {
#if defined(__linux__)
    struct sched_param sp{};
    sp.sched_priority = 0;
    if (::pthread_setschedparam(::pthread_self(), SCHED_IDLE, &sp) == 0) {
        return;
    }
#endif
    ::setpriority(PRIO_PROCESS, 0, 19);
}

// ===========================================================================
// IdleTaskReader
// ===========================================================================

void IdleTaskReader::init() {
    numCores_ = detectCoreCount();
    if (numCores_ == 0) {
        throw std::runtime_error{"IdleTaskReader::init — could not detect core count"};
    }

    workers_.reserve(numCores_);
    for (unsigned int i = 0; i < numCores_; ++i) {
        workers_.emplace_back(std::make_unique<CoreWorker>(i));
    }

    for (auto& w : workers_) { w->start(); }

    std::cout << "[IdleTaskReader] calibrating " << numCores_
              << " cores (" << CoreWorker::WARMUP_SAMPLES
              << " x " << CoreWorker::SAMPLE_MS << "ms)...\n";

    // Calibrate all cores in parallel — each calibrate() just sleeps
    for (auto& w : workers_) { w->calibrate(); }

    std::cout << "[IdleTaskReader] ready.\n"
              << "[IdleTaskReader] WARNING: idle-task heuristic — readings are approximate.\n"
              << "  Self-loading effect causes systematic underestimation of true load.\n"
              << "  Use --reader=proc_stat on Linux for accurate results.\n";
}

unsigned int IdleTaskReader::coreCount() const noexcept { return numCores_; }

// read() — O(N) atomic loads, no blocking, no allocation
void IdleTaskReader::read(uint8_t* buf, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
        buf[i] = workers_[i]->currentLoad();
    }
}

unsigned int IdleTaskReader::detectCoreCount() {
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? static_cast<unsigned int>(n) : 1u;
}

} // namespace cpu_monitor
