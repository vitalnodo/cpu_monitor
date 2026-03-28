#include "app/App.h"
#include "cpu/CpuReaderFactory.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

static void printUsage(const std::string& argv0) {
    std::cerr << "Usage: " << argv0 << " [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  --reader=<method>       CPU reader: auto | proc_stat | freebsd | idle_task\n"
              << "                          Default: auto\n"
              << "  --log-file=<path>       File for periodic CPU load logs\n"
              << "  --log-interval=<secs>   Logging interval in seconds (default: 5)\n"
              << "  --help                  Show this help and exit\n"
              << "\n"
              << "Run-time commands (stdin):\n"
              << "  p          Print load for all cores\n"
              << "  p <N>      Print load for core N (0-based)\n"
              << "  q / Ctrl+C Quit\n";
}

static bool parseArg(const std::string& arg, const std::string& prefix, std::string& out) {
    if (arg.size() > prefix.size() && arg.substr(0, prefix.size()) == prefix) {
        out = arg.substr(prefix.size());
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    cpu_monitor::ReaderMethod method = cpu_monitor::ReaderMethod::Auto;
    cpu_monitor::AppConfig    config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]};
        std::string val;

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }

        if (parseArg(arg, "--reader=", val)) {
            try {
                method = cpu_monitor::readerMethodFromString(val);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error: " << e.what() << '\n';
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            continue;
        }

        if (parseArg(arg, "--log-file=", val)) {
            config.logFile = val;
            continue;
        }

        if (parseArg(arg, "--log-interval=", val)) {
            try {
                const int secs = std::stoi(val);
                if (secs <= 0) { throw std::out_of_range{"must be > 0"}; }
                config.logIntervalSec = static_cast<unsigned int>(secs);
            } catch (...) {
                std::cerr << "Error: --log-interval must be a positive integer\n";
                return EXIT_FAILURE;
            }
            continue;
        }

        std::cerr << "Error: unknown argument '" << arg << "'\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    try {
        auto reader = cpu_monitor::CpuReaderFactory::create(method);
        cpu_monitor::App app{std::move(reader), std::move(config)};
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
