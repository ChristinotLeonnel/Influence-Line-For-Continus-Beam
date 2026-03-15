#pragma once
/**
 * thread_pool.hpp
 * Pool de threads léger — remplace ThreadPoolExecutor Python.
 *
 * Avantages vs Python ThreadPoolExecutor :
 *   - Pas de GIL sur les calculs C++ (calculs vectoriels, I/O JSON).
 *   - Overhead de création de threads réduit (threads réutilisés).
 *   - future<T> standard C++17 — pas de pickling.
 *
 * Implémentation :
 *   - Queue de tâches protégée par mutex + condition_variable.
 *   - N threads workers persistants (N = std::thread::hardware_concurrency).
 *   - submit() retourne un std::future<T> comme Python's Future.
 */

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace influence_line {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t n_threads =
                            std::thread::hardware_concurrency())
        : stop_(false)
    {
        if (n_threads == 0) n_threads = 4;
        workers_.reserve(n_threads);
        for (std::size_t i = 0; i < n_threads; ++i)
            workers_.emplace_back([this]{ worker_loop(); });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lk(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Soumet une tâche et retourne un std::future<ReturnType>.
     *
     * Utilisation :
     *   auto f = pool.submit([](){ return 42; });
     *   int result = f.get();
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        auto future = task->get_future();
        {
            std::unique_lock<std::mutex> lk(mutex_);
            if (stop_)
                throw std::runtime_error("ThreadPool: submit après arrêt");
            tasks_.push([task]{ (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    /// Attend que toutes les tâches en cours soient terminées.
    void wait_all() {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_done_.wait(lk, [this]{ return tasks_.empty() && active_ == 0; });
    }

    std::size_t n_threads() const { return workers_.size(); }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait(lk, [this]{ return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
                ++active_;
            }
            task();
            {
                std::unique_lock<std::mutex> lk(mutex_);
                --active_;
            }
            cv_done_.notify_all();
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mutex_;
    std::condition_variable           cv_;
    std::condition_variable           cv_done_;
    bool                              stop_;
    std::size_t                       active_ = 0;
};

} // namespace influence_line
