#include "sock_handler.h"
#include "reactor.h"
#include "thread_pool.h"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

SockHandler::SockHandler(Handle fd)
    : sock_fd_(fd)
{
    memset(read_buffer_, 0, BUFFER_SIZE);
}

SockHandler::~SockHandler()
{
    if (sock_fd_ != INVALID_HANDLE) {
        close(sock_fd_);
    }
}

void SockHandler::set_nonblocking()
{
    if (sock_fd_ == INVALID_HANDLE) return;
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);
}

void SockHandler::handle_read()
{
    if (sock_fd_ == INVALID_HANDLE) return;

    while (true) {
        ssize_t n = read(sock_fd_, read_buffer_, BUFFER_SIZE);

        if (n > 0) {
            printf("[SockHandler] fd=%d read %zd bytes\n", sock_fd_, n);

            // 交给线程池处理（加分项：handleRead 只读数据，耗时处理丢线程池）
            std::string data(read_buffer_, n);
            auto self = shared_from_this();
            Reactor::get_instance().get_thread_pool().submit(
                [self, data = std::move(data)]() {
                    // 把处理结果放入写缓冲区
                    {
                        std::lock_guard<std::mutex> lock(self->write_mutex_);
                        self->write_buffer_.insert(self->write_buffer_.end(),
                                                   data.begin(), data.end());
                    }
                    // 通知 Reactor 数据准备好了，可以写了
                    Reactor::get_instance().modify_handler(
                        self->get_handle(), EVENT_READABLE | EVENT_WRITABLE);
                });

        } else if (n == 0) {
            // 客户端关闭连接
            printf("[SockHandler] fd=%d disconnected\n", sock_fd_);
            handle_close();
            Reactor::get_instance().remove_handler(sock_fd_);
            return;

        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("[SockHandler] read error");
            handle_error();
            handle_close();
            Reactor::get_instance().remove_handler(sock_fd_);
            return;
        }
    }
}

void SockHandler::handle_write()
{
    if (sock_fd_ == INVALID_HANDLE) return;

    std::lock_guard<std::mutex> lock(write_mutex_);

    if (write_buffer_.empty()) return;

    while (!write_buffer_.empty()) {
        ssize_t n = write(sock_fd_, write_buffer_.data(), write_buffer_.size());

        if (n > 0) {
            write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            Reactor::get_instance().modify_handler(sock_fd_, EVENT_READABLE | EVENT_WRITABLE);
            return;
        } else {
            perror("[SockHandler] write error");
            handle_error();
            handle_close();
            Reactor::get_instance().remove_handler(sock_fd_);
            return;
        }
    }

    // 写完了，恢复只读
    Reactor::get_instance().modify_handler(sock_fd_, EVENT_READABLE);
}

void SockHandler::handle_error()
{
    fprintf(stderr, "[SockHandler] error on fd=%d\n", sock_fd_);
}

void SockHandler::handle_close()
{
    if (sock_fd_ != INVALID_HANDLE) {
        printf("[SockHandler] closing fd=%d\n", sock_fd_);
        close(sock_fd_);
        sock_fd_ = INVALID_HANDLE;
    }
}
