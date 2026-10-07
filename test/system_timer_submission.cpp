/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/system_timer.hpp>
#include <stlab/pre_exit.hpp>

#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>

int main() {
    using namespace std::chrono_literals;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::promise<void> started;
    auto ready = started.get_future();
    std::promise<void> body_completed;
    auto body_done = body_completed.get_future();
    std::promise<void> cleanup_completed;
    auto cleanup_done = cleanup_completed.get_future();
    stlab::system_timer(0ns, [&]() noexcept {
        started.set_value();
        released.wait();
    });
    if (ready.wait_for(5s) != std::future_status::ready) std::_Exit(1);
    auto cleanup = [&](int* pointer) noexcept {
        delete pointer;
        stlab::system_timer(0ns, [&]() noexcept { cleanup_completed.set_value(); });
    };
    stlab::system_timer(0ns, [owned = std::unique_ptr<int, decltype(cleanup)>(new int, cleanup),
                              &body_completed]() noexcept {
        stlab::system_timer(0ns, [&]() noexcept { body_completed.set_value(); });
    });
    for (int i = 0; i != 32; ++i)
        stlab::system_timer(0ns, []() noexcept {});
    release.set_value();
    if (body_done.wait_for(5s) != std::future_status::ready) std::_Exit(2);
    if (cleanup_done.wait_for(5s) != std::future_status::ready) std::_Exit(3);
    stlab::pre_exit();
}
