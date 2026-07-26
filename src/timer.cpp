#include "timer.h"
#include <algorithm>

int TimerHeap::add_timer(int timeout_ms, TimerCallback cb)
{
    int id = next_id_++;
    auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    heap_.push({id, deadline, std::move(cb)});
    return id;
}

int TimerHeap::get_next_timeout_ms()
{
    if (heap_.empty()) return -1;

    auto now = Clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        heap_.top().deadline - now);
    return std::max<int>(0, static_cast<int>(delta.count()));
}

void TimerHeap::tick()
{
    auto now = Clock::now();
    while (!heap_.empty()) {
        auto& top = heap_.top();
        if (top.deadline > now) break;
        top.callback();
        heap_.pop();
    }
}
