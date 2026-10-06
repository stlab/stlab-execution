/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/task.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
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

TEST_CASE("task self-move preserves a move-only target") {
    int destructions = 0;
    auto deleter = [&destructions](int* value) noexcept {
        ++destructions;
        delete value;
    };
    {
        stlab::task<int() noexcept> target =
            [p = std::unique_ptr<int, decltype(deleter)>(new int(73), deleter)]() noexcept {
                return p ? *p : -1;
            };
        auto& alias = target;
        target = std::move(alias);
        CHECK(destructions == 0);
        CHECK(target() == 73);
    }
    CHECK(destructions == 1);
}

TEST_CASE("task self-swap preserves a move-only target") {
    stlab::task<int() noexcept> target = [p = std::make_unique<int>(91)]() noexcept {
        return p ? *p : -1;
    };
    target.swap(target);
    CHECK(target() == 91);
    std::swap(target, target);
    CHECK(target() == 91);
}

TEST_CASE("task preserves a reference result") {
    int value = 119;
    stlab::task<int&()> target = [&]() -> int& { return value; };
    CHECK((std::is_same_v<decltype(target()), int&>));
    auto&& result = target();
    CHECK(&result == &value);
    result = 137;
    CHECK(value == 137);
}

namespace {
struct throwing_move_target {
    throwing_move_target() = default;
    throwing_move_target(const throwing_move_target&) = default;
    throwing_move_target(throwing_move_target&&) { throw std::runtime_error("target move"); }
    auto operator()() const noexcept -> int { return 83; }
};
} // namespace

TEST_CASE("task relocates a throwing-move target without moving the callable") {
    throwing_move_target callable;
    stlab::task<int() noexcept> first{callable};
    stlab::task<int() noexcept> second{std::move(first)};
    CHECK(second() == 83);
    stlab::task<int() noexcept> relocated{second.relocation_concept(), second.relocation_invoke(),
                                          second.relocation_source()};
    CHECK(relocated() == 83);
}
