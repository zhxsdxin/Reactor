#pragma once

#include "event_demultiplexer.h"  // 继承自事件多路分解器抽象基类
#include <sys/select.h>           // POSIX select 相关：fd_set, FD_ZERO, select() 等

// ==================== SelectDemultiplexer 类 ====================
// 基于 POSIX select 的事件多路分解器实现
// select 是更古老、跨平台的多路复用方式，但有 fd 数量限制（默认1024）
class SelectDemultiplexer : public EventDemultiplexer {
public:
    SelectDemultiplexer();                  // 构造函数：初始化三个 fd_set 为空
    ~SelectDemultiplexer() override = default;  // 析构函数：没啥要清理的

    int wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms = 0) override;
    bool regist(Handle handle, uint32_t evt) override;
    bool remove(Handle handle) override;
    bool modify(Handle handle, uint32_t evt) override;

    const char* name() const override { return "select"; }  // 返回名字 "select"

private:
    fd_set read_set_;       // 读事件集合：哪些 fd 要监听可读
    fd_set write_set_;      // 写事件集合：哪些 fd 要监听可写
    fd_set except_set_;     // 异常事件集合：哪些 fd 要监听异常
    Handle max_fd_ = -1;    // 当前注册的最大 fd 值，select 需要这个参数来优化扫描范围
};
