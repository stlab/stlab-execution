/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/system_timer.hpp>
#include <stlab/pre_exit.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <utility>

namespace {
struct state {
    std::atomic<bool> armed{false};
    std::atomic<bool> submitted{false};
    std::shared_future<void> release;
    std::promise<void> completed;
};

struct target {
    state* context;
    bool moved_from = false;

    explicit target(state& value) noexcept : context(&value) {}
    target(target&& other) noexcept : context(other.context) { other.moved_from = true; }
    void operator()() const noexcept { context->release.wait(); }
    ~target() {
        if (moved_from && context->armed.load(std::memory_order_acquire) &&
            !context->submitted.exchange(true)) {
            stlab::system_timer(std::chrono::nanoseconds::zero(),
                                [value = context]() noexcept { value->completed.set_value(); });
        }
    }
};
} // namespace

int main() {
    using namespace std::chrono_literals;
    std::promise<void> release;
    state context;
    context.release = release.get_future().share();
    auto completed = context.completed.get_future();
    std::promise<void> started;
    auto ready = started.get_future();
    stlab::system_timer(0ns, [&]() noexcept {
        started.set_value();
        context.release.wait();
    });
    if (ready.wait_for(5s) != std::future_status::ready) std::_Exit(1);
    stlab::task<void() noexcept> source{target(context)};
    context.armed.store(true, std::memory_order_release);
    const auto status = stlab::stlab_v2_system_timer_submit(
        &stlab::execution_detail::current_task_storage_abi_guard::value,
        source.relocation_concept(), source.relocation_invoke(), source.relocation_source(), 0);
    if (status.code != 0) std::_Exit(3);
    stlab::system_timer(0ns, []() noexcept {});
    release.set_value();
    if (completed.wait_for(5s) != std::future_status::ready) std::_Exit(2);
    stlab::pre_exit();
}
