/*
 * run_client.c
 *
 * run vhost client (socket client, vhost master)
 */

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "vhost_client.h"

static struct sigaction sigact;
int app_running = 0;

static void signal_handler(int);
static void init_signals(void);
static void cleanup(void);

int main(int argc, char* argv[])
{
    VhostClient *vhost_master = NULL;

    atexit(cleanup);
    init_signals();

    char *path = argc == 2 ? argv[1] : NULL;

    /* vhost-user client, can be qemu */
    vhost_master = new_vhost_client(path);
    run_vhost_client(vhost_master);
    free(vhost_master);

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
