---
title: "System Timer Architecture and Guarantees"
entity_type: concept
confidence: 0.75
created: 2026-10-13T00:39:14Z
last_accessed: 2026-10-13T00:40:36Z
last_reinforced: 2026-10-13T00:39:14Z
access_count: 2
sources:
  - "include/stlab/concurrency/system_timer.hpp:50-80"
  - "src/concurrency/detail/timer_common.hpp:25-70"
  - "src/concurrency/system_timer_portable.cpp:90-125"
relationships:
  - type: relates_to
    target: "Process Lifecycle and Pre-Exit Protocol"
  - type: depends_on
    target: "Task Type Erasure and Relocation ABI"
tags: ["system-timer", "timer-status", "stlab-v2-system-timer-submit", "timer-delay", "portable-timers"]
tier: knowledge
source: inferred
---

# System Timer Architecture and Guarantees

The system timer service (`include/stlab/concurrency/system_timer.hpp`) schedules asynchronous tasks for future execution with explicit duration guarantees.

Key invariants and operational rules:
1. Non-early execution guarantee: Positive delays round upward in nanoseconds (`timer_integral_nanoseconds()`) and are guaranteed never to fire prematurely. Non-positive delays are treated as asynchronous zero-delay submissions.
2. ABI-level error protocol: `stlab_v2_system_timer_submit` returns standard-layout `stlab_v2_timer_status` (0=success, 1=allocation failure, 2=generic_category, 3=system_category). Submission failure leaves the source task unconsumed; successful submission assumes exactly-once execution or cancellation.
3. Teardown & cancellation: System timer registers with `core_shutdown` on first use. During `pre_exit()`, pending timers are canceled immediately and their captured state is destroyed; running timer callbacks are waited on (joined) before executor drain begins.
4. Threaded restriction: Timer callbacks and their capture destructors must never invoke `pre_exit()`.

Aliases / also known as: system_timer, timer_status, stlab_v2_system_timer_submit, timer_delay, portable_timers
