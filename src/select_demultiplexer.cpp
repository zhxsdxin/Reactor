#include "select_demultiplexer.h"  // 自己的头文件
#include <cstdio>                  // perror()
#include <cerrno>                  // errno

// ==================== 构造函数：初始化三个 fd_set 为空 ====================
SelectDemultiplexer::SelectDemultiplexer()
{
    FD_ZERO(&read_set_);    // 清空读集合
    FD_ZERO(&write_set_);   // 清空写集合
    FD_ZERO(&except_set_);  // 清空异常集合
}

// ==================== 等待事件：调用 select 阻塞等待 ====================
int SelectDemultiplexer::wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms)
{
    ready_events.clear();  // 先清空上次的结果

    // select 会修改传入的 fd_set，所以必须用副本
    fd_set read_fds = read_set_;
    fd_set write_fds = write_set_;
    fd_set except_fds = except_set_;

    // 构造超时结构体
    struct timeval tv, *ptv = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    int nfds = select(max_fd_ + 1, &read_fds, &write_fds, &except_fds, ptv);
    if (nfds < 0) {
        if (errno == EINTR) return 0;  // 被信号中断，不是真正的错误
        perror("select failed");
        return -1;
    }
    if (nfds == 0) return 0;  // 超时，没有事件

    // select 不告诉"谁就绪"，需要自己遍历 0 ~ max_fd_ 挨个检查
    for (Handle fd = 0; fd <= max_fd_ && (int)ready_events.size() < nfds; ++fd) {
        uint32_t evt = EVENT_NONE;
        if (FD_ISSET(fd, &except_fds)) evt |= EVENT_ERROR;     // 异常就绪
        if (FD_ISSET(fd, &read_fds))   evt |= EVENT_READABLE;  // 可读就绪
        if (FD_ISSET(fd, &write_fds))  evt |= EVENT_WRITABLE;  // 可写就绪

        if (evt != EVENT_NONE) {
            ready_events.push_back({fd, evt});
        }
    }

    return nfds;
}

// ==================== 注册事件：把 fd 加入 select 的监听集合 ====================
bool SelectDemultiplexer::regist(Handle handle, uint32_t evt)
{
    if (handle < 0) return false;

    if (evt & EVENT_READABLE) FD_SET(handle, &read_set_);
    if (evt & EVENT_WRITABLE) FD_SET(handle, &write_set_);
    FD_SET(handle, &except_set_);  // 异常事件总是监听

    if (handle > max_fd_) max_fd_ = handle;  // 更新最大 fd
    return true;
}

// ==================== 移除事件：从所有集合中清除 fd ====================
bool SelectDemultiplexer::remove(Handle handle)
{
    if (handle < 0) return false;

    FD_CLR(handle, &read_set_);
    FD_CLR(handle, &write_set_);
    FD_CLR(handle, &except_set_);

    // 如果删掉的是最大的 fd，往回找新的最大 fd
    if (handle == max_fd_) {
        while (max_fd_ >= 0
               && !FD_ISSET(max_fd_, &read_set_)
               && !FD_ISSET(max_fd_, &write_set_)
               && !FD_ISSET(max_fd_, &except_set_)) {
            --max_fd_;
        }
    }
    return true;
}

// ==================== 修改事件：修改已注册 fd 关心的事件类型 ====================
bool SelectDemultiplexer::modify(Handle handle, uint32_t evt)
{
    if (handle < 0) return false;

    if (evt & EVENT_READABLE)
        FD_SET(handle, &read_set_);
    else
        FD_CLR(handle, &read_set_);

    if (evt & EVENT_WRITABLE)
        FD_SET(handle, &write_set_);
    else
        FD_CLR(handle, &write_set_);

    return true;
}
