/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef STLAB_SRC_CONCURRENCY_DETAIL_MAIN_TASK_QUEUE_HPP
#define STLAB_SRC_CONCURRENCY_DETAIL_MAIN_TASK_QUEUE_HPP

#include <stlab/concurrency/task.hpp>
#include <stlab/execution/config.hpp>

#include <cassert>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace stlab {
STLAB_EXECUTION_VERSION_NAMESPACE_BEGIN()
namespace execution_detail {

/// FIFO of main-executor tasks shared by submitting threads and the thread servicing the main
/// queue.
class main_task_queue {
    using task_t = task<void() noexcept>;
    using entry_t = std::optional<task_t>;

    std::mutex _mutex;
    std::condition_variable _ready;
    std::deque<std::unique_ptr<entry_t>> _tasks;

public:
    /// Appends the task relocated from `source`.
    ///
    /// - Precondition: `source` is the `relocation_source()` of a live task sharing
    ///   `vtable`/`invoke`.
    void push(const task_t::concept_t* vtable, task_t::invoke_t invoke, void* source) {
        std::unique_lock<std::mutex> lock{_mutex};
        auto entry = std::make_unique<entry_t>();
        _tasks.push_back(std::move(entry));
        _tasks.back()->emplace(vtable, invoke, source);
        lock.unlock();
        _ready.notify_one();
    }

    /// Removes and returns the oldest task.
    ///
    /// - Precondition: the queue is not empty.
    auto pop() -> task_t {
        std::unique_ptr<entry_t> extracted;
        {
            std::lock_guard<std::mutex> lock{_mutex};
            assert(!_tasks.empty() && "main executor wake without a queued task.");
            extracted = std::move(_tasks.front());
            _tasks.pop_front();
        }
        return std::move(**extracted);
    }

    /// Waits until a task is available, then removes and returns the oldest task.
    auto wait_pop() -> task_t {
        std::unique_ptr<entry_t> extracted;
        {
            std::unique_lock<std::mutex> lock{_mutex};
            _ready.wait(lock, [&] { return !_tasks.empty(); });
            extracted = std::move(_tasks.front());
            _tasks.pop_front();
        }
        return std::move(**extracted);
    }
};

/// Returns the process-shared main-executor task queue.
///
/// The queue is intentionally never destroyed so pending main-queue wakes never observe a
/// destroyed queue and no main-queue task is destroyed during static destruction.
inline auto main_tasks() -> main_task_queue& {
    static auto& queue = *new main_task_queue; // NOLINT(cppcoreguidelines-owning-memory)
    return queue;
}

} // namespace execution_detail
STLAB_EXECUTION_VERSION_NAMESPACE_END()
} // namespace stlab

#endif
