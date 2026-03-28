#pragma once

// ---------------------------------------------------------------------------
// ThreadAffinity — cross-platform thread-to-core pinning.
//
// Supported platforms:
//   Linux/glibc : pthread_setaffinity_np  (GNU extension)
//   Linux/musl  : sched_setaffinity(0,..) (syscall, no GNU dep)
//   FreeBSD     : pthread_setaffinity_np  (via pthread_np.h)
//
// Returns true on success, false if pinning failed or is unsupported.
// Failure is non-fatal — IdleTaskReader still works, just less accurate.
// ---------------------------------------------------------------------------

#include <cstdint>

#if defined(__linux__) || defined(__FreeBSD__)
#  include <sched.h>      // cpu_set_t, CPU_SET, CPU_ZERO
#  include <pthread.h>
#  if defined(__FreeBSD__)
#    include <pthread_np.h>  // pthread_setaffinity_np on FreeBSD
#  endif
#endif

namespace cpu_monitor {

struct ThreadAffinity {
    ThreadAffinity()  = delete;
    ~ThreadAffinity() = delete;

    static bool setAffinity(unsigned int coreId) noexcept {
#if defined(__linux__) || defined(__FreeBSD__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<int>(coreId), &cpuset);

#  if defined(__FreeBSD__) || defined(__GLIBC__)
        // glibc (Linux) and FreeBSD both provide pthread_setaffinity_np
        return ::pthread_setaffinity_np(
                   ::pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#  else
        // musl libc: no pthread_setaffinity_np — use raw syscall instead
        // pid=0 means "calling thread"
        return ::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0;
#  endif

#else
        // Platform not supported — non-fatal
        (void)coreId;
        return false;
#endif
    }
};

} // namespace cpu_monitor
