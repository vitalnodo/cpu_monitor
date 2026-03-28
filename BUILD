load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "cpu_monitor",
    srcs = ["main.cc"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    deps = [
        "//app:app",
        "//cpu:cpu_reader_factory",
    ],
)
