#include "sock_handler.h"  // 自己的头文件
#include "reactor.h"        // 需要 modify_handler 来切换事件
#include "thread_pool.h"    // 线程池：handle_read 读完后把处理丢给线程池
#include <unistd.h>         // read(), write(), close(), shutdown()
#include <sys/socket.h>     // SHUT_WR, SHUT_RD
#include <fcntl.h>          // fcntl() 设置非阻塞
#include <cerrno>           // errno, EAGAIN, EWOULDBLOCK
#include <cstdio>           // printf, perror, fprintf
#include <cstring>          // memset()

// ==================== 构造函数：保存客户端 fd ====================
SockHandler::SockHandler(Handle fd)
    : sock_fd_(fd)  // atomic 构造，保存客户端 socket 的 fd
{
    memset(read_buffer_, 0, BUFFER_SIZE);  // 读缓冲区清零（好习惯）
}

// ==================== 析构函数：关闭客户端 socket ====================
SockHandler::~SockHandler()
{
    Handle fd = sock_fd_.exchange(INVALID_HANDLE);
    if (fd != INVALID_HANDLE) {
        close(fd);  // 兜底：如果还没 close，这里关掉
    }
}

// ==================== 把客户端 socket 设为非阻塞 ====================
void SockHandler::set_nonblocking()
{
    Handle fd = sock_fd_.load();
    if (fd == INVALID_HANDLE) return;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("[SockHandler] fcntl F_GETFL failed");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[SockHandler] fcntl F_SETFL failed");
    }
}

// ==================== 处理可读事件：读数据并交给线程池处理 ====================
void SockHandler::handle_read()
{
    Handle fd = sock_fd_.load();
    if (fd == INVALID_HANDLE) return;         // 已经关闭了，不再处理
    if (closing_) return;                     // 正在优雅关闭，不再读新数据

    while (true) {
        ssize_t n = read(fd, read_buffer_, BUFFER_SIZE);

        if (n > 0) {
            printf("[SockHandler] fd=%d read %zd bytes, submitting to thread pool...\n", fd, n);

            std::string data(read_buffer_, n);
            auto self = shared_from_this();
            pending_tasks_++;  // 提交任务前计数+1：确保 handle_write 等所有任务完成才关
            Reactor::get_instance().get_thread_pool().submit(
                [self, data = std::move(data)]() {
                    Handle fd = self->sock_fd_.load();
                    if (fd == INVALID_HANDLE) {
                        self->pending_tasks_--;  // 连接已关，任务作废
                        return;
                    }

                    {
                        std::lock_guard<std::mutex> lock(self->write_mutex_);
                        self->write_buffer_.insert(self->write_buffer_.end(),
                                                   data.begin(), data.end());
                    }
                    // pending_tasks_-- 必须在 modify_handler 之前：
                    // 确保 handle_write 检查计数器时，数据已经写入了 write_buffer_
                    self->pending_tasks_--;
                    Reactor::get_instance().modify_handler(
                        fd, EVENT_READABLE | EVENT_WRITABLE);
                });

        } else if (n == 0) {
            // ===== 客户端发了 FIN（正常关闭写端）=====
            printf("[SockHandler] fd=%d client disconnected, graceful shutdown...\n", fd);
            closing_ = true;  // 标记正在关闭，不再接受新数据

            // 注册可写事件，让 handle_write 来决定何时真正关闭
            // 因为线程池可能还有未完成的任务，数据随时可能到达 write_buffer_
            // 不能在 handle_read 里直接关，必须等 handle_write 确认"数据全发完了"
            Reactor::get_instance().modify_handler(
                fd, EVENT_READABLE | EVENT_WRITABLE);
            return;

        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("[SockHandler] read error");
            handle_error();
            Handle fd2 = sock_fd_.exchange(INVALID_HANDLE);
            if (fd2 != INVALID_HANDLE) close(fd2);
            Reactor::get_instance().remove_handler(fd);
            return;
        }
    }
}

// ==================== 处理可写事件：把写缓冲区里的数据发出去 ====================
void SockHandler::handle_write()
{
    Handle fd = sock_fd_.load();
    if (fd == INVALID_HANDLE) return;

    std::lock_guard<std::mutex> lock(write_mutex_);

    if (write_buffer_.empty()) {
        // 没有要写的数据
        // 三个条件都满足才优雅关闭：
        //   ① closing_ 已设置（客户端发了 FIN 或我们打算关）
        //   ② 线程池没有未完成的任务（所有数据都已到达 write_buffer_）
        //   ③ 写缓冲区为空（所有数据都已发送）
        if (closing_ && pending_tasks_.load() == 0) {
            graceful_shutdown(fd);
            Reactor::get_instance().remove_handler(fd);
        }
        return;
    }

    while (!write_buffer_.empty()) {
        ssize_t n = write(fd, write_buffer_.data(), write_buffer_.size());

        if (n > 0) {
            write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);

        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 发送缓冲区满了，下次可写时继续
            Reactor::get_instance().modify_handler(fd, EVENT_READABLE | EVENT_WRITABLE);
            return;

        } else {
            perror("[SockHandler] write error");
            Handle fd2 = sock_fd_.exchange(INVALID_HANDLE);
            if (fd2 != INVALID_HANDLE) close(fd2);
            Reactor::get_instance().remove_handler(fd);
            return;
        }
    }

    // 全部发完了
    if (closing_ && pending_tasks_.load() == 0) {
        // 优雅关闭：数据已全部发送，线程池也没有待处理任务了
        graceful_shutdown(fd);
        Reactor::get_instance().remove_handler(fd);
    } else {
        // 正常情况：恢复为只关心可读
        Reactor::get_instance().modify_handler(fd, EVENT_READABLE);
    }
}

// ==================== 处理错误 ====================
void SockHandler::handle_error()
{
    fprintf(stderr, "[SockHandler] error on fd=%d\n", sock_fd_.load());
}

// ==================== 关闭连接（外部调用，不走优雅关闭流程） ====================
void SockHandler::handle_close()
{
    Handle fd = sock_fd_.exchange(INVALID_HANDLE);
    if (fd != INVALID_HANDLE) {
        printf("[SockHandler] closing fd=%d\n", fd);
        close(fd);
    }
}

// ==================== 优雅关闭：TCP 四次挥手的主动关闭方 ====================
// 标准流程：
//   ① shutdown(SHUT_WR) → 发 FIN，告诉对方"我说完了"
//   ② 继续读，等对方的 FIN（对方收到我们的 FIN 后也会发 FIN）
//   ③ read 返回 0 → 四个挥手完成 → close
//
// 注意：优雅关闭只适用于"客户端先断开"的场景（handle_read 里触发）
// handle_error 等异常场景直接走 handle_close 的粗鲁关闭
void SockHandler::graceful_shutdown(Handle fd)
{
    printf("[SockHandler] fd=%d graceful shutdown...\n", fd);

    // 第一步：shutdown 写端，内核会发 FIN 给对方
    // 和 close 的区别：shutdown 只关写端，还能继续读；close 直接全关
    if (shutdown(fd, SHUT_WR) < 0) {
        perror("[SockHandler] shutdown SHUT_WR failed");
    }

    // 第二步：继续读，直到 read 返回 0（收到对方的 FIN）
    // 给对方一点时间确认收到我们的 FIN
    char drain[256];
    while (true) {
        ssize_t n = read(fd, drain, sizeof(drain));
        if (n <= 0) break;  // 0 = 对面也关了，<0 = 出错/没数据了
    }

    // 第三步：两边都确认完毕，彻底关闭
    close(fd);
    sock_fd_.store(INVALID_HANDLE);  // 原子写，通知其他线程这个 fd 已废弃
    printf("[SockHandler] fd=%d gracefully closed\n", fd);
}
