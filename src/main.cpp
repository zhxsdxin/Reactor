#include <csignal>     // signal(), SIGINT, SIGTERM, SIGPIPE, SIG_IGN
#include <cstdio>      // printf, fprintf
#include <cstdlib>     // atoi() 字符串转整数
#include <atomic>      // std::atomic<bool> 原子布尔变量

#include "reactor.h"          // Reactor 单例
#include "listen_handler.h"   // 监听处理器（Acceptor）
#include "sock_handler.h"     // 连接处理器（Echo）
#include "timer.h"            // 定时器堆
#include "thread_pool.h"      // 线程池

// ==================== 全局运行标志（信号处理器会修改它） ====================
// atomic 保证在信号处理器和主循环之间的读写安全
static std::atomic<bool> g_running = true;

// ==================== 信号处理函数 ====================
// 当用户按 Ctrl+C (SIGINT) 或 kill (SIGTERM) 时被调用
static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[main] received signal %d, shutting down...\n", sig);
        g_running = false;                    // 标记退出
        Reactor::get_instance().stop();       // 通知 Reactor 停止事件循环
    }
}

// ==================== 周期性定时器：每 5 秒打印一次状态 ====================
static void add_periodic_timer()
{
    // add_timer 返回定时器 ID，这里我们不保存，因为不需要取消
    Reactor::get_instance().get_timer_heap().add_timer(5000, []() {
        // 这个 lambda 在 5 秒后被调用
        printf("[timer] server is running...\n");

        // ===== 重新添加自己，实现周期性 =====
        // 定时器是一次性的，执行完后不会自动重复
        // 所以回调里重新 add_timer，就变成了"每隔 5 秒执行一次"
        add_periodic_timer();
    });
}

// ==================== 打印用法 ====================
static void print_usage(const char* prog)
{
    printf("Usage: %s [port]\n", prog);        // 程序名
    printf("  port  - listen port (default: 8080)\n");  // 端口参数说明
}

// ==================== 主函数 ====================
int main(int argc, char* argv[])
{
    // ----- 设置信号处理 -----
    signal(SIGINT, signal_handler);   // Ctrl+C -> 优雅退出
    signal(SIGTERM, signal_handler);  // kill 命令 -> 优雅退出
    signal(SIGPIPE, SIG_IGN);         // 忽略 SIGPIPE：防止向已关闭的 socket 写数据时程序崩溃

    // ----- 解析命令行参数：端口号 -----
    int port = 8080;  // 默认监听 8080 端口
    if (argc >= 2) {
        port = atoi(argv[1]);  // 字符串转整数
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            print_usage(argv[0]);
            return 1;  // 参数不合法，退出
        }
    }

    // ----- 打印欢迎信息 -----
    printf("========================================\n");
    printf("  Reactor Echo Server\n");
    printf("  Port: %d\n", port);
    // Reactor::get_instance() 第一次调用时会自动创建单例
    // get_demux()->name() 返回 "epoll" 或 "select"
    printf("  Demux: %s\n", Reactor::get_instance().get_demux()->name());
    printf("========================================\n");

    // ----- 创建并启动监听处理器 -----
    // make_shared 创建 shared_ptr，自动管理生命周期
    auto listen_handler = std::make_shared<ListenHandler>("0.0.0.0", port);
    if (!listen_handler->start()) {
        fprintf(stderr, "[main] failed to start listen handler\n");
        return 1;  // 启动失败（端口被占用等），退出
    }

    // ----- 添加每 5 秒打印一次的定时器 -----
    add_periodic_timer();

    printf("[main] server started, press Ctrl+C to stop\n");

    // ===== 进入事件主循环 =====
    // 这个函数会一直运行，直到 stop() 被调用（信号处理器里调用）
    // 参数 100 表示 epoll_wait 默认每 100ms 超时一次
    //   这样即使没有 I/O 事件，也能定期检查定时器和退出标志
    Reactor::get_instance().event_loop(100);

    printf("[main] server stopped\n");

    return 0;  // 正常退出
}
