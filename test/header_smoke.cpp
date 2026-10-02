#include <stlab/execution/config.hpp>

#ifdef STLAB_STD_COROUTINES
#error Execution must not define STLab's coroutine policy
#endif

#include <memory>
#include <stlab/concurrency/default_executor.hpp>
#include <stlab/concurrency/executor_base.hpp>
#include <stlab/concurrency/immediate_executor.hpp>
#include <stlab/concurrency/main_executor.hpp>
#include <stlab/concurrency/set_current_thread_name.hpp>
#include <stlab/concurrency/system_timer.hpp>
#include <stlab/concurrency/task.hpp>
#include <stlab/pre_exit.hpp>

int main() {
    int result = 0;
    stlab::task<void() noexcept> work = [p = std::make_unique<int>(42), &result]() noexcept {
        result = *p;
    };
    stlab::immediate_executor(std::move(work));
    stlab::pre_exit();
    return result == 42 ? 0 : 1;
}
