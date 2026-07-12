#pragma once

#include "event_handler.h"  // 继承自事件处理器基类
#include <string>           // 用 std::string 存 IP 地址

// ==================== ListenHandler 类（Acceptor） ====================
// 监听处理器，负责：
// 1. 创建监听 socket，绑定端口，开始 listen
// 2. 当有新连接到来时（可读事件触发），accept 新连接
// 3. 为每个新连接创建 SockHandler 并注册到 Reactor
//
// 继承 enable_shared_from_this，这样在成员函数里可以用 shared_from_this()
// 获取自己的 shared_ptr，安全地把自己注册到 Reactor
class ListenHandler : public EventHandler, public std::enable_shared_from_this<ListenHandler> {
public:
    ListenHandler(const std::string& ip, int port);  // 构造函数：指定监听 IP 和端口
    ~ListenHandler() override;                        // 析构函数：关闭监听 socket

    Handle get_handle() const override { return listen_fd_; }  // 返回监听 socket 的 fd
    void handle_read() override;    // 有可读事件 = 有新连接来了，accept 它
    void handle_write() override;   // 监听 socket 不关心可写，空实现
    void handle_error() override;   // 监听 socket 出错了，打印错误日志
    void handle_close() override;   // 关闭监听 socket，释放资源

    bool start();  // 启动监听：socket -> bind -> listen -> 注册到 Reactor

private:
    bool create_nonblocking_socket();  // 把监听 socket 设为非阻塞模式

    std::string ip_;              // 要监听的 IP 地址（"0.0.0.0" 表示监听所有网卡）
    int port_;                    // 要监听的端口号
    Handle listen_fd_ = INVALID_HANDLE;  // 监听 socket 的 fd，初始化为无效值
};
