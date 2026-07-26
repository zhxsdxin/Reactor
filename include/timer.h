#pragma once

#include <queue>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>

// 定时器堆：用小顶堆管理定时任务，堆顶是最快到期的
class TimerHeap {
public:
    using TimerCallback = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct TimerEntry {
        int id;
        TimePoint deadline;
        TimerCallback callback;

        bool operator>(const TimerEntry& other) const {
            return deadline > other.deadline;
        }
    };

    TimerHeap() = default;
    ~TimerHeap() = default;

    int add_timer(int timeout_ms, TimerCallback cb);  // 返回定时器 ID
    int get_next_timeout_ms();  // 最近一个定时器还有多少毫秒到期
    void tick();                // 触发所有到期的定时器

private:
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> heap_;
    std::atomic<int> next_id_ = 0;
};
