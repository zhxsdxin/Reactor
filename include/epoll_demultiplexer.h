#pragma once

#include "event_demultiplexer.h"  // 继承自事件多路分解器抽象基类
#include <sys/epoll.h>            // Linux epoll 相关结构体：epoll_event, epoll_create1 等
#include <vector>                 // 用 std::vector 存 epoll_wait 返回的事件数组

// ==================== EpollDemultiplexer 类 ====================
// 基于 Linux epoll 的事件多路分解器实现
// epoll 是 Linux 上性能最好的 I/O 多路复用机制，适合海量连接场景
class EpollDemultiplexer : public EventDemultiplexer {
public:
    EpollDemultiplexer();                   // 构造函数：创建 epoll 实例
    ~EpollDemultiplexer() override;         // 析构函数：关闭 epoll fd，释放资源

    // ---- 实现父类的四个纯虚函数 ----
    int wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms = 0) override;
    bool regist(Handle handle, uint32_t evt) override;
    bool remove(Handle handle) override;
    bool modify(Handle handle, uint32_t evt) override;

    const char* name() const override { return "epoll"; }  // 返回名字 "epoll"

private:
    int epoll_fd_ = -1;                    // epoll_create1 返回的 epoll 实例 fd
    static const int MAX_EVENTS = 1024;    // 每次 epoll_wait 最多返回的事件数
    std::vector<epoll_event> evs_;         // 预分配的 epoll_event 数组，复用不重复分配
};
