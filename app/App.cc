#include "app/App.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

// POSIX
#include <sys/select.h>
#include <unistd.h>

namespace cpu_monitor {

App::App(std::unique_ptr<ICpuReader> reader, AppConfig config)
    : reader_(std::move(reader))
    , config_(std::move(config))
{}

void App::run() {
    doInit();
    doRun();
}

void App::doInit() {
    reader_->init();

    numCores_ = reader_->coreCount();
    if (numCores_ == 0) {
        throw std::runtime_error{"ICpuReader reported 0 cores after init"};
    }

    samples_.resize(numCores_, 0u);

    if (!config_.logFile.empty()) {
        logger_ = std::make_unique<FileLogger>(config_.logFile);
    }

    std::cout << "[Init] reader: "  << reader_->name()
              << ", cores: "        << numCores_
              << ", log: "          << (config_.logFile.empty()
                                           ? std::string{"disabled"}
                                           : config_.logFile)
              << " (every "         << config_.logIntervalSec << "s)\n";
}

void App::doRun() {
    std::cout << "[Run] Commands:\n"
              << "  p        - print all cores\n"
              << "  p <N>    - print core N (0-based)\n"
              << "  q / ^C   - quit\n\n";

    static constexpr int kLineBuf = 64;
    char line[kLineBuf];

    while (!quit_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval tv{};
        tv.tv_sec  = static_cast<long>(config_.logIntervalSec);
        tv.tv_usec = 0;

        const int ready = ::select(STDIN_FILENO + 1, &readfds, nullptr, nullptr,
                                   logger_ ? &tv : nullptr);

        if (ready < 0) {
            if (errno == EINTR) { continue; }
            std::cerr << "select: " << std::strerror(errno) << '\n';
            break;
        }

        if (ready == 0) {
            reader_->read(samples_.data(), numCores_);
            if (logger_) { logger_->write(samples_.data(), numCores_); }
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (std::fgets(line, kLineBuf, stdin) == nullptr) {
                quit_ = true;
                break;
            }
            handleCommand(line);
        }
    }

    std::cout << "[Run] Exiting.\n";
}

void App::handleCommand(const char* line) {
    while (*line == ' ' || *line == '\t') { ++line; }

    if (line[0] == 'q' || line[0] == 'Q') {
        quit_ = true;
        return;
    }

    if (line[0] == 'p' || line[0] == 'P') {
        const char* rest = line + 1;
        while (*rest == ' ' || *rest == '\t') { ++rest; }

        if (*rest == '\0' || *rest == '\n' || *rest == '\r') {
            reader_->read(samples_.data(), numCores_);
            printAllCores();
        } else {
            char* end{};
            const long idx = std::strtol(rest, &end, 10);
            if (end == rest || idx < 0
                    || static_cast<unsigned int>(idx) >= numCores_) {
                std::cout << "  Invalid core index. Valid range: 0.."
                          << (numCores_ - 1u) << '\n';
                return;
            }
            reader_->read(samples_.data(), numCores_);
            printCore(static_cast<unsigned int>(idx));
        }
        return;
    }

    if (line[0] != '\n' && line[0] != '\r' && line[0] != '\0') {
        std::cout << "  Unknown command. Use: p | p <N> | q\n";
    }
}

void App::printAllCores() const {
    std::cout << "--- CPU Load ---\n";
    for (unsigned int i = 0; i < numCores_; ++i) {
        std::cout << "  Core " << i << ": "
                  << static_cast<unsigned int>(samples_[i]) << "%\n";
    }
    std::cout << "----------------\n";
}

void App::printCore(unsigned int coreIndex) const {
    assert(coreIndex < numCores_);
    std::cout << "  Core " << coreIndex << ": "
              << static_cast<unsigned int>(samples_[coreIndex]) << "%\n";
}

} // namespace cpu_monitor
