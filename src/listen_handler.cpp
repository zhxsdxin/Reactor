#include "listen_handler.h"
#include "sock_handler.h"
#include "reactor.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>

ListenHandler::ListenHandler(const std::string& ip, int port)
    : ip_(ip), port_(port)
{
}

ListenHandler::~ListenHandler()
{
    handle_close();
}

bool ListenHandler::start()
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        perror("[ListenHandler] socket failed");
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (!create_nonblocking_socket()) {
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ListenHandler] bind failed");
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        perror("[ListenHandler] listen failed");
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
        return false;
    }

    printf("[ListenHandler] listening on %s:%d, fd=%d\n", ip_.c_str(), port_, listen_fd_);

    auto self = shared_from_this();
    Reactor::get_instance().register_handler(self, EVENT_READABLE);
    return true;
}

bool ListenHandler::create_nonblocking_socket()
{
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags < 0) return false;
    if (fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    return true;
}

void ListenHandler::handle_read()
{
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("[ListenHandler] accept failed");
            break;
        }

        printf("[ListenHandler] new connection fd=%d\n", client_fd);

        auto sock_handler = std::make_shared<SockHandler>(client_fd);
        sock_handler->set_nonblocking();
        Reactor::get_instance().register_handler(sock_handler, EVENT_READABLE);
    }
}

void ListenHandler::handle_write()
{
}

void ListenHandler::handle_error()
{
    fprintf(stderr, "[ListenHandler] error on fd=%d\n", listen_fd_);
}

void ListenHandler::handle_close()
{
    if (listen_fd_ != INVALID_HANDLE) {
        printf("[ListenHandler] closing fd=%d\n", listen_fd_);
        close(listen_fd_);
        listen_fd_ = INVALID_HANDLE;
    }
}
