#pragma once

#include <queue>       // std::priority_queue 优先队列（最小堆）
#include <vector>      // std::vector 作为优先队列的底层容器
#include <map>         // std::map 记录哪些定时器被取消了
#include <functional>  // std::function 包装定时器回调
#include <chrono>      // std::chrono::steady_clock 高精度稳定时钟
#include <atomic>      // std::atomic<int> 原子递增的定时器ID

// ==================== TimerHeap 定时器堆类 ====================
// 基于最小堆实现的定时器管理器
// 所有定时器按到期时间排序，每次只检查堆顶是否到期
// Reactor 在每轮事件循环后调用 tick() 来触发到期的定时器
class TimerHeap {
public:
    using TimerCallback = std::function<void()>;  // 定时器回调类型：无参无返回值
    using Clock = std::chrono::steady_clock;      // 用 steady_clock：单调递增，不受系统时间调整影响
    using TimePoint = Clock::time_point;           // 时间点类型

    // ---- 定时器条目 ----
    struct TimerEntry {
        int id;                    // 定时器唯一ID，用于取消
        TimePoint deadline;        // 到期时间点
        TimerCallback callback;    // 到期后要执行的回调函数

        // 重载 > 运算符，优先队列默认是大顶堆，用 greater 反转成小顶堆
        bool operator>(const TimerEntry& other) const {
            return deadline > other.deadline;  // 截止时间越早，优先级越高
        }
    };

    TimerHeap() = default;   // 默认构造函数
    ~TimerHeap() = default;  // 默认析构函数

    int add_timer(int timeout_ms, TimerCallback cb);  // 添加定时器，返回定时器ID
    void cancel_timer(int timer_id);                    // 根据ID取消定时器
    int get_next_timeout_ms();                          // 获取最近一个定时器还有多少毫秒到期
    void tick();                                         // 触发所有已到期的定时器

private:
    // 优先队列：底层用 vector，用 greater 比较 => 小顶堆，堆顶是最早到期的
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> heap_;
    std::atomic<int> next_id_ = 0;          // 自增的定时器ID生成器
    std::map<int, bool> cancelled_;          // 被取消的定时器ID集合（懒惰删除标记）
};
