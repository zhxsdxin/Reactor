#pragma once

#include "event_demultiplexer.h"
#include <map>
#include <mutex>
#include <atomic>

class ReactorImplementation {
public:
    ReactorImplementation();
    explicit ReactorImplementation(EventDemultiplexer* demux);
    ~ReactorImplementation();

    ReactorImplementation(const ReactorImplementation&) = delete;
    ReactorImplementation& operator=(const ReactorImplementation&) = delete;

    void register_handler(HandlerPtr handler, uint32_t evt);
    void remove_handler(Handle handle);
    void modify_handler(Handle handle, uint32_t evt);

    int wait_and_dispatch(int timeout_ms);

    void start_loop() { running_ = true; }
    void stop();
    bool is_running() const { return running_; }

    EventDemultiplexer* get_demux() { return demux_.get(); }

private:
    std::unique_ptr<EventDemultiplexer> demux_;
    std::map<Handle, HandlerPtr> handlers_;
    std::mutex mutex_;
    std::atomic<bool> running_ = false;
};
