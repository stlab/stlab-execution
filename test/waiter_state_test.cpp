/*
    Copyright 2013 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../src/concurrency/detail/waiter_state.hpp"

#include <doctest/doctest.h>

TEST_CASE("portable waiter retains a wake requested before waiting") {
    stlab::execution_detail::waiter_state state;

    REQUIRE_FALSE(state.wake());
    REQUIRE_FALSE(state.begin_wait());

    CHECK(state.begin_wait());
    CHECK(state.wake());
}
