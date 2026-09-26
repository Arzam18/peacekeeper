#ifndef PEACEKEEPER_STACK_THREAD
#define PEACEKEEPER_STACK_THREAD

#include <pthread.h>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>

/*
 * Android/bionic gives pthread-created threads a much smaller default stack
 * than the 32 MiB stack intended by Peacekeeper's desktop build.
 *
 * Keep the engine's existing search intact and give every engine-created
 * worker/search thread the same stack budget explicitly.
 *
 * 32 MiB is virtual address space reservation; pages are committed on demand.
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
            PEACEKEEPER_THREAD_STACK_SIZE < PTHREAD_STACK_MIN
                ? PTHREAD_STACK_MIN
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

#endif
