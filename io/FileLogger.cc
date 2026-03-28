#include "io/FileLogger.h"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>

namespace cpu_monitor {

FileLogger::FileLogger(const std::string& path) {
    file_ = std::fopen(path.c_str(), "a");
    if (!file_) {
        throw std::runtime_error{
            std::string{"FileLogger: cannot open '"} + path
            + "': " + std::strerror(errno)};
    }
}

FileLogger::~FileLogger() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

// write() must not allocate — uses fprintf directly into the open FILE*
void FileLogger::write(const uint8_t* loads, unsigned int count) {
    if (!file_ || count == 0) { return; }

    const std::time_t now = std::time(nullptr);
    std::fprintf(file_, "%lld", static_cast<long long>(now));
    for (unsigned int i = 0; i < count; ++i) {
        std::fprintf(file_, ",%u", static_cast<unsigned int>(loads[i]));
    }
    std::fputc('\n', file_);
    std::fflush(file_);
}

} // namespace cpu_monitor
