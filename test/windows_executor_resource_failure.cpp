/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/task.hpp>
#include <stlab/execution/config.hpp>

#include "../src/concurrency/detail/core_shutdown.hpp"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace {
bool failure_injected = false;

auto WINAPI fail_work_allocation(PTP_WORK_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON) -> PTP_WORK {
    failure_injected = true;
    return nullptr;
}

void WINAPI reject_work_submission(PTP_WORK) {
    std::fputs("FAIL: submitted work after allocation failure\n", stderr);
    std::_Exit(1);
}
} // namespace

// Exercise the real release-mode scheduler without passing an invalid handle to Windows.
#define CreateThreadpoolWork fail_work_allocation
#define SubmitThreadpoolWork reject_work_submission
#include "../src/concurrency/executor_abi.cpp"
#undef SubmitThreadpoolWork
#undef CreateThreadpoolWork

int main() {
    std::set_terminate([] {
        if (!failure_injected) {
            std::fputs("FAIL: terminated before work allocation failure\n", stderr);
            std::_Exit(1);
        }
        std::puts("PASS: work allocation failure terminated before submission");
        std::fflush(stdout);
        std::_Exit(0);
    });
    stlab::default_executor([]() noexcept {
        std::fputs("FAIL: executed task after work allocation failure\n", stderr);
        std::_Exit(1);
    });
    std::fputs("FAIL: scheduling returned after allocation failure\n", stderr);
    return 1;
}
