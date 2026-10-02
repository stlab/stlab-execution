/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/task.hpp>

#include <functional>
#include <memory>
#include <utility>

#include <doctest/doctest.h>

TEST_CASE("task retains a move-only capture through move construction") {
    int observed = 0;
    stlab::task<void() noexcept> first = [p = std::make_unique<int>(42), &observed]() noexcept {
        observed = *p;
    };
    stlab::task<void() noexcept> second = std::move(first);
    second();
    CHECK(observed == 42);
}

TEST_CASE("empty task throws bad_function_call") {
    stlab::task<void()> empty;
    CHECK(empty == nullptr);
    CHECK_THROWS_AS(empty(), std::bad_function_call);
}

TEST_CASE("task releases its owned target exactly once after moving") {
    int destructions = 0;
    auto deleter = [&destructions](int* value) noexcept {
        ++destructions;
        delete value;
    };
    {
        stlab::task<void() noexcept> first =
            [p = std::unique_ptr<int, decltype(deleter)>(new int(42), deleter)]() noexcept {};
        {
            stlab::task<void() noexcept> second = std::move(first);
            CHECK(destructions == 0);
        }
        CHECK(destructions == 1);
    }
    CHECK(destructions == 1);
}

TEST_CASE("task reassignment releases the previous owned target") {
    int destructions = 0;
    auto deleter = [&destructions](int* value) noexcept {
        ++destructions;
        delete value;
    };
    stlab::task<void() noexcept> target =
        [p = std::unique_ptr<int, decltype(deleter)>(new int(42), deleter)]() noexcept {};
    target = nullptr;
    CHECK(target == nullptr);
    CHECK(destructions == 1);
}
