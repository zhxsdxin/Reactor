#include "epoll_demultiplexer.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <cerrno>
#include <cstdio>

static uint32_t event_to_epoll(uint32_t evt)
{
    uint32_t ep = 0;
    if (evt & EVENT_READABLE)  ep |= EPOLLIN;
    if (evt & EVENT_WRITABLE)  ep |= EPOLLOUT;
    if (evt & EVENT_ET)        ep |= EPOLLET;
    return ep;
}

static uint32_t epoll_to_event(uint32_t ep)
{
    uint32_t evt = EVENT_NONE;
    if (ep & EPOLLIN)    evt |= EVENT_READABLE;
    if (ep & EPOLLOUT)   evt |= EVENT_WRITABLE;
    if (ep & EPOLLERR)   evt |= EVENT_ERROR;
    if (ep & EPOLLHUP)   evt |= EVENT_HANGUP;
    if (ep & EPOLLRDHUP) evt |= EVENT_RDHUP;
    return evt;
}

EpollDemultiplexer::EpollDemultiplexer()
    : evs_(MAX_EVENTS)
{
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        perror("epoll_create1 failed");
    }
}

EpollDemultiplexer::~EpollDemultiplexer()
{
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

int EpollDemultiplexer::wait_event(std::vector<ReadyEvent>& ready_events, int timeout_ms)
{
    ready_events.clear();
    if (epoll_fd_ < 0) return -1;

    int nfds = epoll_wait(epoll_fd_, evs_.data(), MAX_EVENTS, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) return 0;
        perror("epoll_wait failed");
        return -1;
    }
    if (nfds == 0) return 0;

    ready_events.reserve(nfds);
    for (int i = 0; i < nfds; ++i) {
        ready_events.push_back({evs_[i].data.fd, epoll_to_event(evs_[i].events)});
    }
    return nfds;
}

bool EpollDemultiplexer::regist(Handle handle, uint32_t evt)
{
    if (epoll_fd_ < 0) return false;
    epoll_event ev;
    ev.data.fd = handle;
    ev.events = event_to_epoll(evt);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, handle, &ev) < 0) {
        perror("epoll_ctl ADD failed");
        return false;
    }
    return true;
}

bool EpollDemultiplexer::remove(Handle handle)
{
    if (epoll_fd_ < 0) return false;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, handle, nullptr) < 0) {
        perror("epoll_ctl DEL failed");
        return false;
    }
    return true;
}

bool EpollDemultiplexer::modify(Handle handle, uint32_t evt)
{
    if (epoll_fd_ < 0) return false;
    epoll_event ev;
    ev.data.fd = handle;
    ev.events = event_to_epoll(evt);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, handle, &ev) < 0) {
        perror("epoll_ctl MOD failed");
        return false;
    }
    return true;
}
