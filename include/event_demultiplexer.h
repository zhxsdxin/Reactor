#pragma once  // 防止头文件被重复包含

#include "event_handler.h"  // 需要用到 Handle、HandlerPtr 等类型
#include <map>              // 用 std::map 存 handle -> Handler 的映射
#include <vector>           // 用 std::vector 存就绪事件列表

// ==================== ReadyEvent 结构体 ====================
// 表示一个已经就绪的事件：哪个fd + 发生了什么事件
struct ReadyEvent {
    Handle handle;      // 就绪的文件描述符
    uint32_t events;    // 就绪的事件类型（可能是 EVENT_READABLE | EVENT_WRITABLE 等组合）
};

// ==================== EventDemultiplexer 抽象基类 ====================
// 事件多路分解器，负责跟操作系统打交道（epoll / select）
// 把"哪些fd上有事件发生"这个事情抽象出来
class EventDemultiplexer {
public:
    EventDemultiplexer() = default;         // 默认构造函数
    virtual ~EventDemultiplexer() = default; // 虚析构，子类可以正确清理

    // ---- 四个纯虚函数，子类（epoll版/select版）必须实现 ----

    // 阻塞等待事件发生，把就绪的fd和事件类型填到 ready_events 里
    // timeout_ms: 超时毫秒数，0=立即返回，-1=永久阻塞
    // 返回值：就绪事件数量，<0 表示出错
    virtual int wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms = 0) = 0;

    // 注册一个fd，告诉内核"我关心这个fd上的哪些事件"
    // handle: 要注册的fd
    // evt: 关心的事件掩码（EVENT_READABLE | EVENT_WRITABLE 等）
    virtual bool regist(Handle handle, uint32_t evt) = 0;

    // 从内核监听列表中移除一个fd
    virtual bool remove(Handle handle) = 0;

    // 修改一个已注册fd的关心事件（比如从"只读"改为"只写"）
    virtual bool modify(Handle handle, uint32_t evt) = 0;

    // 返回这个多路分解器的名字，用于调试打印
    virtual const char* name() const = 0;
};
