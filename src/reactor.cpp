#include "reactor.h"
#include "thread_pool.h"
#include "timer.h"
#include <cstdio>

Reactor& Reactor::get_instance()
{
    static Reactor instance;
    return instance;
}

Reactor::Reactor()
    : impl_(std::make_unique<ReactorImplementation>())
    , thread_pool_(std::make_unique<ThreadPool>(4))
    , timer_heap_(std::make_unique<TimerHeap>())
{
}

Reactor::~Reactor() = default;

void Reactor::register_handler(HandlerPtr handler, uint32_t evt)
{
    impl_->register_handler(handler, evt);
}

void Reactor::remove_handler(Handle handle)
{
    impl_->remove_handler(handle);
}

void Reactor::modify_handler(Handle handle, uint32_t evt)
{
    impl_->modify_handler(handle, evt);
}

void Reactor::event_loop(int default_timeout_ms)
{
    impl_->start_loop();
    printf("[Reactor] event loop started\n");

    while (impl_->is_running()) {
        int timeout = timer_heap_->get_next_timeout_ms();
        if (timeout < 0) {
            timeout = default_timeout_ms;
        } else if (default_timeout_ms >= 0 && timeout > default_timeout_ms) {
            timeout = default_timeout_ms;
        }

        impl_->wait_and_dispatch(timeout);
        timer_heap_->tick();
    }

    printf("[Reactor] event loop stopped\n");
}

void Reactor::stop()
{
    impl_->stop();
}

EventDemultiplexer* Reactor::get_demux()
{
    return impl_->get_demux();
}

ThreadPool& Reactor::get_thread_pool()
{
    return *thread_pool_;
}

TimerHeap& Reactor::get_timer_heap()
{
    return *timer_heap_;
}
