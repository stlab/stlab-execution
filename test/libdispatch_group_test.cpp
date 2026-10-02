#include <stlab/concurrency/detail/libdispatch_executor_group.hpp>

#include <chrono>
#include <future>
#include <thread>
#include <utility>

#include <doctest/doctest.h>

TEST_CASE("dispatch group move assignment retires the previous group's work") {
    stlab::execution_detail::group_t first;
    stlab::execution_detail::group_t second;
    const auto old = first._group;
    const auto replacement = second._group;
    dispatch_group_enter(old);
    std::promise<void> started;
    auto ready = started.get_future();
    std::promise<void> completed;
    auto done = completed.get_future();
    std::thread assignment([&] {
        started.set_value();
        first = std::move(second);
        completed.set_value();
    });
    ready.wait();
    CHECK(done.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
    dispatch_group_leave(old);
    assignment.join();
    CHECK(first._group == replacement);
    CHECK(second._group == nullptr);
    auto& alias = first;
    first = std::move(alias);
    CHECK(first._group == replacement);
}
