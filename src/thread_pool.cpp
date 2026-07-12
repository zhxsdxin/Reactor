#include "thread_pool.h"  // 自己的头文件
#include <cstdio>          // printf

// ==================== 构造函数：创建 num_threads 个工作线程 ====================
ThreadPool::ThreadPool(size_t num_threads)
{
    printf("[ThreadPool] starting %zu worker threads\n", num_threads);
    // 预分配空间，避免 vector 多次扩容
    workers_.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        // emplace_back: 直接在 vector 尾部构造 std::thread
        // &ThreadPool::worker_loop: 成员函数指针
        // this: 把当前对象作为成员函数的隐含参数传入
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

// ==================== 析构函数：等待所有线程完成 ====================
ThreadPool::~ThreadPool()
{
    stop();  // 停止线程池，join 所有线程
}

// ==================== 提交一个任务到队列 ====================
void ThreadPool::submit(Task task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁
        if (!running_) return;                      // 线程池已停止，不接受新任务
        tasks_.push(std::move(task));               // 把任务移到队列尾部（移动语义，免拷贝）
    }  // 锁释放

    cond_.notify_one();  // 唤醒一个正在睡觉的工作线程："有活干了！"
}

// ==================== 停止线程池 ====================
void ThreadPool::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁
        if (!running_) return;                      // 已经停过了，不重复操作
        running_ = false;                           // 设置停止标志
    }

    cond_.notify_all();  // 唤醒所有睡觉的线程，让它们检查 running_ 标志

    // ===== join 所有工作线程，等待它们完成 =====
    for (auto& worker : workers_) {
        if (worker.joinable()) {  // 如果线程还可以 join（还没被 join 过）
            worker.join();        // 等待线程结束
        }
    }
    printf("[ThreadPool] all workers stopped\n");
}

// ==================== 查看还有多少待处理任务 ====================
size_t ThreadPool::pending_tasks() const
{
    std::lock_guard<std::mutex> lock(mutex_);  // 加锁
    return tasks_.size();                       // 返回队列大小
}

// ==================== 工作线程的主循环 ====================
void ThreadPool::worker_loop()
{
    while (true) {
        Task task;  // 用来接任务的变量

        {
            std::unique_lock<std::mutex> lock(mutex_);  // 加锁（unique_lock 可以手动 unlock）

            // ===== 条件变量等待 =====
            // wait 的第二个参数是唤醒条件（lambda）：
            //   返回 true  -> 醒来继续执行
            //   返回 false -> 继续睡（防止虚假唤醒）
            cond_.wait(lock, [this]() {
                return !running_ || !tasks_.empty();
                // 线程池被停止  或者  队列里有任务  -> 该醒醒了
            });

            // 线程池停止了 且 队列空了 -> 彻底退出
            if (!running_ && tasks_.empty()) return;

            // 从队列头部取一个任务
            task = std::move(tasks_.front());  // 移动，免拷贝
            tasks_.pop();                      // 从队列删除
        }  // 锁在这里释放，执行任务时不持锁，提高并发度

        task();  // 执行任务（不持锁！）
    }
}
