/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../src/concurrency/core_shutdown.cpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main() {
    std::unique_lock lock(stlab::execution_detail::state().mutex);
    std::thread caller([] {
        std::set_terminate([] {
            const bool passed = !std::current_exception();
            std::puts(passed ? "PASS: invalid cleanup is rejected without registry locking" :
                               "FAIL: invalid cleanup threw before validation");
            std::fflush(stdout);
            std::_Exit(passed ? 0 : 1);
        });
        stlab::execution_detail::register_core_executor_cleanup(nullptr);
    });
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::fputs("FAIL: private cleanup validation waits for the registry mutex\n", stderr);
    std::_Exit(1);
}
