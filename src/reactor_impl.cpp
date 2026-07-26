#include "reactor_impl.h"
#include "epoll_demultiplexer.h"
#include "select_demultiplexer.h"
#include <cstdio>

ReactorImplementation::ReactorImplementation()
{
#ifdef USE_SELECT
    demux_ = std::make_unique<SelectDemultiplexer>();
#else
    demux_ = std::make_unique<EpollDemultiplexer>();
#endif
    printf("[Reactor] using %s\n", demux_->name());
}

ReactorImplementation::ReactorImplementation(EventDemultiplexer* demux)
    : demux_(demux)
{
    printf("[Reactor] using %s\n", demux_->name());
}

ReactorImplementation::~ReactorImplementation()
{
    stop();
}

void ReactorImplementation::register_handler(HandlerPtr handler, uint32_t evt)
{
    Handle handle = handler->get_handle();
    if (handle == INVALID_HANDLE) return;

    handler->set_events(evt);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[handle] = handler;
    }
    demux_->regist(handle, evt);
}

void ReactorImplementation::remove_handler(Handle handle)
{
    if (handle == INVALID_HANDLE) return;
    demux_->remove(handle);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(handle);
    }
}

void ReactorImplementation::modify_handler(Handle handle, uint32_t evt)
{
    if (handle == INVALID_HANDLE) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(handle);
        if (it != handlers_.end()) {
            it->second->set_events(evt);
        }
    }
    demux_->modify(handle, evt);
}

int ReactorImplementation::wait_and_dispatch(int timeout_ms)
{
    std::vector<ReadyEvent> ready_events;
    int n = demux_->wait_event(ready_events, timeout_ms);
    if (n < 0) {
        running_ = false;
        return n;
    }

    for (const auto& re : ready_events) {
        HandlerPtr handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(re.handle);
            if (it == handlers_.end()) continue;
            handler = it->second;
        }

        if (!handler) continue;

        if (re.events & (EVENT_ERROR | EVENT_HANGUP | EVENT_RDHUP)) {
            handler->handle_error();
            handler->handle_close();
            continue;
        }
        if (re.events & EVENT_READABLE) {
            handler->handle_read();
        }
        if (re.events & EVENT_WRITABLE) {
            handler->handle_write();
        }
    }

    return n;
}

void ReactorImplementation::stop()
{
    running_ = false;
}
