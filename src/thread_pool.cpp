#include "thread_pool.h"
#include <cstdio>

ThreadPool::ThreadPool(size_t num_threads)
{
    printf("[ThreadPool] starting %zu threads\n", num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::submit(Task task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        tasks_.push(std::move(task));
    }
    cond_.notify_one();
}

void ThreadPool::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }
    cond_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

size_t ThreadPool::pending_tasks() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop()
{
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this]() { return !running_ || !tasks_.empty(); });
            if (!running_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
