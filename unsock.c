#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "common.h"
#include "unsock.h"

/* alloc a new socket struct, initialize state to INSTANCE_CREATED */
UnSock* new_unsock(const char* path)
{
    UnSock* s = (UnSock*) calloc(1, sizeof(UnSock));
    strncpy(s->sock_path, path ? path : VHOST_SOCK_NAME, PATH_MAX);
    return s;
}

/* close socket */
int close_unsock(UnSock* s)
{
    if (s->is_connected) {
        // Close and unlink the socket
        close(s->sock);
        s->is_connected = 0;
        if(s->is_server) {
            unlink(s->sock_path);
        }
    }

    return 0;
}
