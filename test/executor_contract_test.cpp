/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/immediate_executor.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <ostream>
#include <thread>
#include <utility>

#include <doctest/doctest.h>

TEST_CASE("immediate executor runs on the calling thread before returning") {
    const auto caller = std::this_thread::get_id();
    auto observed = caller;
    bool invoked = false;
    stlab::immediate_executor([&]() noexcept {
        observed = std::this_thread::get_id();
        invoked = true;
    });
    CHECK(invoked);
    CHECK(observed == caller);
}

namespace {

/// Verifies that an executor delivers the value owned by a move-only task.
template <class Executor>
void check_move_only_submission(Executor executor) {
    std::promise<int> completion;
    auto result = completion.get_future();
    executor([p = std::make_unique<int>(42),
              completion = std::move(completion)]() mutable noexcept { completion.set_value(*p); });
    REQUIRE(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(result.get() == 42);
}

} // namespace

TEST_CASE("default executor invokes a move-only task") {
    check_move_only_submission(stlab::default_executor);
}

TEST_CASE("high executor invokes a move-only task") {
    check_move_only_submission(stlab::high_executor);
}

TEST_CASE("low executor invokes a move-only task") {
    check_move_only_submission(stlab::low_executor);
}
