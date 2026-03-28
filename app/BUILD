load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "app",
    srcs = ["App.cc"],
    hdrs = ["App.h"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    deps = [
        "//cpu:icpu_reader",
        "//io:file_logger",
    ],
    visibility = ["//visibility:public"],
)
