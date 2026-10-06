/*
    Copyright 2013 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/task.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

using namespace stlab;

namespace {
/// Returns the address of the linker guard for the current task relocation storage ABI.
///
/// - Postcondition: the returned pointer refers to the current storage-ABI guard symbol.
auto current_task_abi_guard() noexcept -> const unsigned char* {
    return &stlab::execution_detail::current_task_storage_abi_guard::value;
}

struct counted_task_context {
    std::atomic<int>* _count{nullptr};
    std::atomic<int>* _remaining{nullptr};
    std::condition_variable* _ready{nullptr};
    std::mutex* _mutex{nullptr};

    static void run(void* context) noexcept {
        auto& self = *static_cast<counted_task_context*>(context);
        self._count->fetch_add(1, std::memory_order_relaxed);

        std::scoped_lock lock{*self._mutex};
        if (self._remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) {
            self._ready->notify_one();
        }
    }
};

/// Submits `count` tasks and waits until each task has executed exactly once.
///
/// - Complexity: O(`count`) submissions and checks.
template <typename Submit>
void wait_for_all_submissions(Submit&& submit, std::size_t count) {
    std::vector<std::atomic<int>> executions(count);
    for (auto& execution : executions)
        execution.store(0, std::memory_order_relaxed);

    std::vector<counted_task_context> contexts(count);
    std::atomic<int> remaining{static_cast<int>(count)};
    std::condition_variable ready;
    std::mutex mutex;

    for (std::size_t i = 0; i < count; ++i) {
        contexts[i] = counted_task_context{&executions[i], &remaining, &ready, &mutex};
        task<void() noexcept> t{
            [context = &contexts[i]]() noexcept { counted_task_context::run(context); }};
        submit(t.relocation_concept(), t.relocation_invoke(), t.relocation_source(), i);
    }

    {
        std::unique_lock<std::mutex> lock{mutex};
        ready.wait(lock, [&] { return remaining.load(std::memory_order_acquire) == 0; });
    }

    for (const auto& execution : executions) {
        REQUIRE(execution.load(std::memory_order_relaxed) == 1);
    }
}
} // namespace

TEST_CASE("abi_executor_submit_executes_each_task_exactly_once_across_priorities") {
    wait_for_all_submissions(
        [](const task<void() noexcept>::concept_t* vtable, task<void() noexcept>::invoke_t invoke,
           void* source, std::size_t index) {
            switch (index % 3) {
                case 0:
                    stlab_v2_high_executor_submit(current_task_abi_guard(), vtable, invoke, source);
                    break;
                case 1:
                    stlab_v2_default_executor_submit(current_task_abi_guard(), vtable, invoke,
                                                     source);
                    break;
                case 2:
                    stlab_v2_low_executor_submit(current_task_abi_guard(), vtable, invoke, source);
                    break;
            }
        },
        96);
}

TEST_CASE("abi_executor_submit_drains_concurrent_contention_without_dropping_tasks") {
    constexpr std::size_t submitter_count = 8;
    constexpr std::size_t tasks_per_submitter = 128;

    std::vector<std::atomic<int>> executions(submitter_count * tasks_per_submitter);
    for (auto& execution : executions)
        execution.store(0, std::memory_order_relaxed);

    std::vector<counted_task_context> contexts(executions.size());
    std::atomic<int> remaining{static_cast<int>(contexts.size())};
    std::condition_variable ready;
    std::mutex mutex;

    for (std::size_t i = 0; i < contexts.size(); ++i) {
        contexts[i] = counted_task_context{&executions[i], &remaining, &ready, &mutex};
    }

    std::vector<std::thread> submitters;
    submitters.reserve(submitter_count);

    for (std::size_t submitter = 0; submitter < submitter_count; ++submitter) {
        submitters.emplace_back([&, submitter] {
            const auto base = submitter * tasks_per_submitter;

            for (std::size_t offset = 0; offset < tasks_per_submitter; ++offset) {
                auto* context = &contexts[base + offset];
                task<void() noexcept> t{
                    [context]() noexcept { counted_task_context::run(context); }};
                switch ((submitter + offset) % 3) {
                    case 0:
                        stlab_v2_high_executor_submit(current_task_abi_guard(),
                                                      t.relocation_concept(), t.relocation_invoke(),
                                                      t.relocation_source());
                        break;
                    case 1:
                        stlab_v2_default_executor_submit(
                            current_task_abi_guard(), t.relocation_concept(), t.relocation_invoke(),
                            t.relocation_source());
                        break;
                    case 2:
                        stlab_v2_low_executor_submit(current_task_abi_guard(),
                                                     t.relocation_concept(), t.relocation_invoke(),
                                                     t.relocation_source());
                        break;
                }
            }
        });
    }

    for (auto& submitter : submitters)
        submitter.join();

    {
        std::unique_lock<std::mutex> lock{mutex};
        ready.wait(lock, [&] { return remaining.load(std::memory_order_acquire) == 0; });
    }

    for (const auto& execution : executions) {
        REQUIRE(execution.load(std::memory_order_relaxed) == 1);
    }
}

TEST_CASE("abi callback does not publish completion before acquiring the final-access lock") {
    std::atomic<int> count{0};
    std::atomic<int> remaining{1};
    std::condition_variable ready;
    std::mutex mutex;
    counted_task_context context{&count, &remaining, &ready, &mutex};
    std::unique_lock lock(mutex);
    std::thread callback([&] { counted_task_context::run(&context); });
    const auto started_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (count.load(std::memory_order_acquire) == 0 &&
           std::chrono::steady_clock::now() < started_deadline)
        std::this_thread::yield();
    CHECK(count.load() == 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    while (remaining.load(std::memory_order_acquire) != 0 &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(remaining.load(std::memory_order_acquire) == 1);
    lock.unlock();
    callback.join();
    CHECK(remaining.load() == 0);
}
