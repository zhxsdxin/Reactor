#include "epoll_demultiplexer.h"  // 自己的头文件
#include <unistd.h>               // close() 函数
#include <sys/epoll.h>            // epoll_event, epoll_create1, epoll_ctl, epoll_wait
#include <cerrno>                 // errno 错误码
#include <cstdio>                 // perror() 打印错误

// ==================== 辅助函数：把我们的 EventType 转成 epoll 认识的事件 ====================
static uint32_t event_to_epoll(uint32_t evt)
{
    uint32_t ep = 0;  // 初始化为 0

    // 按位检查并转换
    if (evt & EVENT_READABLE)  ep |= EPOLLIN;    // 可读 -> EPOLLIN
    if (evt & EVENT_WRITABLE)  ep |= EPOLLOUT;   // 可写 -> EPOLLOUT
    if (evt & EVENT_ET)        ep |= EPOLLET;    // 边缘触发标记

    // EPOLLERR（错误）、EPOLLHUP（挂起）、EPOLLRDHUP（对端半关闭）
    // 这三个 epoll 默认就会监听，不需要用户显式注册
    ep |= EPOLLERR | EPOLLHUP | EPOLLRDHUP;

    return ep;  // 返回转换后的 epoll 事件掩码
}

// ==================== 辅助函数：把 epoll 返回的事件转成我们的 EventType ====================
static uint32_t epoll_to_event(uint32_t ep)
{
    uint32_t evt = EVENT_NONE;  // 初始化为"无事件"

    if (ep & EPOLLIN)       evt |= EVENT_READABLE;  // EPOLLIN  -> 可读
    if (ep & EPOLLOUT)      evt |= EVENT_WRITABLE;  // EPOLLOUT -> 可写
    if (ep & EPOLLERR)      evt |= EVENT_ERROR;     // EPOLLERR -> 错误
    if (ep & EPOLLHUP)      evt |= EVENT_HANGUP;    // EPOLLHUP -> 挂起
    if (ep & EPOLLRDHUP)    evt |= EVENT_RDHUP;     // EPOLLRDHUP -> 对端半关闭

    return evt;  // 返回我们自己的事件掩码
}

// ==================== 构造函数：创建 epoll 实例 ====================
EpollDemultiplexer::EpollDemultiplexer()
    : evs_(MAX_EVENTS)  // 初始化列表：预分配 1024 个 epoll_event 的空间，避免动态扩容
{
    // epoll_create1(0) 等价于 epoll_create(1024)，但更现代
    // 参数 0 表示没有特殊标志（也可以用 EPOLL_CLOEXEC 让子进程不继承）
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        perror("epoll_create1 failed");  // 创建失败，打印系统错误
    }
}

// ==================== 析构函数：关闭 epoll 实例 ====================
EpollDemultiplexer::~EpollDemultiplexer()
{
    if (epoll_fd_ >= 0) {       // 如果 epoll fd 是有效的
        close(epoll_fd_);       // 关闭它，释放内核资源
        epoll_fd_ = -1;         // 标记为无效，防止重复关闭
    }
}

// ==================== 等待事件：调用 epoll_wait 阻塞等待 ====================
int EpollDemultiplexer::wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms)
{
    ready_events.clear();  // 先清空上次的结果

    if (epoll_fd_ < 0) return -1;  // epoll fd 无效，直接返回错误

    // ===== 核心调用：epoll_wait =====
    //   evs_.data() : 存放就绪事件的数组首地址
    //   MAX_EVENTS  : 数组容量
    //   timeout_ms  : 超时毫秒数（0=立即返回，-1=永久阻塞）
    //   返回值 nfds : 就绪的 fd 数量
    int nfds = epoll_wait(epoll_fd_, evs_.data(), MAX_EVENTS, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) return 0;  // 被信号中断，不是真正的错误，返回0
        perror("epoll_wait failed");   // 其他错误，打印并返回 -1
        return -1;
    }
    if (nfds == 0) return 0;  // 超时到了，没有事件，正常返回 0

    ready_events.reserve(nfds);  // 预分配空间，避免 push_back 时多次扩容
    for (int i = 0; i < nfds; ++i) {
        ReadyEvent re;
        re.handle = evs_[i].data.fd;                    // 就绪的 fd
        re.events = epoll_to_event(evs_[i].events);     // 转换为我们的事件类型
        ready_events.push_back(re);                     // 加入结果列表
    }

    return nfds;  // 返回就绪事件数量
}

// ==================== 注册事件：把 fd 加入 epoll 监听 ====================
bool EpollDemultiplexer::regist(Handle handle, uint32_t evt)
{
    if (epoll_fd_ < 0) return false;  // epoll fd 无效，失败

    epoll_event ev;                         // 构造 epoll_event 结构体
    ev.data.fd = handle;                    // 把 fd 存到 data 里，事件触发时能取出来
    ev.events = event_to_epoll(evt);        // 把我们的事件类型转成 epoll 认识的格式

    // EPOLL_CTL_ADD：往 epoll 实例里添加一个 fd
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, handle, &ev) < 0) {
        perror("epoll_ctl ADD failed");     // 注册失败，打印错误
        return false;
    }
    return true;  // 注册成功
}

// ==================== 移除事件：把 fd 从 epoll 监听中删除 ====================
bool EpollDemultiplexer::remove(Handle handle)
{
    if (epoll_fd_ < 0) return false;  // epoll fd 无效，失败

    // EPOLL_CTL_DEL：从 epoll 实例里删除一个 fd
    // 注意：Linux 内核 2.6.9 之后，删除时 event 参数可以为 nullptr
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, handle, nullptr) < 0) {
        perror("epoll_ctl DEL failed");     // 删除失败，打印错误
        return false;
    }
    return true;  // 删除成功
}

// ==================== 修改事件：修改已注册 fd 的关心事件类型 ====================
bool EpollDemultiplexer::modify(Handle handle, uint32_t evt)
{
    if (epoll_fd_ < 0) return false;  // epoll fd 无效，失败

    epoll_event ev;                         // 构造新的 epoll_event
    ev.data.fd = handle;                    // fd 不变
    ev.events = event_to_epoll(evt);        // 新的事件掩码

    // EPOLL_CTL_MOD：修改已存在的 fd 的监听事件
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, handle, &ev) < 0) {
        perror("epoll_ctl MOD failed");     // 修改失败，打印错误
        return false;
    }
    return true;  // 修改成功
}
