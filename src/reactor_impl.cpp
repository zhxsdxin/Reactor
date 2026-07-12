#include "reactor_impl.h"            // 自己的头文件
#include "epoll_demultiplexer.h"     // epoll 版多路分解器
#include "select_demultiplexer.h"    // select 版多路分解器
#include <cstdio>                    // printf, perror

// ==================== 构造函数：创建底层多路分解器 ====================
ReactorImplementation::ReactorImplementation()
{
    // 用条件编译选择多路分解器
    // 默认用 epoll；编译时加 -DUSE_SELECT 可切为 select
#ifdef USE_SELECT
    demux_ = std::make_unique<SelectDemultiplexer>();   // 创建 select 版
#else
    demux_ = std::make_unique<EpollDemultiplexer>();    // 创建 epoll 版（默认）
#endif
    printf("[Reactor] using %s demultiplexer\n", demux_->name());
}

// ==================== 构造函数（手动指定多路分解器） ====================
ReactorImplementation::ReactorImplementation(EventDemultiplexer* demux)
    : demux_(demux)  // 初始化列表：接管传入的多路分解器
{
    printf("[Reactor] using %s demultiplexer\n", demux_->name());
}

// ==================== 析构函数 ====================
ReactorImplementation::~ReactorImplementation()
{
    stop();  // 通知事件循环停止
}

// ==================== 注册 Handler ====================
void ReactorImplementation::register_handler(HandlerPtr handler, uint32_t evt)
{
    Handle handle = handler->get_handle();  // 获取这个 Handler 管理的 fd
    if (handle == INVALID_HANDLE) return;   // fd 无效，直接返回

    handler->set_events(evt);  // 让 Handler 记住自己关心哪些事件

    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁：保护 handlers_ 映射表
        handlers_[handle] = handler;                 // 把 fd -> Handler 加入映射表
    }  // 锁在这里自动释放（RAII）

    demux_->regist(handle, evt);  // 告诉底层多路分解器：帮我盯着这个 fd
}

// ==================== 移除 Handler ====================
void ReactorImplementation::remove_handler(Handle handle)
{
    demux_->remove(handle);  // 先告诉底层多路分解器：别再盯着这个 fd 了

    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁
        handlers_.erase(handle);                    // 从映射表中删除
    }
}

// ==================== 修改 Handler 关心的事件 ====================
void ReactorImplementation::modify_handler(Handle handle, uint32_t evt)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 加锁
        auto it = handlers_.find(handle);            // 在映射表中查找
        if (it != handlers_.end()) {                // 找到了
            it->second->set_events(evt);             // 更新 Handler 自己的事件记录
        }
    }

    demux_->modify(handle, evt);  // 告诉底层多路分解器：这个 fd 关心的事件变了
}

// ==================== 单次等待并分发事件 ====================
int ReactorImplementation::wait_and_dispatch(int timeout_ms)
{
    std::vector<ReadyEvent> ready_events;                  // 存放就绪事件的列表
    int n = demux_->wait_event(ready_events, timeout_ms);  // 阻塞等待事件
    if (n < 0) {
        perror("[Reactor] wait_event error");  // 等待出错，打印日志
        running_ = false;                      // 标记停止
        return n;
    }

    // ===== 遍历所有就绪事件，分发给对应的 Handler =====
    for (const auto& re : ready_events) {
        HandlerPtr handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);  // 加锁
            auto it = handlers_.find(re.handle);        // 查找这个 fd 对应的 Handler
            if (it == handlers_.end()) continue;        // 找不到了（可能已经被移除），跳过
            handler = it->second;                       // 复制 shared_ptr，保证 Handler 在回调期间不被销毁
        }  // 释放锁后再调用回调，避免死锁

        if (!handler) continue;  // Handler 为空（不应该发生），跳过

        // ===== 错误事件优先处理 =====
        // 如果 fd 上发生了错误、挂起、或对端半关闭
        if (re.events & (EVENT_ERROR | EVENT_HANGUP | EVENT_RDHUP)) {
            handler->handle_error();  // 先调用错误处理
            handler->handle_close();  // 再关闭连接
            continue;                 // 不再处理读写事件
        }

        // 可读事件：调用 handle_read()
        if (re.events & EVENT_READABLE) {
            handler->handle_read();
        }

        // 可写事件：调用 handle_write()
        if (re.events & EVENT_WRITABLE) {
            handler->handle_write();
        }
    }

    return n;  // 返回就绪事件数量
}

// ==================== 停止事件循环 ====================
void ReactorImplementation::stop()
{
    running_ = false;  // 设置原子标志位为 false，事件循环检测到后退出
}
