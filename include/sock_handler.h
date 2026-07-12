#pragma once

#include "event_handler.h"  // 继承自事件处理器基类
#include <string>           // 用 std::string
#include <vector>           // 用 std::vector<char> 做写缓冲区
#include <mutex>            // 用 std::mutex 保护写缓冲区（线程池场景下可能需要）
#include <atomic>           // std::atomic<Handle>：线程池和事件循环可能同时访问 sock_fd_

// ==================== SockHandler 类（连接处理器） ====================
// 负责处理单个客户端连接的读写，实现 Echo 功能：
//   - 收到什么数据就原样发回去（echo）
//   - 非阻塞 I/O + 缓冲区，正确处理半读半写
//
// 同样继承 enable_shared_from_this，便于在回调中安全获取自己的 shared_ptr
class SockHandler : public EventHandler, public std::enable_shared_from_this<SockHandler> {
public:
    explicit SockHandler(Handle fd);  // 构造函数：传入已 accept 得到的客户端 fd
    ~SockHandler() override;          // 析构函数：关闭客户端 socket

    Handle get_handle() const override { return sock_fd_.load(); }  // 原子读，返回客户端 socket 的 fd

    void handle_read() override;   // 可读事件：从 socket 读数据，echo 回去
    void handle_write() override;  // 可写事件：把写缓冲区里的数据发送出去
    void handle_error() override;  // 出错：打印日志
    void handle_close() override;  // 关闭连接：close socket

    void set_nonblocking();  // 把客户端 socket 设为非阻塞模式

private:
    // sock_fd_ 用 atomic 保护：
    //   事件循环线程写（handle_close 设为 -1）
    //   线程池 worker 读（lambda 里传给 modify_handler）
    // 不用 atomic 就是 undefined behavior
    void graceful_shutdown(Handle fd);  // 优雅关闭：先 shutdown 发 FIN，等对面确认后 close

    std::atomic<Handle> sock_fd_ = INVALID_HANDLE;
    bool closing_ = false;                       // 标记正在优雅关闭（只由事件循环线程访问）
    std::atomic<int> pending_tasks_ = 0;         // 线程池中未完成的任务数：提交+1，完成-1
    // 只有 closing_ && pending_tasks_==0 && write_buffer_空 时才真正 close
    static const size_t BUFFER_SIZE = 4096;     // 读缓冲区大小：每次最多读 4KB
    char read_buffer_[BUFFER_SIZE] = {};        // 读缓冲区：从 socket 读数据暂存到这里
    std::vector<char> write_buffer_;             // 写缓冲区：待发送的数据排队在这里
    std::mutex write_mutex_;                     // 保护写缓冲区的锁
};
