#include "cpu/FreeBsdReader.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

#if defined(__FreeBSD__)
#  include <sys/sysctl.h>
#  include <sys/resource.h>
#endif

namespace cpu_monitor {

void FreeBsdReader::init() {
#if !defined(__FreeBSD__)
    throw std::runtime_error{"FreeBsdReader: only supported on FreeBSD"};
#else
    std::vector<CoreTick> tmp;
    if (!readSysctl(tmp) || tmp.empty()) {
        throw std::runtime_error{
            "FreeBsdReader: failed to read kern.cp_times via sysctl"};
    }
    numCores_ = static_cast<unsigned int>(tmp.size());
    prev_     = std::move(tmp);
    cur_.resize(numCores_);                         // pre-allocate
    rawBuf_.resize(numCores_ * CPUSTATES);          // pre-allocate sysctl buffer

    std::cout << "[FreeBsdReader] detected " << numCores_ << " cores\n";
#endif
}

unsigned int FreeBsdReader::coreCount() const noexcept { return numCores_; }

void FreeBsdReader::read(uint8_t* buf, unsigned int count) {
    assert(count == numCores_);
    if (!readSysctl(cur_)) {
        std::memset(buf, 0, count);
        return;
    }
    for (unsigned int i = 0; i < numCores_; ++i) {
        buf[i]   = cur_[i].loadSince(prev_[i]);
        prev_[i] = cur_[i];
    }
}

bool FreeBsdReader::readSysctl(std::vector<CoreTick>& out) const {
#if !defined(__FreeBSD__)
    (void)out;
    return false;
#else
    std::size_t len = rawBuf_.size() * sizeof(unsigned long);
    if (::sysctlbyname("kern.cp_times", rawBuf_.data(), &len, nullptr, 0) != 0) {
        return false;
    }

    const std::size_t n = len / (CPUSTATES * sizeof(unsigned long));
    if (n == 0) { return false; }

    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned long* p = rawBuf_.data() + i * CPUSTATES;
        out[i].user = p[CP_USER];
        out[i].nice = p[CP_NICE];
        out[i].sys  = p[CP_SYS];
        out[i].intr = p[CP_INTR];
        out[i].idle = p[CP_IDLE];
    }
    return true;
#endif
}

} // namespace cpu_monitor
