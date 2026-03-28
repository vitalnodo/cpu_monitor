# cpu_monitor

A command-line tool for monitoring per-core CPU load.
Be cautious as it contains AI slop. Thanks to Claude anyway.

---

## Building

Requires [Bazel](https://bazel.build/) 7+ with bzlmod enabled (default in Bazel 7+).

```bash
bazel build //:cpu_monitor
```

The binary is placed at `bazel-bin/cpu_monitor`.

### Requirements

| Component | Minimum |
|-----------|---------|
| Bazel     | 7.0 (bzlmod) |
| C++       | C++17   |
| Platform  | Linux, FreeBSD, maybe POSIX |

---

## Usage

```
cpu_monitor [OPTIONS]
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--reader=<method>` | `auto` | CPU reader backend (see below) |
| `--log-file=<path>` | *(disabled)* | File for periodic CSV load logs |
| `--log-interval=<secs>` | `5` | How often to write to the log file |
| `--help` | | Print help and exit |

### Reader methods

| Method | Platform | Accuracy | Description |
|--------|----------|----------|-------------|
| `auto` | any | best available | Picks the best reader for the current platform |
| `proc_stat` | Linux | ✅ exact | Reads `/proc/stat` kernel counters |
| `freebsd` | FreeBSD | ✅ exact | Reads `kern.cp_times` via `sysctl` |
| `idle_task` | POSIX | ⚠️ approximate | Idle-task heuristic (see [APPROACH.md](APPROACH.md)) |

### Examples

```bash
# Auto-detect (proc_stat on Linux, freebsd on FreeBSD, idle_task elsewhere)
./bazel-bin/cpu_monitor

# Force proc_stat reader
./bazel-bin/cpu_monitor --reader=proc_stat

# Log to file every 10 seconds
./bazel-bin/cpu_monitor --log-file=cpu.log --log-interval=10

# Combine options
./bazel-bin/cpu_monitor --reader=proc_stat --log-file=cpu.log --log-interval=30
```

---

## Interactive commands

Once running, the tool reads commands from stdin:

| Command | Description |
|---------|-------------|
| `p` | Print load for all cores |
| `p <N>` | Print load for core N (0-based) |
| `q` | Quit |
| `Ctrl+D` | Quit (EOF) |
| `Ctrl+C` | Quit (signal) |

### Example session

```
$ ./bazel-bin/cpu_monitor
[ProcStatReader] detected 8 cores
[Init] reader: proc_stat, cores: 8, log: disabled (every 5s)
[Run] Commands:
  p        - print all cores
  p <N>    - print core N (0-based)
  q / ^C   - quit

p
--- CPU Load ---
  Core  0:  12%
  Core  1:   4%
  Core  2:  67%
  Core  3:   3%
  Core  4:   5%
  Core  5:   2%
  Core  6:   8%
  Core  7:   1%
----------------
p 2
  Core  2:  71%
q
[Run] Exiting.
```

---

## Log file format

When `--log-file` is specified, one CSV line is written every `--log-interval` seconds:

```
<unix_timestamp>,<core0_%>,<core1_%>,...
```

Example (`cpu.log`):

```
1718000000,12,4,67,3,5,2,8,1
1718000010,15,3,72,4,6,1,9,2
```

---

## Project structure

```
cpu_monitor/
├── main.cc                  Entry point, CLI argument parsing
├── MODULE.bazel             Bazel bzlmod config
├── app/
│   ├── App.h / App.cc       State machine (Init → Run), stdin select() loop
│   └── BUILD
├── cpu/
│   ├── ICpuReader.h         Pure abstract interface
│   ├── ProcStatReader       Linux /proc/stat reader
│   ├── FreeBsdReader        FreeBSD sysctl kern.cp_times reader
│   ├── IdleTaskReader       POSIX idle-task heuristic
│   ├── UnsupportedReader    Stub that fails fast with a clear message
│   ├── CpuReaderFactory     Factory: auto-selects or creates by method
│   ├── ThreadAffinity.h     Cross-platform thread-to-core pinning
│   └── BUILD
├── io/
│   ├── FileLogger.h/.cc     RAII periodic CSV file writer
│   └── BUILD
├── README.md                This file
├── APPROACH.md              Why per-core measurement is platform-specific
└── decisions.md             Architecture and implementation decisions
```

---

## Platform notes

### Linux

Uses `/proc/stat` by default — the same source as `top` and `htop`.
Accurate, zero overhead.

Thread affinity for `idle_task` reader uses:
- `pthread_setaffinity_np` on glibc
- `sched_setaffinity(0, ...)` on musl (Alpine, Buildroot, Yocto with musl)

### FreeBSD

Uses `sysctl kern.cp_times` — kernel counters, same accuracy as Linux `/proc/stat`.

### Other POSIX

Falls back to `idle_task` heuristic. Readings are approximate —
see [APPROACH.md](APPROACH.md) for a detailed explanation of limitations.

---

## Further reading

- [APPROACH.md](APPROACH.md) — why per-core CPU measurement is inherently
  platform-specific, comparison of all approaches, and known limitations
  of the idle-task heuristic
- [decisions.md](decisions.md) — architecture and implementation decisions
  with justifications
