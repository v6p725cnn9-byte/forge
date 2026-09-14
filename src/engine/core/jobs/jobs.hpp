#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace forge::core {

class JobPool {
public:
    explicit JobPool(std::size_t workers = 0)
    {
        if (workers == 0) {
            workers = std::thread::hardware_concurrency();
            if (workers == 0) workers = 1;
        }
        workers_.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i) {
            workers_.emplace_back([this] { run(); });
        }
    }

    ~JobPool() { shutdown(); }

    JobPool(const JobPool&) = delete;
    JobPool& operator=(const JobPool&) = delete;

    void enqueue(std::function<void()> job)
    {
        {
            std::lock_guard lock(mutex_);
            if (stop_) return;
            queue_.push(std::move(job));
            ++outstanding_;
        }
        cv_.notify_one();
    }

    void wait()
    {
        std::unique_lock lock(mutex_);
        idle_.wait(lock, [this] { return outstanding_ == 0; });
    }

    std::size_t worker_count() const { return workers_.size(); }

    void shutdown()
    {
        {
            std::lock_guard lock(mutex_);
            if (stop_) return;
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_)
            if (worker.joinable()) worker.join();
        workers_.clear();
    }

private:
    void run()
    {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                job = std::move(queue_.front());
                queue_.pop();
            }
            job();
            {
                std::lock_guard lock(mutex_);
                --outstanding_;
            }
            idle_.notify_all();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idle_;
    std::queue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    std::size_t outstanding_ = 0;
    bool stop_ = false;
};

} // namespace forge::core
