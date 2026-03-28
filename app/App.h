#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <vector>

#include "cpu/ICpuReader.h"
#include "io/FileLogger.h"

namespace cpu_monitor {

struct AppConfig {
    std::string  logFile;
    unsigned int logIntervalSec{5};
};

class App {
public:
    explicit App(std::unique_ptr<ICpuReader> reader, AppConfig config);

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

    ~App() = default;

    void run();

private:
    void doInit();
    void doRun();
    void handleCommand(const char* line);
    void printAllCores()  const;
    void printCore(unsigned int coreIndex) const;

    std::unique_ptr<ICpuReader> reader_;
    AppConfig                   config_;
    std::unique_ptr<FileLogger> logger_;

    unsigned int         numCores_{0};
    std::vector<uint8_t> samples_;
    bool                 quit_{false};
};

} // namespace cpu_monitor
