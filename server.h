#ifndef SERVER_H
#define SERVER_H

#include "store.h"
#include "persistence.h"

#define SERVER_MAX_CLIENTS  128
#define SERVER_BUF_SIZE     4096
#define SERVER_EVICT_SECS   5    /* run expiry sweep every N seconds */

typedef struct client {
    int   fd;
    char  buf[SERVER_BUF_SIZE];
    int   buf_len;
    int   active;
} Client;

typedef struct server {
    int      listen_fd;
    int      port;
    Store   *store;
    AOF     *aof;
    Client   clients[SERVER_MAX_CLIENTS];
} Server;

/* Start listening and enter the event loop. Blocks until interrupted. */
int server_run(int port, const char *aof_path);

#endif /* SERVER_H */
