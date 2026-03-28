#include "cpu/ProcStatReader.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace cpu_monitor {

void ProcStatReader::init() {
#ifndef __linux__
    throw std::runtime_error{"ProcStatReader: /proc/stat is Linux-only"};
#else
    std::vector<CoreTick> tmp;
    if (!parseProcStat(tmp) || tmp.empty()) {
        throw std::runtime_error{
            "ProcStatReader: failed to read per-core lines from /proc/stat"};
    }

    numCores_ = static_cast<unsigned int>(tmp.size());
    prev_     = std::move(tmp);
    cur_.resize(numCores_);   // pre-allocate — no alloc in read()

    std::cout << "[ProcStatReader] detected " << numCores_ << " cores\n";
#endif
}

unsigned int ProcStatReader::coreCount() const noexcept { return numCores_; }

void ProcStatReader::read(uint8_t* buf, unsigned int count) {
    assert(count == numCores_);

    if (!parseProcStat(cur_)) {
        std::memset(buf, 0, count);
        return;
    }

    for (unsigned int i = 0; i < numCores_; ++i) {
        buf[i]   = cur_[i].loadSince(prev_[i]);
        prev_[i] = cur_[i];
    }
}

bool ProcStatReader::parseProcStat(std::vector<CoreTick>& out) const {
#ifndef __linux__
    (void)out;
    return false;
#else
    FILE* f = std::fopen("/proc/stat", "r");
    if (!f) { return false; }

    out.clear();
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "cpu", 3) != 0) { break; }
        if (line[3] == ' ')                     { continue; }

        CoreTick t{};
        std::sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
                    &t.user, &t.nice, &t.system, &t.idle,
                    &t.iowait, &t.irq, &t.softirq, &t.steal);
        out.push_back(t);
    }
    std::fclose(f);
    return !out.empty();
#endif
}

} // namespace cpu_monitor
