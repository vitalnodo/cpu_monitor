load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "icpu_reader",
    hdrs = ["ICpuReader.h"],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "thread_affinity",
    hdrs = ["ThreadAffinity.h"],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "proc_stat_reader",
    srcs = ["ProcStatReader.cc"],
    hdrs = ["ProcStatReader.h"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    deps = [":icpu_reader"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "freebsd_reader",
    srcs = ["FreeBsdReader.cc"],
    hdrs = ["FreeBsdReader.h"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    deps = [":icpu_reader"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "idle_task_reader",
    srcs = ["IdleTaskReader.cc"],
    hdrs = ["IdleTaskReader.h"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    linkopts = ["-lpthread"],
    deps = [
        ":icpu_reader",
        ":thread_affinity",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "unsupported_reader",
    hdrs = ["UnsupportedReader.h"],
    copts = ["-std=c++17"],
    deps = [":icpu_reader"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "cpu_reader_factory",
    srcs = ["CpuReaderFactory.cc"],
    hdrs = ["CpuReaderFactory.h"],
    copts = ["-std=c++17", "-Wall", "-Wextra"],
    deps = [
        ":icpu_reader",
        ":proc_stat_reader",
        ":freebsd_reader",
        ":idle_task_reader",
        ":unsupported_reader",
    ],
    visibility = ["//visibility:public"],
)
