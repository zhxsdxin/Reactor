#pragma once

#include "reactor_impl.h"    // 内部实现类
#include "event_handler.h"   // HandlerPtr 类型定义
#include <memory>            // std::unique_ptr

// 前置声明：头文件里只声明不包含，减少编译依赖
class ThreadPool;
class TimerHeap;

// ==================== Reactor 单例类 ====================
// Reactor 是整个框架的门面（Facade），采用单例模式 + pimpl 桥接模式：
//   - 单例模式：全局只有一个 Reactor 实例，通过 get_instance() 获取
//   - pimpl 模式：接口和实现分离，Reactor 只是空壳，实际工作交给 ReactorImplementation
//
// 额外集成了线程池和定时器，作为加分项
class Reactor {
public:
    static Reactor& get_instance();  // 获取全局唯一实例（第一次调用时自动创建）

    // ---- 事件管理接口（转发给 ReactorImplementation） ----
    void register_handler(HandlerPtr handler, uint32_t evt);  // 注册事件处理器
    void remove_handler(Handle handle);                        // 移除事件处理器
    void modify_handler(Handle handle, uint32_t evt);          // 修改处理器关心的事件

    // ---- 事件循环 ----
    // 进入主循环，持续等待事件并分发，直到调用 stop()
    // default_timeout_ms: epoll_wait 的默认超时（如果没定时器），默认100ms
    void event_loop(int default_timeout_ms = 100);
    void stop();  // 停止事件循环

    // ---- 访问器 ----
    EventDemultiplexer* get_demux();   // 获取底层多路分解器（用于调试）
    ThreadPool& get_thread_pool();     // 获取线程池（用于耗时任务）
    TimerHeap& get_timer_heap();       // 获取定时器堆（用于定时任务）

    ~Reactor();  // 析构函数

private:
    Reactor();  // 构造函数私有，外部只能通过 get_instance() 获取

    // 禁止拷贝：单例不能复制
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    std::unique_ptr<ReactorImplementation> impl_;   // pimpl：真正的实现
    std::unique_ptr<ThreadPool> thread_pool_;       // 线程池（加分项）
    std::unique_ptr<TimerHeap> timer_heap_;         // 定时器堆（加分项）
};
