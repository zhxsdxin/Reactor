#include "timer.h"     // 自己的头文件
#include <algorithm>   // std::max

// ==================== 添加一个定时器 ====================
int TimerHeap::add_timer(int timeout_ms, TimerCallback cb)
{
    int id = next_id_++;  // 原子自增，生成唯一 ID（线程安全）
    // 计算到期时间 = 当前时间 + 超时毫秒数
    auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);

    // 把定时器条目推入小顶堆
    heap_.push({id, deadline, std::move(cb)});

    cancelled_[id] = false;  // 记录这个 ID 没有被取消
    return id;               // 返回 ID，用户可以用它来取消定时器
}

// ==================== 取消一个定时器（懒惰删除） ====================
void TimerHeap::cancel_timer(int timer_id)
{
    // 不直接从堆里删（优先队列不支持随机删除），而是标记为已取消
    // 等到 tick() 或 get_next_timeout_ms() 碰到它时再真正跳过
    cancelled_[timer_id] = true;
}

// ==================== 获取最近一个定时器还有多少毫秒到期 ====================
int TimerHeap::get_next_timeout_ms()
{
    // ===== 懒惰删除：跳过堆顶被取消的定时器 =====
    while (!heap_.empty()) {
        int id = heap_.top().id;                    // 堆顶定时器的 ID
        auto it = cancelled_.find(id);              // 查找取消记录
        if (it != cancelled_.end() && it->second) { // 被取消了
            cancelled_.erase(it);                    // 清理取消记录
            heap_.pop();                             // 弹出堆顶（丢弃）
        } else {
            break;  // 堆顶没被取消，保留
        }
    }

    if (heap_.empty()) return -1;  // 没有定时器了，返回 -1

    // 计算距离到期还有多少毫秒
    auto now = Clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        heap_.top().deadline - now);  // 到期时间 - 当前时间 = 剩余时间

    // 如果已经过期了（delta < 0），返回 0（立即触发）
    // std::max 保证不返回负数
    return std::max<int>(0, static_cast<int>(delta.count()));
}

// ==================== 触发所有已到期的定时器 ====================
void TimerHeap::tick()
{
    auto now = Clock::now();  // 获取当前时间（只调一次，性能更好）

    while (!heap_.empty()) {
        auto& top = heap_.top();  // 引用堆顶（最早到期的）

        // ===== 检查是否被取消了 =====
        auto it = cancelled_.find(top.id);
        if (it != cancelled_.end() && it->second) {
            cancelled_.erase(it);  // 清理取消记录
            heap_.pop();            // 丢弃
            continue;               // 继续检查下一个
        }

        // ===== 检查是否到期了 =====
        if (top.deadline > now) break;  // 还没到期，堆里后面的一定也没到期

        // ===== 到期了，执行回调 =====
        top.callback();           // 调用用户设置的回调函数
        cancelled_.erase(top.id); // 清理可能的取消记录
        heap_.pop();              // 弹出执行完毕的定时器
    }
}
