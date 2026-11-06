# ASSIGNMENT 02 — TEST RESULTS

## Task 1 — Sensor Monitoring System

- Platform: QEMU ARMv7, Linux 5.15
- Sampling interval: 500 ms
- Test duration: 60 seconds
- Samples read: 120
- Samples processed: 120
- Total alarms: 112
- Buffer overflow: 0
- Graceful shutdown: PASS

## Task 2 — Memory Management

- Valgrind duration: 30 seconds
- Samples processed: 60
- Memory errors: 0
- Memory leaks: 0 bytes
- Allocations: 17
- Frees: 17

| Time | VmRSS | VmSize |
|---|---|---|
| 5s | 2108 KB | 101104 KB |
| 30s | 2108 KB | 101104 KB |
| 55s | 2108 KB | 101104 KB |

## Task 3 — Debugging and Profiling

- GDB: All four worker threads identified
- strace: 1258 system calls over 30 seconds
- perf task-clock: 17.04 ms
- Context switches: 801
- CPU migrations: 39
- Page faults: 81

## Task 4 — Scheduling Jitter

Independent scheduling jitter, measured in microseconds:

| Scheduler | Min | Avg | Max | P99 |
|---|---|---|---|---|
| SCHED_OTHER | 43.514 | 122.771 | 958.024 | 527.412 |
| SCHED_FIFO | 5.655 | 56.340 | 420.718 | 303.909 |

Sensor Thread deadline lateness, measured in microseconds:

| Scheduler | Min | Avg | Max | P99 |
|---|---|---|---|---|
| SCHED_OTHER | 52.299 | 85763.008 | 178181.258 | 177072.563 |
| SCHED_FIFO | 1.543 | 797.840 | 7058.786 | 6889.558 |

Note: Sensor Thread measurements include sampling drift and CUSE read/retry delays. They must not be interpreted as pure scheduler jitter.
