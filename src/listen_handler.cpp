#include "listen_handler.h"   // 自己的头文件
#include "sock_handler.h"     // 需要创建 SockHandler 来处理新连接
#include "reactor.h"          // 需要把新连接注册到 Reactor
#include <sys/socket.h>       // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h>       // sockaddr_in, htons, INADDR_ANY
#include <arpa/inet.h>        // inet_pton() 把 IP 字符串转成二进制
#include <fcntl.h>            // fcntl() 设置文件描述符属性
#include <unistd.h>           // close()
#include <cstdio>             // printf, perror, fprintf
#include <cstring>            // memset()
#include <cerrno>             // errno, EAGAIN, EWOULDBLOCK

// ==================== 构造函数：保存 IP 和端口 ====================
ListenHandler::ListenHandler(const std::string& ip, int port)
    : ip_(ip)      // 保存要监听的 IP 地址
    , port_(port)  // 保存要监听的端口号
{
}

// ==================== 析构函数：关闭监听 socket ====================
ListenHandler::~ListenHandler()
{
    handle_close();  // 调用关闭函数，确保 socket 被释放
}

// ==================== 启动监听：socket -> bind -> listen -> 注册到 Reactor ====================
bool ListenHandler::start()
{
    // ----- 步骤1：创建 socket -----
    // AF_INET: IPv4 协议族
    // SOCK_STREAM: TCP 流式套接字
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        perror("[ListenHandler] socket failed");  // 创建失败
        return false;
    }

    // ----- 步骤2：设置 SO_REUSEADDR -----
    // 让端口在 TIME_WAIT 状态下也能被重用，避免"Address already in use"错误
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ----- 步骤3：设为非阻塞模式 -----
    // 非阻塞模式下 accept 不会卡住，没有新连接时返回 EAGAIN
    if (!create_nonblocking_socket()) {
        close(listen_fd_);              // 设置失败，关闭 socket
        listen_fd_ = INVALID_HANDLE;    // 标记为无效
        return false;
    }

    // ----- 步骤4：绑定地址和端口 -----
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));     // 先全部清零
    addr.sin_family = AF_INET;          // IPv4
    addr.sin_port = htons(port_);       // 端口号（htons: 主机字节序 -> 网络字节序）
    if (ip_.empty() || ip_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;  // 监听本机所有网卡的 IP
    } else {
        inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);  // 把字符串 IP 转成二进制
    }

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ListenHandler] bind failed");  // 绑定失败（通常是端口被占用）
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
        return false;
    }

    // ----- 步骤5：开始监听 -----
    // SOMAXCONN: 系统允许的最大连接等待队列长度
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        perror("[ListenHandler] listen failed");  // 监听失败
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
        return false;
    }

    printf("[ListenHandler] listening on %s:%d, fd=%d\n", ip_.c_str(), port_, listen_fd_);

    // ----- 步骤6：把自己注册到 Reactor -----
    // shared_from_this() 获取自己的 shared_ptr，安全地交给 Reactor 管理
    auto self = shared_from_this();
    Reactor::get_instance().register_handler(self, EVENT_READABLE);  // 只关心可读（新连接到来）
    return true;
}

// ==================== 把 socket 设为非阻塞模式 ====================
bool ListenHandler::create_nonblocking_socket()
{
    // F_GETFL: 获取当前文件描述符的标志位
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags < 0) {
        perror("[ListenHandler] fcntl F_GETFL failed");
        return false;
    }
    // F_SETFL: 设置新的标志位
    // O_NONBLOCK: 非阻塞标志，加上去之后所有 I/O 操作不会卡住
    if (fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("[ListenHandler] fcntl F_SETFL failed");
        return false;
    }
    return true;
}

// ==================== 处理可读事件：accept 新连接 ====================
void ListenHandler::handle_read()
{
    // 用 while 循环一次性 accept 所有等待中的连接
    // 因为是非阻塞模式，没有更多连接时 accept 返回 -1 (errno=EAGAIN)
    while (true) {
        struct sockaddr_in client_addr;           // 存放客户端地址信息
        socklen_t addr_len = sizeof(client_addr); // 地址结构体的大小

        // ===== accept：接受一个新连接 =====
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0) {
            // accept 返回负数
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 非阻塞模式下没有更多连接了，退出循环
            }
            perror("[ListenHandler] accept failed");  // 真正的错误
            break;
        }

        printf("[ListenHandler] new connection fd=%d\n", client_fd);

        // ===== 为新连接创建 SockHandler 并注册到 Reactor =====
        auto sock_handler = std::make_shared<SockHandler>(client_fd);  // 创建连接处理器
        sock_handler->set_nonblocking();                                // 设为非阻塞
        Reactor::get_instance().register_handler(sock_handler, EVENT_READABLE);  // 注册可读事件
    }
}

// ==================== 处理可写事件：监听 socket 不需要 ====================
void ListenHandler::handle_write()
{
    // 监听 socket 不关心可写，空实现
}

// ==================== 处理错误 ====================
void ListenHandler::handle_error()
{
    fprintf(stderr, "[ListenHandler] error on listen fd=%d\n", listen_fd_);
}

// ==================== 关闭监听 socket ====================
void ListenHandler::handle_close()
{
    if (listen_fd_ != INVALID_HANDLE) {         // 如果 fd 还有效
        printf("[ListenHandler] closing listen fd=%d\n", listen_fd_);
        close(listen_fd_);                      // 关闭 socket
        listen_fd_ = INVALID_HANDLE;            // 标记为无效，防止重复关闭
    }
}
