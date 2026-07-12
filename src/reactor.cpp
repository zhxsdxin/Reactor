#include "reactor.h"        // 自己的头文件
#include "thread_pool.h"    // 线程池完整定义（这里需要用它的方法）
#include "timer.h"          // 定时器完整定义
#include <cstdio>           // printf

// ==================== 获取全局唯一实例（单例模式的经典实现） ====================
Reactor& Reactor::get_instance()
{
    static Reactor instance;  // C++11 保证局部静态变量的初始化是线程安全的
    return instance;          // 返回引用
}

// ==================== 私有构造函数 ====================
Reactor::Reactor()
    : impl_(std::make_unique<ReactorImplementation>())  // 创建内部实现
    , thread_pool_(std::make_unique<ThreadPool>(4))      // 创建线程池，默认 4 个线程
    , timer_heap_(std::make_unique<TimerHeap>())          // 创建定时器堆
{
}

// ==================== 析构函数 ====================
Reactor::~Reactor() = default;  // unique_ptr 自动释放，无需手动清理

// ==================== 注册 Handler（转发给内部实现） ====================
void Reactor::register_handler(HandlerPtr handler, uint32_t evt)
{
    impl_->register_handler(handler, evt);
}

// ==================== 移除 Handler（转发给内部实现） ====================
void Reactor::remove_handler(Handle handle)
{
    impl_->remove_handler(handle);
}

// ==================== 修改 Handler 事件（转发给内部实现） ====================
void Reactor::modify_handler(Handle handle, uint32_t evt)
{
    impl_->modify_handler(handle, evt);
}

// ==================== 事件主循环（定时器集成版） ====================
void Reactor::event_loop(int default_timeout_ms)
{
    impl_->start_loop();  // 标记事件循环开始
    printf("[Reactor] entering event loop, default timeout=%dms\n", default_timeout_ms);

    while (impl_->is_running()) {  // 只要没被 stop()，就一直循环

        // ===== 计算本次 epoll_wait 的超时时间 =====
        // 先看看最近有没有定时器要到期
        int timeout = timer_heap_->get_next_timeout_ms();  // -1 表示没有定时器
        if (timeout < 0) {
            // 没有定时器，用默认超时（100ms）
            timeout = default_timeout_ms;
        } else if (default_timeout_ms >= 0 && timeout > default_timeout_ms) {
            // 定时器还很远，但默认超时更短，用默认超时
            // 这样可以保证事件循环每隔 default_timeout_ms 至少醒来一次
            timeout = default_timeout_ms;
        }

        // ===== 等待并分发 I/O 事件 =====
        impl_->wait_and_dispatch(timeout);

        // ===== 触发所有到期的定时器 =====
        timer_heap_->tick();
    }

    printf("[Reactor] event loop stopped\n");
}

// ==================== 停止事件循环 ====================
void Reactor::stop()
{
    impl_->stop();  // 设置 running_ = false，下一轮循环检测到后退出
}

// ==================== 获取底层多路分解器 ====================
EventDemultiplexer* Reactor::get_demux()
{
    return impl_->get_demux();
}

// ==================== 获取线程池 ====================
ThreadPool& Reactor::get_thread_pool()
{
    return *thread_pool_;
}

// ==================== 获取定时器堆 ====================
TimerHeap& Reactor::get_timer_heap()
{
    return *timer_heap_;
}
