#pragma once

#include "event_handler.h"
#include <string>

class ListenHandler : public EventHandler, public std::enable_shared_from_this<ListenHandler> {
public:
    ListenHandler(const std::string& ip, int port);
    ~ListenHandler() override;

    Handle get_handle() const override { return listen_fd_; }
    void handle_read() override;
    void handle_write() override;
    void handle_error() override;
    void handle_close() override;

    bool start();

private:
    bool create_nonblocking_socket();

    std::string ip_;
    int port_;
    Handle listen_fd_ = INVALID_HANDLE;
};
