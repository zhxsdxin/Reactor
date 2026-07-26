#pragma once

#include <cstdint>
#include <memory>

// 事件类型：用位掩码表示，一个 fd 可以同时关心多种事件
enum EventType : uint32_t {
    EVENT_NONE      = 0,
    EVENT_READABLE  = 0x01,
    EVENT_WRITABLE  = 0x02,
    EVENT_ERROR     = 0x04,
    EVENT_HANGUP    = 0x08,
    EVENT_RDHUP     = 0x10,
    EVENT_ET        = 0x20,
};

using Handle = int;
const Handle INVALID_HANDLE = -1;

class EventHandler {
public:
    EventHandler() = default;
    virtual ~EventHandler() = default;

    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    virtual Handle get_handle() const = 0;
    virtual void handle_read() = 0;
    virtual void handle_write() = 0;
    virtual void handle_error() = 0;
    virtual void handle_close() = 0;

    bool is_readable() const { return events_ & EVENT_READABLE; }
    bool is_writable() const { return events_ & EVENT_WRITABLE; }
    uint32_t get_events() const { return events_; }
    void set_events(uint32_t evt) { events_ = evt; }
    void add_events(uint32_t evt) { events_ |= evt; }
    void del_events(uint32_t evt) { events_ &= ~evt; }

protected:
    uint32_t events_ = EVENT_NONE;
};

using HandlerPtr = std::shared_ptr<EventHandler>;
