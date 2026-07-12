#pragma once

#include <thread>              // std::thread 线程类
#include <vector>              // std::vector 存线程列表
#include <queue>               // std::queue 任务队列
#include <mutex>               // std::mutex 保护任务队列
#include <condition_variable>  // std::condition_variable 线程间通知机制
#include <functional>          // std::function 包装任意可调用对象
#include <atomic>              // std::atomic<bool> 原子运行标志

// ==================== ThreadPool 线程池类 ====================
// 固定数量的工作线程 + 一个任务队列
// 可以把耗时操作（如复杂计算、磁盘I/O）丢给线程池异步执行
// 避免阻塞 Reactor 的主事件循环
class ThreadPool {
public:
    using Task = std::function<void()>;  // 任务类型：任意无参无返回值的可调用对象

    explicit ThreadPool(size_t num_threads = 4);  // 构造函数：创建 num_threads 个工作线程
    ~ThreadPool();                                  // 析构函数：等待所有线程完成，清理

    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(Task task);               // 往任务队列里丢一个任务
    void stop();                          // 停止线程池，等待所有任务完成
    size_t pending_tasks() const;         // 查看还有多少个任务没执行

private:
    void worker_loop();                   // 工作线程的主循环：不断从队列取任务执行

    std::vector<std::thread> workers_;    // 工作线程列表
    std::queue<Task> tasks_;              // 任务队列（FIFO：先丢进去的先执行）
    mutable std::mutex mutex_;            // 保护任务队列的互斥锁
    std::condition_variable cond_;        // 条件变量：队列空时工作线程在这睡觉
    std::atomic<bool> running_ = true;    // 线程池是否还在运行
};
