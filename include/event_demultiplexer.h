#pragma once

#include "event_handler.h"
#include <map>
#include <vector>

// 就绪事件：哪个 fd + 发生了什么事件
struct ReadyEvent {
    Handle handle;
    uint32_t events;
};

// 事件多路分解器抽象基类
class EventDemultiplexer {
public:
    EventDemultiplexer() = default;
    virtual ~EventDemultiplexer() = default;

    virtual int wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms = 0) = 0;
    virtual bool regist(Handle handle, uint32_t evt) = 0;
    virtual bool remove(Handle handle) = 0;
    virtual bool modify(Handle handle, uint32_t evt) = 0;

    virtual const char* name() const = 0;
};
