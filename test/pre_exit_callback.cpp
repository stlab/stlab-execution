/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/system_timer.hpp>
#include <stlab/pre_exit.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <thread>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <stdlib.h>
#endif

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc != 2) std::_Exit(2);
    if (std::signal(SIGABRT, [](int) { std::_Exit(0); }) == SIG_ERR) std::_Exit(2);
    std::set_terminate([] { std::_Exit(0); });
    const std::string scenario(argv[1]);
    auto cleanup = [](int* pointer) noexcept {
        delete pointer;
        stlab::pre_exit();
    };
    if (scenario == "executor")
        stlab::default_executor([]() noexcept { stlab::pre_exit(); });
    else if (scenario == "timer")
        stlab::system_timer(std::chrono::nanoseconds::zero(), []() noexcept { stlab::pre_exit(); });
    else if (scenario == "executor_capture")
        stlab::default_executor(
            [p = std::unique_ptr<int, decltype(cleanup)>(new int, cleanup)]() noexcept {});
    else if (scenario == "timer_capture")
        stlab::system_timer(
            std::chrono::nanoseconds::zero(),
            [p = std::unique_ptr<int, decltype(cleanup)>(new int, cleanup)]() noexcept {});
    else
        std::_Exit(2);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::_Exit(1);
}
