/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/task.hpp>

#include <array>
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
struct member_target {
    int value = 73;
    auto add(int amount) noexcept -> int& {
        value += amount;
        return value;
    }
    auto read() const -> int { return value; }
};

struct throwing_move_target {
    throwing_move_target() = default;
    throwing_move_target(const throwing_move_target&) = default;
    throwing_move_target(throwing_move_target&&) { throw std::runtime_error("target move"); }
    auto operator()() const noexcept -> int { return 83; }
};
} // namespace

TEST_CASE("task invokes member functions with forwarded arguments and reference results") {
    member_target object;
    stlab::task<int&(member_target&, int) noexcept> add = &member_target::add;
    CHECK(noexcept(add(object, 18)));
    CHECK(&add(object, 18) == &object.value);
    CHECK(object.value == 91);

    stlab::task<int(const member_target&)> read = &member_target::read;
    CHECK(read(object) == 91);
}

TEST_CASE("task invokes data members through objects pointers and reference wrappers") {
    member_target object;
    stlab::task<int&(member_target&) noexcept> by_reference = &member_target::value;
    by_reference(object) = 97;
    CHECK(object.value == 97);

    stlab::task<int&(member_target*) noexcept> by_pointer = &member_target::value;
    CHECK(&by_pointer(&object) == &object.value);

    stlab::task<int&(std::reference_wrapper<member_target>) noexcept> by_wrapper =
        &member_target::value;
    CHECK(&by_wrapper(std::ref(object)) == &object.value);

    auto owned = std::make_unique<member_target>();
    stlab::task<int&(const std::unique_ptr<member_target>&) noexcept> by_owner =
        &member_target::value;
    by_owner(owned) = 137;
    CHECK(owned->value == 137);
}

TEST_CASE("null member pointers produce empty tasks") {
    decltype(&member_target::add) null_function = nullptr;
    stlab::task<int&(member_target&, int) noexcept> function = null_function;
    CHECK(function == nullptr);

    decltype(&member_target::value) null_member = nullptr;
    stlab::task<int&(member_target&) noexcept> member = null_member;
    CHECK(member == nullptr);
}

TEST_CASE("heap-backed task forwards move-only arguments and preserves reference results") {
    int value = 119;
    auto callable = [padding = std::array<int, stlab::stlab_v2_task_storage_size>{},
                     &value](std::unique_ptr<int> amount) noexcept -> int& {
        value += *amount + padding[0];
        return value;
    };
    static_assert(sizeof(callable) > stlab::stlab_v2_task_storage_size);
    stlab::task<int&(std::unique_ptr<int>) noexcept> target = std::move(callable);
    CHECK(&target(std::make_unique<int>(18)) == &value);
    CHECK(value == 137);
}

TEST_CASE("task relocates a throwing-move target without moving the callable") {
    throwing_move_target callable;
    stlab::task<int() noexcept> first{callable};
    stlab::task<int() noexcept> second{std::move(first)};
    CHECK(second() == 83);
    stlab::task<int() noexcept> relocated{second.relocation_concept(), second.relocation_invoke(),
                                          second.relocation_source()};
    CHECK(relocated() == 83);
}
