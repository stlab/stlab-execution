/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/system_timer.hpp>
#include <stlab/concurrency/task.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace {
std::function<void()> during_preparation;
bool observe_allocation = false;
bool prepared = false;
bool invoked = false;
stlab::task<void() noexcept>* client_target = nullptr;
} // namespace

auto operator new(std::size_t size) -> void* {
    auto* pointer = std::malloc(std::max(size, std::size_t{1}));
    if (!pointer) throw std::bad_alloc();
    if (observe_allocation) {
        observe_allocation = false;
        during_preparation();
    }
    return pointer;
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

#if STLAB_TASK_SYSTEM(WINDOWS)
#include "../src/concurrency/system_timer_windows.cpp"
#else
namespace timer_test_std {
using namespace std;
namespace chrono {
using namespace std::chrono;
struct steady_clock : std::chrono::steady_clock {
    static auto now() noexcept -> time_point {
        if (during_preparation) {
            auto hook = std::move(during_preparation);
            during_preparation = nullptr;
            hook();
        }
        return std::chrono::steady_clock::now();
    }
};
} // namespace chrono
} // namespace timer_test_std
#define std timer_test_std
#include "../src/concurrency/system_timer_portable.cpp"
#undef std
#endif

int main() {
    std::set_terminate([] {
        if (client_target) (*client_target)();
        const bool passed = prepared && invoked && !std::current_exception();
        std::puts(passed ? "PASS: private timer preparation permits shutdown and retains the task" :
                           "FAIL: private timer preparation failed or consumed the task");
        std::fflush(stdout);
        std::_Exit(passed ? 0 : 1);
    });
#if STLAB_TASK_SYSTEM(WINDOWS)
    stlab::v2::windows_timers service;
#else
    stlab::v2::portable_timers service;
#endif
    during_preparation = [&] {
        std::promise<void> closed;
        auto ready = closed.get_future();
        std::thread closer([&] {
            service.close();
            closed.set_value();
        });
        if (ready.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
            std::fputs("FAIL: private timer preparation holds the admission mutex\n", stderr);
            std::_Exit(1);
        }
        closer.join();
        prepared = true;
    };
    stlab::task<void() noexcept> target = []() noexcept { invoked = true; };
    client_target = &target;
#if STLAB_TASK_SYSTEM(WINDOWS)
    observe_allocation = true;
#endif
    service.submit(target.relocation_concept(), target.relocation_invoke(),
                   target.relocation_source(), 0);
    std::fputs("FAIL: closed timer admission returned successfully\n", stderr);
    return 1;
}
