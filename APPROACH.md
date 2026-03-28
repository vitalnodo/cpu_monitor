# CPU Load Measurement — Approach & Trade-offs

This document explains *why* per-core CPU load measurement is inherently
platform-specific, what approaches exist, and what trade-offs each carries.

---

## Why there is no portable per-core CPU load API

POSIX defines process and thread CPU *time* (`clock_gettime`, `CLOCK_PROCESS_CPUTIME_ID`,
`CLOCK_THREAD_CPUTIME_ID`), but it does **not** define any API to query
system-wide per-core utilisation. Every OS exposes this differently:

| Platform | Kernel interface       |
|----------|------------------------|
| Linux    | `/proc/stat`           |
| FreeBSD  | `sysctl kern.cp_times` |
| macOS    | `host_processor_info()` (Mach) |
| Solaris  | `kstat_read()`         |

None of these are POSIX. A truly portable solution must either use
platform-specific code paths or fall back to a heuristic.

---

## Approach 1: Kernel counters (Linux `/proc/stat`, FreeBSD `sysctl`)

The kernel maintains per-core tick counters split by category
(user, nice, system, idle, iowait, irq, …). Reading them twice with
a known interval gives an accurate delta:

```
load = (delta_active / delta_total) * 100

active = user + nice + system + irq + softirq + steal
total  = active + idle + iowait
```

**Pros:**
- Ground truth — same data that `top`/`htop` use
- Zero CPU overhead
- No timing jitter

**Cons:**
- Platform-specific interface per OS family
- Not available on POSIX-only environments (e.g. some embedded RTOSes)

---

## Approach 2: Idle-task heuristic

Inspired by a common RTOS pattern:
> *"CPU load = time the CPU is NOT running the idle task."*

One low-priority thread per core acts as a synthetic idle task.
It increments a counter in a tight loop. The fewer iterations it
completes in a fixed window, the busier the core was.

```
load = (1 - delta_iterations / baseline_iterations) * 100
```

This requires **two independent mechanisms**, both platform-specific:

### 2a. Thread affinity (pinning to a core)

Without affinity, the idle thread may migrate between cores at any time.
If it migrates, it measures a mixture of multiple cores — not one.
`nice` / `setpriority` does **not** help here: priority affects *when*
a thread runs, not *where*. Affinity and priority are orthogonal.

| Platform      | Affinity API                   | Notes                              |
|---------------|--------------------------------|------------------------------------|
| Linux/glibc   | `pthread_setaffinity_np`       | GNU extension, not in POSIX        |
| Linux/musl    | `sched_setaffinity(0, ...)`    | Raw syscall; musl has no pthread_setaffinity_np |
| FreeBSD       | `pthread_setaffinity_np`       | Via `<pthread_np.h>`               |
| macOS         | `thread_policy_set` (AFFINITY) | Hint only — kernel may ignore it   |
| Solaris       | `processor_bind`               | Hard pin                           |
| POSIX-only    | *(not available)*              | Measurement becomes system-average |

Without a working affinity API the idle-task approach still compiles
and runs, but it measures aggregate system load rather than per-core load.

### 2b. Thread priority (being preempted by real work)

The idle thread must be preempted as soon as any real work arrives on
the core. Otherwise it keeps running alongside real tasks and
underestimates load.

| Mechanism              | Availability     | Behaviour                                      |
|------------------------|------------------|------------------------------------------------|
| `SCHED_IDLE`           | Linux only       | Below any `SCHED_OTHER`/nice level; best choice|
| `setpriority` nice +19 | POSIX            | Lowest normal priority; CFS still gives it a   |
|                        |                  | minimum time slice — may underestimate at 100% |
| Nothing                | always           | Shares time equally; completely wrong results  |

Note: even `nice +19` is not equivalent to an RTOS idle task.
Under Linux CFS, `min_granularity_ns` guarantees every runnable thread
gets *some* CPU time. At 100% load the idle thread still runs briefly,
so measured load may read ~85–95% instead of 100%.
`SCHED_IDLE` eliminates this — it is truly starved by `SCHED_OTHER`.

**Summary:** for correct per-core idle-task measurement you need
*both* a working affinity API *and* `SCHED_IDLE` (or equivalent).
On platforms where either is missing the measurement degrades gracefully
but becomes less accurate.

---

## What this application does

```
Platform detection (compile-time):

  __linux__    → ProcStatReader    (accurate, kernel counters)
  __FreeBSD__  → FreeBsdReader     (accurate, kernel counters)
  _POSIX_THREADS → IdleTaskReader  (heuristic, see caveats above)
  else         → UnsupportedReader (fails at init() with a clear message)
```

The reader can also be forced at runtime via `--reader=` flag, which
allows testing the idle-task heuristic on Linux and comparing results
against `/proc/stat` for validation.

### IdleTaskReader behaviour by platform

| Platform       | Affinity       | Priority      | Per-core accuracy |
|----------------|---------------|---------------|-------------------|
| Linux/glibc    | `sched_setaffinity` (via `pthread_setaffinity_np`) | `SCHED_IDLE` → `nice +19` | Good |
| Linux/musl     | `sched_setaffinity(0,...)` | `SCHED_IDLE` → `nice +19` | Good |
| FreeBSD        | `pthread_setaffinity_np` | `nice +19` (no SCHED_IDLE) | Moderate |
| Other POSIX    | none          | `nice +19`    | System-average only |

---

## Known limitations of IdleTaskReader

1. **Baseline drift**: baseline is measured once at startup. If the system
   is already under load during Init, baseline is underestimated and all
   subsequent readings will show less load than reality.

2. **Short bursts**: the measurement window is 200 ms. Bursts shorter than
   this window are averaged out and may not be visible.

3. **CFS min_granularity**: on Linux with `nice +19`, at sustained 100%
   load the idle thread still receives a small time slice. Reported load
   may saturate at ~90–95% rather than 100%. Use `SCHED_IDLE` (requires
   no special privileges on modern kernels) for better saturation.

4. **Counter overhead**: `counter_` is a plain `unsigned long long`
   accessed only from the worker thread — no atomic needed, no cache
   traffic. `load_` is `std::atomic<uint8_t>` written by worker and
   read by main thread — intentionally cache-line separated via
   struct layout to avoid false sharing.

---

## IdleTaskReader — accuracy in practice

### The self-loading problem

The idle thread runs a tight counter-increment loop. From the kernel's
perspective this is real CPU work — `/proc/stat` will show the core as
heavily loaded even when the system is otherwise idle. The thread
consumes the very resource it is trying to measure.

This is fundamentally different from an RTOS idle task, which executes
a `WFI` / `HLT` instruction and surrenders the core to the hardware
until an interrupt arrives. A Linux userspace thread cannot do this —
it is always "runnable" and always burns cycles when scheduled.

`SCHED_IDLE` reduces but does not eliminate this effect. The thread
still executes `fetch_add` millions of times per second when it gets
CPU time.

### What this means for readings

Because the idle thread itself consumes cycles, the baseline measured
at calibration is lower than true 100%-idle would be. All subsequent
load readings are therefore **systematically underestimated**:

```
True load:        0%  →  50%  →  100%
IdleTaskReader:   0%  →  ~30% →  ~70%   (approximate, varies by system)
```

The absolute values are not reliable. However the **relative trend**
is preserved — a spike in real load produces a corresponding spike
in the reading, making it useful as a coarse indicator.

### When IdleTaskReader is acceptable

- No kernel counter API is available (non-Linux, non-FreeBSD POSIX)
- You need to detect *whether* load is rising, not its exact value
- The platform supports thread affinity (otherwise it is not per-core at all)
- Readings are interpreted as trends, not absolute percentages

### When IdleTaskReader should not be used

- Accurate per-core utilisation figures are required
- The system is already under load at startup (baseline will be wrong)
- Load events are shorter than SAMPLE_MS (200 ms) — they will be missed
- Thread affinity is unavailable — measurement becomes system-wide average

### Possible improvement (not implemented)

Replacing `fetch_add` with `sched_yield()` in the inner loop would
reduce self-loading: the thread voluntarily gives up its time slice
each iteration, so it runs less aggressively. The counter would increment
far fewer times, but the relative delta would still reflect available
idle time. This does not fully solve the problem but narrows the gap
between reported and true load.

On Linux specifically, using `/proc/stat` via `--reader=proc_stat` is
always the correct choice. `IdleTaskReader` exists as a last resort
for platforms where no kernel counter interface is available.
