/*
    Copyright 2026 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../src/concurrency/main_executor_qt.cpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <new>

namespace {
thread_local unsigned allocations = 0;
}

auto operator new(std::size_t size) -> void* {
    if (auto* pointer = std::malloc(std::max(size, std::size_t{1}))) {
        ++allocations;
        return pointer;
    }
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

int main(int argc, char** argv) {
    QCoreApplication application{argc, argv};
    using namespace stlab::STLAB_EXECUTION_VERSION_NAMESPACE()::execution_detail;
    // Warm Qt's initialization before measuring its own event and QObject storage.
    {
        executor_event event{main_task_t{[]() noexcept {}}};
    }
    unsigned qt_allocations = 0;
    {
        const auto before = allocations;
        QEvent event{QEvent::User};
        event_receiver receiver;
        receiver.moveToThread(application.thread());
        qt_allocations = allocations - before;
    }
    bool invoked = false;
    const auto before = allocations;
    {
        executor_event event{main_task_t{[&]() noexcept { invoked = true; }}};
        if (allocations - before != qt_allocations) {
            std::fputs("FAIL: Qt executor allocated separate receiver storage\n", stderr);
            return 1;
        }
        if (event.receiver()->thread() != application.thread()) {
            std::fputs("FAIL: Qt executor receiver has incorrect thread affinity\n", stderr);
            return 1;
        }
        QCoreApplication::sendEvent(event.receiver(), &event);
    }
    if (!invoked) {
        std::fputs("FAIL: Qt executor did not invoke its task\n", stderr);
        return 1;
    }
}
