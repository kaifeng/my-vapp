/*
 * run_server.c
 *
 * run vhost server (socket server, vhost slave)
 */

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "vhost_server.h"

static struct sigaction sigact;
int app_running = 0;

static void signal_handler(int);
static void init_signals(void);
static void cleanup(void);

int main(int argc, char* argv[])
{
    VhostServer *vhost_slave = NULL;

    atexit(cleanup);
    init_signals();

    char *path = argc == 2 ? argv[1] : NULL;

    /* vhost-user backend, who creates the unit domain socket */
    vhost_slave = new_vhost_server(path, 1);
    run_vhost_server(vhost_slave);
    end_vhost_server(vhost_slave);
    free(vhost_slave);

    return EXIT_SUCCESS;
}

static void signal_handler(int sig){
    switch(sig)
    {
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
        app_running = 0;
        break;
    default:
        break;
    }
}

static void init_signals(void){
    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, (struct sigaction *)NULL);
}

static void cleanup(void){
    sigemptyset(&sigact.sa_mask);
    app_running = 0;
}
