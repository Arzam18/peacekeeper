#ifndef PEACEKEEPER_STACK_THREAD
#define PEACEKEEPER_STACK_THREAD

#if defined(_WIN32)
// Keep the upstream Windows build working; the custom stack is for Android/POSIX.
#include <thread>
using StackThread = std::thread;
#else
#include <pthread.h>
#include <cstddef>
#include <functional>
#include <exception>
#include <stdexcept>
#include <utility>

/*
 * Android/bionic pthread stacks are much smaller than the 32 MiB stack size
 * the original project requested for its executable. The search worker keeps
 * the same callable and arguments as std::thread, but receives that stack
 * budget explicitly. The large NNUE accumulator is separately heap-allocated
 * in iterative_deepening(), so this stack remains available for recursion.
 *
 * The stack reservation is virtual address space; pages are committed on use.
 */
constexpr std::size_t PEACEKEEPER_THREAD_STACK_SIZE = 32ull * 1024ull * 1024ull;

class StackThread {
private:
    pthread_t thread_{};
    bool joinable_{false};

    static void* entry(void* arg) {
        std::function<void()>* task =
            static_cast<std::function<void()>*>(arg);

        try {
            (*task)();
        } catch (...) {
            // A std::thread exception would terminate the process as well.
            // Do not let an exception escape through the pthread ABI.
            delete task;
            std::terminate();
        }

        delete task;
        return nullptr;
    }

public:
    StackThread() = default;

    template <class Function, class... Args>
    explicit StackThread(Function&& function, Args&&... args) {
        start(
            std::bind(
                std::forward<Function>(function),
                std::forward<Args>(args)...));
    }

    StackThread(const StackThread&) = delete;
    StackThread& operator=(const StackThread&) = delete;

    StackThread(StackThread&& other) noexcept
        : thread_(other.thread_), joinable_(other.joinable_) {
        other.joinable_ = false;
    }

    StackThread& operator=(StackThread&& other) noexcept {
        if (this != &other) {
            if (joinable_) {
                std::terminate();
            }
            thread_ = other.thread_;
            joinable_ = other.joinable_;
            other.joinable_ = false;
        }
        return *this;
    }

    ~StackThread() {
        // Match std::thread's safety rule: an accidentally destroyed
        // joinable thread terminates instead of silently detaching.
        if (joinable_) {
            std::terminate();
        }
    }

    template <class Callable>
    void start(Callable&& callable) {
        if (joinable_) {
            std::terminate();
        }

        auto* task = new std::function<void()>(
            std::forward<Callable>(callable));

        pthread_attr_t attr;
        if (pthread_attr_init(&attr) != 0) {
            delete task;
            throw std::runtime_error("pthread_attr_init failed");
        }

        const std::size_t stack_size =
            PEACEKEEPER_THREAD_STACK_SIZE < static_cast<std::size_t>(PTHREAD_STACK_MIN)
                ? static_cast<std::size_t>(PTHREAD_STACK_MIN)
                : PEACEKEEPER_THREAD_STACK_SIZE;

        if (pthread_attr_setstacksize(&attr, stack_size) != 0) {
            pthread_attr_destroy(&attr);
            delete task;
            throw std::runtime_error("pthread_attr_setstacksize failed");
        }

        const int rc = pthread_create(&thread_, &attr, &StackThread::entry, task);
        pthread_attr_destroy(&attr);

        if (rc != 0) {
            delete task;
            throw std::runtime_error("pthread_create failed");
        }

        joinable_ = true;
    }

    bool joinable() const noexcept {
        return joinable_;
    }

    void join() {
        if (!joinable_) {
            return;
        }

        const int rc = pthread_join(thread_, nullptr);
        if (rc != 0) {
            throw std::runtime_error("pthread_join failed");
        }

        joinable_ = false;
    }

    void detach() {
        if (!joinable_) {
            return;
        }

        const int rc = pthread_detach(thread_);
        if (rc != 0) {
            throw std::runtime_error("pthread_detach failed");
        }

        joinable_ = false;
    }

    pthread_t native_handle() const noexcept {
        return thread_;
    }
};

#endif // _WIN32

#endif
