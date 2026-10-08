#include <stlab/concurrency/system_timer.hpp>
#include <stlab/concurrency/task.hpp>

#include <dispatch/dispatch.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>

namespace {
std::function<void()> during_preparation;
std::function<void()> during_arming;
std::function<void()> before_resume;
std::function<void()> after_cancel;
bool canceled = false;
bool resumed = false;
bool released = false;
bool cancel_handler_cleared = false;
bool invoked = false;
stlab::task<void() noexcept>* client_target = nullptr;

auto create_source(dispatch_source_type_t type,
                   uintptr_t handle,
                   uintptr_t mask,
                   dispatch_queue_t queue) -> dispatch_source_t {
    auto source = dispatch_source_create(type, handle, mask, queue);
    if (!source) std::_Exit(2);
    if (during_preparation) during_preparation();
    return source;
}

void cancel_source(dispatch_source_t source) {
    canceled = true;
    dispatch_source_cancel(source);
    if (after_cancel) after_cancel();
}

void resume_source(dispatch_object_t source) {
    resumed = true;
    if (before_resume) before_resume();
    dispatch_resume(source);
}

void set_timer(dispatch_source_t source,
               dispatch_time_t start,
               std::uint64_t interval,
               std::uint64_t leeway) {
    dispatch_source_set_timer(source, start, interval, leeway);
    if (during_arming) during_arming();
}

void release_source(dispatch_object_t source) {
    released = true;
    dispatch_release(source);
}

void set_cancel_handler(dispatch_source_t source, dispatch_function_t handler) {
    cancel_handler_cleared = handler == nullptr;
    dispatch_source_set_cancel_handler_f(source, handler);
}
} // namespace

#define dispatch_source_create create_source
#define dispatch_source_cancel cancel_source
#define dispatch_resume resume_source
#define dispatch_release release_source
#define dispatch_source_set_cancel_handler_f set_cancel_handler
#define dispatch_source_set_timer set_timer
#include "../src/concurrency/system_timer_libdispatch.cpp"
#undef dispatch_source_create
#undef dispatch_source_cancel
#undef dispatch_resume
#undef dispatch_release
#undef dispatch_source_set_cancel_handler_f
#undef dispatch_source_set_timer

namespace {
void close_during_preparation(stlab::v2::dispatch_timers& service) {
    std::promise<void> closed;
    auto ready = closed.get_future();
    std::thread closer([&] {
        service.close();
        closed.set_value();
    });
    if (ready.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        std::fputs("FAIL: timer preparation blocks shutdown on the admission mutex\n", stderr);
        std::_Exit(1);
    }
    closer.join();
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "activation_shutdown") == 0) {
        stlab::v2::dispatch_timers service;
        std::promise<void> cancellation;
        auto requested = cancellation.get_future();
        std::thread closer;
        after_cancel = [&] { cancellation.set_value(); };
        before_resume = [&] {
            closer = std::thread([&] { service.close(); });
            if (requested.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
                std::fputs("FAIL: source activation holds the admission mutex\n", stderr);
                std::_Exit(1);
            }
        };
        stlab::task<void() noexcept> target = []() noexcept { invoked = true; };
        service.submit(target.relocation_concept(), target.relocation_invoke(),
                       target.relocation_source(), 0);
        closer.join();
        if (invoked || !released) {
            std::fputs("FAIL: shutdown did not drain the canceled inactive source\n", stderr);
            return 1;
        }
        std::puts("PASS: shutdown cancels and drains a source activated after publication");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "capture_destruction") == 0) {
        stlab::v2::dispatch_timers service;
        std::promise<void> destroyed;
        auto ready = destroyed.get_future();
        stlab::task<void() noexcept> target =
            [owned = std::unique_ptr<int, std::function<void(int*)>>(new int, [&](int* pointer) {
                 delete pointer;
                 destroyed.set_value();
             })]() noexcept {};
        service.submit(
            target.relocation_concept(), target.relocation_invoke(), target.relocation_source(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1)).count());
        after_cancel = [&] {
            if (ready.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
                std::fputs("FAIL: capture destruction waits for the admission mutex\n", stderr);
                std::_Exit(1);
            }
        };
        service.close();
        std::puts("PASS: capture destruction does not acquire the admission mutex");
        return 0;
    }
    std::set_terminate([] {
        if (client_target) (*client_target)();
        const bool passed = canceled && resumed && released && cancel_handler_cleared && invoked;
        std::puts(passed ?
                      "PASS: closed admission discards preparation without consuming the task" :
                      "FAIL: closed admission leaked preparation or consumed the task");
        std::fflush(stdout);
        std::_Exit(passed ? 0 : 1);
    });
    stlab::v2::dispatch_timers service;
    auto prepare = [&] { close_during_preparation(service); };
    if (argc == 2 && std::strcmp(argv[1], "arming_shutdown") == 0)
        during_arming = prepare;
    else
        during_preparation = prepare;
    stlab::task<void() noexcept> target = []() noexcept { invoked = true; };
    client_target = &target;
    service.submit(target.relocation_concept(), target.relocation_invoke(),
                   target.relocation_source(), 0);
    std::fputs("FAIL: closed timer admission returned successfully\n", stderr);
    return 1;
}
