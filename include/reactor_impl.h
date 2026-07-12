#pragma once

#include "event_demultiplexer.h"  // 持有事件多路分解器的指针
#include <map>                    // 用 std::map 维护 fd -> Handler 的映射表
#include <mutex>                  // 用 mutex 保护 handlers_ 映射表（多线程安全）
#include <atomic>                 // 用 atomic<bool> 做无锁的运行状态标志
#include <functional>             // 用 std::function 做回调

// ==================== ReactorImplementation 类 ====================
// Reactor 的核心实现类，负责：
// 1. 维护 fd -> Handler 的映射表
// 2. 注册/移除/修改 Handler 关心的事件
// 3. 调用底层多路分解器等待事件，并分发给对应的 Handler
// 
// 这个类通过 pimpl（指针实现）模式被 Reactor 单例包装
class ReactorImplementation {
public:
    ReactorImplementation();                          // 默认构造：自动选择 epoll 或 select
    explicit ReactorImplementation(EventDemultiplexer* demux);  // 手动指定多路分解器
    ~ReactorImplementation();                         // 析构：停止事件循环

    // 禁止拷贝：Reactor 实现是唯一的
    ReactorImplementation(const ReactorImplementation&) = delete;
    ReactorImplementation& operator=(const ReactorImplementation&) = delete;

    // ---- 对外接口 ----

    // 注册一个 Handler：把它加入映射表，并告诉内核关心哪些事件
    void register_handler(HandlerPtr handler, uint32_t evt);

    // 移除一个 Handler：从映射表删除，从内核监听列表删除
    void remove_handler(Handle handle);

    // 修改 Handler 关心的事件（比如从"只读"切换为"只写"）
    void modify_handler(Handle handle, uint32_t evt);

    // 单次等待并分发事件：阻塞等待，有事件就绪时分发给对应 Handler
    // timeout_ms: 等待超时（毫秒）
    // 返回就绪事件数量
    int wait_and_dispatch(int timeout_ms);

    void start_loop() { running_ = true; }  // 标记事件循环开始运行
    void stop();                             // 标记事件循环应该停止
    bool is_running() const { return running_; }  // 查询是否正在运行

    EventDemultiplexer* get_demux() { return demux_.get(); }  // 获取底层多路分解器

private:
    std::unique_ptr<EventDemultiplexer> demux_;   // 底层多路分解器（epoll 或 select）
    std::map<Handle, HandlerPtr> handlers_;       // fd -> Handler 映射表
    std::mutex mutex_;                             // 保护 handlers_ 的互斥锁
    std::atomic<bool> running_ = false;            // 事件循环是否正在运行（原子变量，多线程安全读写）
};
