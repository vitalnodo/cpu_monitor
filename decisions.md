# Design Decisions

## Overview

```
main.cc
  └── CpuReaderFactory::create()  →  unique_ptr<ICpuReader>
  └── App(reader, config)
        ├── doInit()   ← all allocations happen here
        └── doRun()    ← select() loop, zero allocations
```

---

## 1. Two-state architecture: Init → Run

**Decision:** `App::run()` calls `doInit()` then `doRun()` sequentially.

**Why:**
The requirement explicitly separates Init and Run states. A simple
sequential call is the most direct expression — no threads, no state
enum, no condition variables needed at the state-machine level.

All heap allocations (`samples_` vector, `FileLogger`, `ICpuReader::init()`)
happen in `doInit()`. Once `doRun()` starts, no further allocations occur.

---

## 2. Memory allocation policy

| Object | Where allocated | Why |
|--------|----------------|-----|
| `samples_` (`vector<uint8_t>`) | `doInit()` via `resize()` | Fixed size after init, written in-place in Run |
| `FileLogger` | `doInit()` via `make_unique` | Opens file descriptor once |
| `ICpuReader` | before `App` constructor | Factory creates it, App takes ownership |
| `ProcStatReader::cur_` | `init()` via `resize()` | Avoids per-call allocation in `read()` |
| `FreeBsdReader::rawBuf_` | `init()` via `resize()` | Pre-sized sysctl buffer |
| `IdleTaskReader::workers_` | `init()` via `reserve()` + `emplace_back` | One `CoreWorker` per core |
| stdin line buffer | stack (`char[64]`) | Never needs more than one line |

---

## 3. Reader selection: factory + priority chain

**Decision:** `CpuReaderFactory::create(ReaderMethod)` with compile-time platform detection.

**Priority for `auto`:**
```
__linux__       → ProcStatReader    (kernel counters, most accurate)
__FreeBSD__     → FreeBsdReader     (kernel counters, most accurate)
_POSIX_THREADS  → IdleTaskReader    (heuristic fallback)
else            → UnsupportedReader (fails at init() with a clear message)
```

The reader can be overridden at runtime via `--reader=` which is useful for:
- testing `idle_task` on Linux and comparing against `proc_stat`
- using `idle_task` on Linux when `/proc/stat` is unavailable.

See [APPROACH.md](APPROACH.md) for a detailed explanation of why each
platform needs a different approach.

---

## 4. stdin handling: `select()` with timeout

**Decision:** `doRun()` uses a single `select()` call with `tv = logIntervalSec`.

**Why not blocking `fgets`:**
A blocking read would stall the file logger — the process would only
write to the log file when the user typed something.

**Why not a separate timer thread:**
`select()` with a timeout handles both stdin and the timer in one
syscall, with no additional threads, no signals, and no `timerfd_create`.

**Special case — logging disabled:**
When `logFile` is empty, `nullptr` is passed as the timeout pointer.
`select()` then blocks indefinitely until stdin is readable, with no
spurious wakeups.

```
logger present  →  select(stdin, timeout=logIntervalSec)
logger absent   →  select(stdin, timeout=nullptr)   // block forever
```

---

## 5. CPU load type: `uint8_t` [0..100]

**Decision:** Integer percentage stored as `uint8_t`.

**Why:**
- 0–100 fits in 7 bits; `uint8_t` is the natural fit
- Integer percentages are what every system monitor displays
- Avoids floating-point in the hot path and in output formatting
- `atomic<uint8_t>` in `CoreWorker` is lock-free on all mainstream architectures

---

## 6. `FileLogger`: separate RAII class

**Decision:** Standalone class owning one `FILE*`, not inlined into `App`.

**Why separate:**
Single Responsibility — `App` runs the state machine, `FileLogger`
manages one file descriptor. They can be tested and replaced independently.

**Why `FILE*` and `fprintf` instead of `std::ofstream`:**
`write()` is called in the Run state and must not allocate.
`std::ofstream` with `<<` can allocate internally (locale, sentry).
`fprintf` into an already-open `FILE*` does not.

---

## 7. `ThreadAffinity`: header-only utility struct

**Decision:** `ThreadAffinity::setAffinity(coreId)` as a `static` method
in a header-only `struct` with `= delete` constructor.

**Why header-only:**
Contains only `#ifdef` branches and a single function — no translation
unit needed. Keeps platform-specific includes isolated from the rest of
the codebase.

**Platform coverage:**

| Platform | API | Notes |
|----------|-----|-------|
| Linux / glibc | `pthread_setaffinity_np` | GNU extension |
| Linux / musl | `sched_setaffinity(0, ...)` | Raw syscall; musl lacks the pthread variant |
| FreeBSD | `pthread_setaffinity_np` | Via `<pthread_np.h>` |
| Other | `return false` | Non-fatal — reader still works, less accurate |

**musl detection:**
musl does not define any public identification macro.
Detection is via `!defined(__GLIBC__)` on Linux — a common convention
in embedded Linux projects (Buildroot, Yocto with musl).

---

## 8. `IdleTaskReader`: worker thread owns the measurement cycle

**Decision:** `idleLoop()` runs the tight counter loop and updates `load_`
atomically. `currentLoad()` is a single `atomic::load` — never blocks.

**Why the worker owns the cycle:**
An earlier design had `currentLoad()` calling `nanosleep()` internally.
This meant `read()` in `App` blocked for `N_cores × SAMPLE_MS` — 800 ms
for 4 cores, 3.2 s for 16 cores. Moving the sleep into the worker thread
makes `read()` O(N) atomic loads regardless of core count.

**Calibration:**
`idleLoop()` runs `WARMUP_SAMPLES` (3) measurement windows before entering
the continuous phase. `calibrate()` on the main thread simply waits long
enough for this to complete, then checks `load_` for sanity.

---

## 9. CLI argument format

**Decision:** `--key=value` parsed manually, no `getopt` / external libraries.

**Why:**
- Zero external dependencies beyond POSIX
- Only 3 flags — a manual prefix-match loop is adequate
- `getopt_long` availability and behaviour varies across platforms

---

## 10. Build system: Bazel 9 / bzlmod

**Decision:** One `cc_library` per logical module, `MODULE.bazel` with `rules_cc`.

**Why fine-grained targets:**
Only changed translation units are recompiled. `linkopts = ["-lpthread"]`
is scoped to `//cpu:idle_task_reader` — targets that do not use threads
are not linked against pthreads.

**Why bzlmod over WORKSPACE:**
`WORKSPACE` is deprecated in Bazel 7+ and produces warnings in Bazel 9.
`MODULE.bazel` with `bazel_dep` is the current recommended approach.
