#pragma once

#include "reactor_impl.h"
#include <memory>

class ThreadPool;
class TimerHeap;

class Reactor {
public:
    static Reactor& get_instance();

    void register_handler(HandlerPtr handler, uint32_t evt);
    void remove_handler(Handle handle);
    void modify_handler(Handle handle, uint32_t evt);
    void event_loop(int default_timeout_ms = 100);
    void stop();

    EventDemultiplexer* get_demux();
    ThreadPool& get_thread_pool();
    TimerHeap& get_timer_heap();

    ~Reactor();

private:
    Reactor();
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    std::unique_ptr<ReactorImplementation> impl_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<TimerHeap> timer_heap_;
};
