#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <atomic>

#include "reactor.h"
#include "listen_handler.h"
#include "timer.h"

static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[main] shutting down...\n");
        Reactor::get_instance().stop();
    }
}

static void add_periodic_timer()
{
    Reactor::get_instance().get_timer_heap().add_timer(5000, []() {
        printf("[timer] server is running...\n");
        add_periodic_timer();
    });
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    int port = 8080;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            return 1;
        }
    }

    printf("=== Reactor Echo Server ===\n");
    printf("Port: %d, Demux: %s\n", port, Reactor::get_instance().get_demux()->name());

    auto listen = std::make_shared<ListenHandler>("0.0.0.0", port);
    if (!listen->start()) {
        fprintf(stderr, "failed to start server\n");
        return 1;
    }

    add_periodic_timer();

    printf("server started, Ctrl+C to stop\n");
    Reactor::get_instance().event_loop(100);
    printf("server stopped\n");

    return 0;
}
