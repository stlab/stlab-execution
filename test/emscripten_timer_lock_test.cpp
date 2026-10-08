/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/system_timer.hpp>
#include <stlab/concurrency/task.hpp>
#include <stlab/pre_exit.hpp>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <utility>

namespace timer_test_std {
using namespace std;
inline thread_local unsigned mutex_depth = 0;

class mutex {
    // Provides real exclusion for the backend state while observing this thread's lock depth.
    std::mutex _mutex;

public:
    void lock() {
        _mutex.lock();
        ++mutex_depth;
    }
    void unlock() {
        --mutex_depth;
        _mutex.unlock();
    }
    auto try_lock() -> bool {
        if (!_mutex.try_lock()) return false;
        ++mutex_depth;
        return true;
    }
};

#if defined(__EMSCRIPTEN_PTHREADS__)
using condition_variable = std::condition_variable_any;
#endif
} // namespace timer_test_std

namespace {
bool check_registration = false;
bool check_cancellation = false;
void* watched_record = nullptr;
bool watched_deleted = false;
em_arg_callback_func timeout_callback = nullptr;
void* timeout_context = nullptr;
long timeout_id = 0;

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

auto set_timeout(em_arg_callback_func callback, double delay, void* context) -> long {
    if (check_registration && timer_test_std::mutex_depth != 0)
        fail("timeout registration holds the list mutex");
    timeout_callback = callback;
    timeout_context = context;
    timeout_id = emscripten_set_timeout(callback, delay, context);
    return timeout_id;
}

void clear_timeout(long id) {
    if (check_cancellation && timer_test_std::mutex_depth != 0)
        fail("timeout cancellation holds the list mutex");
    emscripten_clear_timeout(id);
}
} // namespace

auto operator new(std::size_t size) -> void* {
    if (auto* pointer = std::malloc(std::max(size, std::size_t{1}))) return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept {
    if (pointer && pointer == watched_record) {
        if (timer_test_std::mutex_depth != 0)
            fail("closed registration destroys its record under the list mutex");
        watched_record = nullptr;
        watched_deleted = true;
    }
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

#define std timer_test_std
#define emscripten_set_timeout set_timeout
#define emscripten_clear_timeout clear_timeout
#include "../src/concurrency/system_timer_emscripten.cpp"
#undef emscripten_clear_timeout
#undef emscripten_set_timeout
#undef std

namespace {
struct run_context {
    const char* scenario;
};

void run(void* context) {
    using namespace stlab::execution_detail;
    const auto* scenario = static_cast<run_context*>(context)->scenario;
    if (std::strcmp(scenario, "closed_registration") == 0) {
        auto record = std::make_unique<timer_record>(3600000000000);
        stlab::task<void() noexcept> target{[]() noexcept {}};
        record->target.emplace(target.relocation_concept(), target.relocation_invoke(),
                               target.relocation_source());
        auto& service = state();
        {
            std::scoped_lock lock(service.mutex);
            service.insert(*record);
        }
        watched_record = record.get();
        auto* registration = record.release();
        close_on_main();
        register_timer(registration);
        if (!watched_deleted) fail("closed registration did not release its record");
    } else if (std::strcmp(scenario, "registration") == 0 || std::strcmp(scenario, "rearm") == 0 ||
               std::strcmp(scenario, "cancellation") == 0) {
        check_registration = std::strcmp(scenario, "registration") == 0;
        check_cancellation = std::strcmp(scenario, "cancellation") == 0;
        stlab::system_timer(std::chrono::hours(1), []() noexcept {});
        if (std::strcmp(scenario, "rearm") == 0) {
            emscripten_clear_timeout(timeout_id);
            check_registration = true;
            timeout_callback(timeout_context);
        }
    } else {
        fail("unknown timer lock scenario");
    }
    stlab::pre_exit();
    std::printf("PASS: %s performs private timeout work outside the list mutex\n", scenario);
}
} // namespace

int main(int argc, char** argv) {
    run_context context{argc == 2 ? argv[1] : "registration"};
#if defined(__EMSCRIPTEN_PTHREADS__)
    if (!emscripten_is_main_runtime_thread()) {
        emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VI, &run, &context);
        return 0;
    }
#endif
    run(&context);
}
