#pragma once  // 防止头文件被重复包含

#include <cstdint>   // 提供 uint32_t 等固定宽度整数类型
#include <memory>    // 提供 std::shared_ptr 智能指针
#include <functional> // 提供 std::function 函数包装器

// ==================== 事件类型枚举 ====================
// 用位掩码表示，一个 fd 可以同时关心多种事件（如可读 + 可写）
enum EventType : uint32_t {
    EVENT_NONE      = 0,      // 0x00: 不关心任何事件
    EVENT_READABLE  = 0x01,   // 0x01: 可读事件（socket上有数据到达）
    EVENT_WRITABLE  = 0x02,   // 0x02: 可写事件（socket发送缓冲区有空闲）
    EVENT_ERROR     = 0x04,   // 0x04: 错误事件
    EVENT_HANGUP    = 0x08,   // 0x08: 连接挂起/对端关闭
    EVENT_RDHUP     = 0x10,   // 0x10: 对端半关闭（只关了写端）
    EVENT_ET        = 0x20,   // 0x20: 边缘触发模式标记（epoll专用）
};

// ==================== 类型别名 ====================
using Handle = int;                         // Handle 就是文件描述符(int)，统一叫法
const Handle INVALID_HANDLE = -1;           // 无效句柄的标记值，类似 nullptr 的感觉

// ==================== EventHandler 抽象基类 ====================
// 所有事件处理器（监听器、连接处理器等）都必须继承这个类
// 实现四个纯虚函数来定义自己的业务逻辑
class EventHandler {
public:
    EventHandler() = default;               // 默认构造函数，啥也不干
    virtual ~EventHandler() = default;      // 虚析构函数，保证子类能正确释放

    // 禁止拷贝：一个socket连接由一个Handler独享，不能随便复制
    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    // ---- 四个纯虚函数，子类必须实现 ----
    virtual Handle get_handle() const = 0;  // 返回这个Handler管理的socket fd
    virtual void handle_read() = 0;         // 当fd可读时，Reactor会调用这个函数
    virtual void handle_write() = 0;        // 当fd可写时，Reactor会调用这个函数
    virtual void handle_error() = 0;        // 当fd出错时，Reactor会调用这个函数
    virtual void handle_close() = 0;        // 关闭连接、释放资源的清理函数

    // ---- 辅助方法，方便查询和修改当前关心的事件 ----
    bool is_readable() const { return events_ & EVENT_READABLE; }   // 当前是否关心可读事件
    bool is_writable() const { return events_ & EVENT_WRITABLE; }   // 当前是否关心可写事件
    uint32_t get_events() const { return events_; }                 // 获取当前事件掩码
    void set_events(uint32_t evt) { events_ = evt; }               // 直接设置事件掩码
    void add_events(uint32_t evt) { events_ |= evt; }              // 追加事件（按位或）
    void del_events(uint32_t evt) { events_ &= ~evt; }             // 删除事件（按位与非）

protected:
    uint32_t events_ = EVENT_NONE;  // 当前这个Handler关心的事件掩码，默认啥也不关心
};

// HandlerPtr 是 EventHandler 的共享指针别名
// 用 shared_ptr 管理生命周期，避免野指针和内存泄漏
using HandlerPtr = std::shared_ptr<EventHandler>;
