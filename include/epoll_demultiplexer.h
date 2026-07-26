#pragma once

#include "event_demultiplexer.h"
#include <sys/epoll.h>
#include <vector>

class EpollDemultiplexer : public EventDemultiplexer {
public:
    EpollDemultiplexer();
    ~EpollDemultiplexer() override;

    int wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms = 0) override;
    bool regist(Handle handle, uint32_t evt) override;
    bool remove(Handle handle) override;
    bool modify(Handle handle, uint32_t evt) override;

    const char* name() const override { return "epoll"; }

private:
    int epoll_fd_ = -1;
    static const int MAX_EVENTS = 1024;
    std::vector<epoll_event> evs_;
};
