/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../src/concurrency/detail/core_shutdown.hpp"
#include "../src/concurrency/detail/main_task_queue.hpp"
#include "../src/concurrency/detail/waiter_state.hpp"
#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/set_current_thread_name.hpp>
#include <stlab/pre_exit.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace allocation_test {
std::atomic<unsigned> count{0};
}

auto operator new(std::size_t size) -> void* {
    if (auto* pointer = std::malloc(std::max(size, std::size_t{1}))) {
        allocation_test::count.fetch_add(1, std::memory_order_relaxed);
        return pointer;
    }
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

// Include the real implementation with test-only scheduling observations. Standard and public
// headers are already included, so the substitutions affect only executor_abi.cpp's internals.
namespace portable_test_std {
using namespace std;
inline unsigned reported_hardware = 2;
inline std::atomic<unsigned> sleepers{0};
inline std::function<void()> before_shutdown;

class thread {
    std::thread _thread;

public:
    template <class F>
    explicit thread(F&& f) : _thread(std::forward<F>(f)) {}
    thread(thread&&) noexcept = default;
    auto operator=(thread&&) noexcept -> thread& = default;
    static auto hardware_concurrency() noexcept -> unsigned { return reported_hardware; }
    void join() {
        if (before_shutdown) {
            auto hook = std::move(before_shutdown);
            before_shutdown = nullptr;
            hook();
        }
        _thread.join();
    }
};

class condition_variable : public std::condition_variable {
public:
    void wait(std::unique_lock<std::mutex>& lock) {
        sleepers.fetch_add(1, std::memory_order_release);
        std::condition_variable::wait(lock);
        sleepers.fetch_sub(1, std::memory_order_release);
    }

    template <class Predicate>
    void wait(std::unique_lock<std::mutex>& lock, Predicate predicate) {
        while (!predicate()) {
            if (before_shutdown) {
                auto hook = std::move(before_shutdown);
                before_shutdown = nullptr;
                lock.unlock();
                hook();
                lock.lock();
                continue;
            }
            wait(lock);
        }
    }
};
} // namespace portable_test_std

#undef STLAB_TASK_POOL_MAXIMUM
#if defined(PORTABLE_TEST_UNLIMITED)
#define STLAB_TASK_POOL_MAXIMUM() 0u
#else
#define STLAB_TASK_POOL_MAXIMUM() 4u
#endif
#define std portable_test_std
#ifndef PORTABLE_TEST_EXECUTOR_SOURCE
#define PORTABLE_TEST_EXECUTOR_SOURCE "../src/concurrency/executor_abi.cpp"
#endif
#include PORTABLE_TEST_EXECUTOR_SOURCE
#undef std

namespace {
using namespace std::chrono_literals;

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

template <class Predicate>
void await(Predicate predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) fail(message);
        std::this_thread::yield();
    }
}

void expansion_wake() {
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<bool> entered{false};
    stlab::default_executor([&]() noexcept {
        stlab::stlab_v2_notify_default_executor_before_waiting();
        entered.store(true, std::memory_order_release);
        released.wait();
    });
    await([&] { return entered.load(std::memory_order_acquire); },
          "initial worker did not enter its blocking task");
    await([] { return portable_test_std::sleepers.load(std::memory_order_acquire) == 1; },
          "expansion worker did not become idle");

    std::atomic<unsigned> completed{0};
    for (auto priority : {stlab::execution_detail::executor_priority::high,
                          stlab::execution_detail::executor_priority::medium,
                          stlab::execution_detail::executor_priority::low}) {
        stlab::task<void() noexcept> task{[&]() noexcept { ++completed; }};
        stlab::execution_detail::submit_executor_proc(priority, task);
        await([&] { return completed.load() != 0; },
              "submission did not wake the idle expansion worker");
        completed.store(0);
        await([] { return portable_test_std::sleepers.load(std::memory_order_acquire) == 1; },
              "expansion worker did not return to idle");
    }
    release.set_value();
    stlab::pre_exit();
}

void expansion_during_join() {
    std::set_terminate([] { fail("shutdown left an expansion thread unjoined"); });
    std::promise<void> continue_task;
    auto continuation = continue_task.get_future().share();
    std::atomic<bool> entered{false};
    std::atomic<bool> notified{false};
    stlab::default_executor([&]() noexcept {
        entered.store(true, std::memory_order_release);
        continuation.wait();
        stlab::stlab_v2_notify_default_executor_before_waiting();
        notified.store(true, std::memory_order_release);
    });
    await([&] { return entered.load(std::memory_order_acquire); }, "worker did not start");
    portable_test_std::before_shutdown = [&] {
        continue_task.set_value();
        await([&] { return notified.load(std::memory_order_acquire); },
              "blocking notification stalled during shutdown");
    };
    stlab::pre_exit();
}

void descendant_wait(bool capture_cleanup) {
    std::set_terminate([] { fail("shutdown left an expansion thread unjoined"); });
    std::promise<void> continue_task;
    auto continuation = continue_task.get_future().share();
    std::atomic<bool> entered{false};
    std::atomic<unsigned> completed{0};
    auto descendants = [&]() noexcept {
        for (auto priority : {stlab::execution_detail::executor_priority::high,
                              stlab::execution_detail::executor_priority::medium,
                              stlab::execution_detail::executor_priority::low}) {
            std::promise<void> child;
            auto ready = child.get_future();
            stlab::task<void() noexcept> task{[&]() noexcept { child.set_value(); }};
            stlab::execution_detail::submit_executor_proc(priority, task);
            stlab::stlab_v2_notify_default_executor_before_waiting();
            if (ready.wait_for(5s) != std::future_status::ready)
                fail("accepted callback's descendant stalled during shutdown");
            ++completed;
        }
        std::puts("OBSERVED: all three descendant waits completed during shutdown");
        std::fflush(stdout);
    };
    auto capture = std::shared_ptr<int>{new int, [&](int* pointer) noexcept {
                                            delete pointer;
                                            if (capture_cleanup) descendants();
                                        }};
    stlab::default_executor([&, capture]() noexcept {
        entered.store(true, std::memory_order_release);
        continuation.wait();
        if (!capture_cleanup) descendants();
    });
    capture.reset();
    await([&] { return entered.load(std::memory_order_acquire); }, "worker did not start");
    portable_test_std::before_shutdown = [&] { continue_task.set_value(); };
    stlab::pre_exit();
    if (completed.load() != 3) fail("shutdown did not drain every descendant");
}

void hardware_zero() {
    portable_test_std::reported_hardware = 0;
    if (stlab::execution_detail::portable_hardware_concurrency() != 1)
        fail("zero hardware concurrency was not normalized to one");
    if (stlab::execution_detail::executor_shard_count() != 1)
        fail("zero hardware concurrency did not retain one shard");
    std::atomic<bool> completed{false};
    stlab::default_executor([&]() noexcept { completed.store(true, std::memory_order_release); });
    await([&] { return completed.load(std::memory_order_acquire); },
          "zero hardware concurrency could not execute a task");
    stlab::pre_exit();
}

struct destructor_submission {
    bool* armed;
    bool* submitted;
    void* queue;
    void (*submit)(void*);

    void operator()() const noexcept {}
    ~destructor_submission() {
        if (*armed && !std::exchange(*submitted, true)) submit(queue);
    }
};

void queue_reentry(bool main_queue) {
    for (bool waiting_pop : {false, true}) {
        bool armed = false;
        bool submitted = false;
        if (main_queue) {
            stlab::execution_detail::main_task_queue queue;
            stlab::task<void() noexcept> source{destructor_submission{
                &armed, &submitted, &queue, [](void* value) {
                    stlab::task<void() noexcept> child{[]() noexcept {}};
                    static_cast<stlab::execution_detail::main_task_queue*>(value)->push(
                        child.relocation_concept(), child.relocation_invoke(),
                        child.relocation_source());
                }}};
            queue.push(source.relocation_concept(), source.relocation_invoke(),
                       source.relocation_source());
            armed = true;
            auto task = waiting_pop ? queue.wait_pop() : queue.pop();
            if (!submitted) fail("main queue did not destroy the consumed source");
            (void)queue.pop();
        } else {
            stlab::execution_detail::task_shard queue;
            stlab::task<void() noexcept> source{destructor_submission{
                &armed, &submitted, &queue, [](void* value) {
                    stlab::task<void() noexcept> child{[]() noexcept {}};
                    static_cast<stlab::execution_detail::task_shard*>(value)->push(
                        {child.relocation_concept(), child.relocation_invoke(),
                         child.relocation_source()});
                }}};
            queue.push({source.relocation_concept(), source.relocation_invoke(),
                        source.relocation_source()});
            armed = true;
            auto task = waiting_pop ? queue.pop() : queue.try_pop();
            if (!submitted) fail("executor queue did not destroy the consumed source");
            (void)queue.pop();
        }
    }
    stlab::pre_exit();
}

void queue_noallocation(bool main_queue) {
    for (bool waiting_pop : {false, true}) {
        stlab::task<void() noexcept> source{[]() noexcept {}};
        if (main_queue) {
            stlab::execution_detail::main_task_queue queue;
            queue.push(source.relocation_concept(), source.relocation_invoke(),
                       source.relocation_source());
            const auto before = allocation_test::count.load();
            auto task = waiting_pop ? queue.wait_pop() : queue.pop();
            if (allocation_test::count.load() != before)
                fail("main queue allocated while consuming an accepted task");
        } else {
            stlab::execution_detail::task_shard queue;
            queue.push({source.relocation_concept(), source.relocation_invoke(),
                        source.relocation_source()});
            const auto before = allocation_test::count.load();
            auto task = waiting_pop ? queue.pop() : queue.try_pop();
            (void)queue.pop();
            (void)queue.try_pop();
            if (allocation_test::count.load() != before)
                fail("executor queue allocated while consuming an accepted task");
        }
    }
    stlab::pre_exit();
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) fail("expected one portable executor regression scenario");
    const std::string scenario = argv[1];
    if (scenario == "expansion_wake")
        expansion_wake();
    else if (scenario == "expansion_during_join")
        expansion_during_join();
    else if (scenario == "hardware_zero")
        hardware_zero();
    else if (scenario == "descendant_wait")
        descendant_wait(false);
    else if (scenario == "descendant_capture_wait")
        descendant_wait(true);
    else if (scenario == "queue_reentry")
        queue_reentry(false);
    else if (scenario == "main_queue_reentry")
        queue_reentry(true);
    else if (scenario == "queue_noallocation")
        queue_noallocation(false);
    else if (scenario == "main_queue_noallocation")
        queue_noallocation(true);
    else
        fail("unknown scenario");
    std::printf("PASS: %s\n", argv[1]);
}
