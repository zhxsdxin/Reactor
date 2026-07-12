#include "sock_handler.h"  // 自己的头文件
#include "reactor.h"        // 需要 modify_handler 来切换事件
#include "thread_pool.h"    // 预留：将来可以用线程池处理耗时任务
#include <unistd.h>         // read(), write(), close()
#include <fcntl.h>          // fcntl() 设置非阻塞
#include <cerrno>           // errno, EAGAIN, EWOULDBLOCK
#include <cstdio>           // printf, perror, fprintf
#include <cstring>          // memset()
#include <algorithm>        // std::copy, std::min 等

// ==================== 构造函数：保存客户端 fd ====================
SockHandler::SockHandler(Handle fd)
    : sock_fd_(fd)  // 保存客户端 socket 的 fd
{
    memset(read_buffer_, 0, BUFFER_SIZE);  // 读缓冲区清零（好习惯）
}

// ==================== 析构函数：关闭客户端 socket ====================
SockHandler::~SockHandler()
{
    handle_close();  // 确保 socket 被关闭
}

// ==================== 把客户端 socket 设为非阻塞 ====================
void SockHandler::set_nonblocking()
{
    if (sock_fd_ == INVALID_HANDLE) return;  // fd 无效，不操作

    int flags = fcntl(sock_fd_, F_GETFL, 0);     // 获取当前标志位
    if (flags < 0) {
        perror("[SockHandler] fcntl F_GETFL failed");
        return;
    }
    // 加上 O_NONBLOCK 标志，所有读写操作都不会阻塞
    if (fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[SockHandler] fcntl F_SETFL failed");
    }
}

// ==================== 处理可读事件：读数据并 Echo 回去 ====================
void SockHandler::handle_read()
{
    if (sock_fd_ == INVALID_HANDLE) return;  // 已经关闭了，不再处理

    // ===== 非阻塞读，读到 EAGAIN 为止 =====
    while (true) {
        // read() 从 socket 读数据到 read_buffer_
        // 返回值 n：
        //   > 0  : 读到了 n 个字节
        //   = 0  : 对端关闭了连接（EOF）
        //   < 0  : 出错（可能是 EAGAIN 表示暂无数据）
        ssize_t n = read(sock_fd_, read_buffer_, BUFFER_SIZE);
        if (n > 0) {
            printf("[SockHandler] fd=%d read %zd bytes, echoing...\n", sock_fd_, n);

            // ===== 把读到的东西追加到写缓冲区 =====
            {
                std::lock_guard<std::mutex> lock(write_mutex_);    // 加锁保护写缓冲区
                write_buffer_.insert(write_buffer_.end(),          // 追加到 vector 末尾
                                     read_buffer_, read_buffer_ + n);
            }

            // ===== 尝试立即写回去（Echo 的核心逻辑） =====
            handle_write();

        } else if (n == 0) {
            // read 返回 0：客户端正常关闭了连接（发了 FIN）
            printf("[SockHandler] fd=%d client disconnected\n", sock_fd_);
            handle_close();                              // 关闭 socket
            Reactor::get_instance().remove_handler(sock_fd_);  // 从 Reactor 移除
            return;

        } else {
            // read 返回 -1
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 非阻塞模式下没有更多数据了，退出循环
            }
            // 真正的错误（如连接被重置）
            perror("[SockHandler] read error");
            handle_error();
            handle_close();
            Reactor::get_instance().remove_handler(sock_fd_);
            return;
        }
    }
}

// ==================== 处理可写事件：把写缓冲区里的数据发出去 ====================
void SockHandler::handle_write()
{
    if (sock_fd_ == INVALID_HANDLE) return;  // 已经关闭了，不再处理

    std::lock_guard<std::mutex> lock(write_mutex_);  // 加锁保护写缓冲区

    if (write_buffer_.empty()) return;  // 没有数据要写，直接返回

    // ===== 非阻塞写，写到缓冲区空或 EAGAIN 为止 =====
    while (!write_buffer_.empty()) {
        // write() 把写缓冲区的数据发到 socket
        // 返回值 n：
        //   > 0  : 成功发送了 n 个字节
        //   < 0  : 出错（EAGAIN = 发送缓冲区满了，暂时发不出去）
        ssize_t n = write(sock_fd_, write_buffer_.data(), write_buffer_.size());
        if (n > 0) {
            // 发送成功了 n 字节，把它们从缓冲区头部删掉
            write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);

        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // ===== 发送缓冲区满了，暂时发不出去 =====
            // 告诉 Reactor：我除了关心可读，现在也关心可写
            // 等 socket 可写时，Reactor 会再调用 handle_write() 继续发送剩余数据
            Reactor::get_instance().modify_handler(sock_fd_, EVENT_READABLE | EVENT_WRITABLE);
            return;  // 返回，等下次可写事件再继续

        } else {
            // 真正的写入错误
            perror("[SockHandler] write error");
            handle_error();
            handle_close();
            Reactor::get_instance().remove_handler(sock_fd_);
            return;
        }
    }

    // ===== 写缓冲区清空了，说明数据全部发送完毕 =====
    // 恢复为只关心可读事件，不再关心可写（避免无意义的可写事件轰炸）
    if (write_buffer_.empty()) {
        Reactor::get_instance().modify_handler(sock_fd_, EVENT_READABLE);
    }
}

// ==================== 处理错误 ====================
void SockHandler::handle_error()
{
    fprintf(stderr, "[SockHandler] error on fd=%d\n", sock_fd_);
}

// ==================== 关闭连接 ====================
void SockHandler::handle_close()
{
    if (sock_fd_ != INVALID_HANDLE) {              // 如果 fd 还有效
        printf("[SockHandler] closing fd=%d\n", sock_fd_);
        close(sock_fd_);                           // 关闭 socket
        sock_fd_ = INVALID_HANDLE;                 // 标记为无效，防止重复关闭
    }
}
