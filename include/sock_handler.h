#pragma once

#include "event_handler.h"
#include <string>
#include <vector>
#include <mutex>

// 连接处理器：一个客户端连接对应一个 SockHandler
// 实现 Echo 功能：收到什么就原样发回去
class SockHandler : public EventHandler, public std::enable_shared_from_this<SockHandler> {
public:
    explicit SockHandler(Handle fd);
    ~SockHandler() override;

    Handle get_handle() const override { return sock_fd_; }

    void handle_read() override;
    void handle_write() override;
    void handle_error() override;
    void handle_close() override;

    void set_nonblocking();

private:
    Handle sock_fd_ = INVALID_HANDLE;
    static const size_t BUFFER_SIZE = 1024;  // PDF 要求 1024 字节缓冲区
    char read_buffer_[1024] = {};
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
};
