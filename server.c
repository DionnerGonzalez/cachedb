#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "server.h"
#include "store.h"
#include <ctype.h>
#include "protocol.h"
#include "persistence.h"

static volatile int running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static char *dispatch(Server *srv, const char *line)
{
    Command *cmd = cmd_parse(line);
    if (!cmd)
        return resp_error("empty command");

    char *response = NULL;

    if (strcmp(cmd->argv[0], "PING") == 0) {
        response = strdup("+PONG\r\n");

    } else if (strcmp(cmd->argv[0], "SET") == 0) {
        if (cmd->argc < 3) {
            response = resp_error("SET requires key and value");
        } else {
            int ttl = 0;
            /* look for EX option */
            for (int i = 3; i < cmd->argc - 1; i++) {
                char upper[8] = {0};
                strncpy(upper, cmd->argv[i], 7);
                for (int j = 0; upper[j]; j++)
                    upper[j] = (char)toupper((unsigned char)upper[j]);
                if (strcmp(upper, "EX") == 0)
                    ttl = atoi(cmd->argv[i+1]);
            }
            if (store_set(srv->store, cmd->argv[1], cmd->argv[2], ttl) == 0) {
                if (srv->aof)
                    aof_write_set(srv->aof, cmd->argv[1], cmd->argv[2], ttl);
                response = resp_ok();
            } else {
                response = resp_error("out of memory");
            }
        }

    } else if (strcmp(cmd->argv[0], "GET") == 0) {
        if (cmd->argc < 2) {
            response = resp_error("GET requires key");
        } else {
            char *val = store_get(srv->store, cmd->argv[1]);
            response = resp_bulk(val);
        }

    } else if (strcmp(cmd->argv[0], "DEL") == 0) {
        if (cmd->argc < 2) {
            response = resp_error("DEL requires key");
        } else {
            int deleted = store_del(srv->store, cmd->argv[1]);
            if (deleted && srv->aof)
                aof_write_del(srv->aof, cmd->argv[1]);
            response = resp_integer(deleted);
        }

    } else if (strcmp(cmd->argv[0], "EXISTS") == 0) {
        if (cmd->argc < 2) {
            response = resp_error("EXISTS requires key");
        } else {
            response = resp_integer(store_exists(srv->store, cmd->argv[1]));
        }

    } else if (strcmp(cmd->argv[0], "KEYS") == 0) {
        size_t count = 0;
        char **keys = store_keys(srv->store, &count);
        if (!keys) {
            response = resp_error("out of memory");
        } else {
            response = resp_array(keys, count);
            for (size_t i = 0; i < count; i++)
                free(keys[i]);
            free(keys);
        }

    } else if (strcmp(cmd->argv[0], "FLUSH") == 0) {
        store_flush(srv->store);
        if (srv->aof)
            aof_write_flush(srv->aof);
        response = resp_ok();

    } else if (strcmp(cmd->argv[0], "DBSIZE") == 0) {
        response = resp_integer((long)srv->store->count);

    } else if (strcmp(cmd->argv[0], "QUIT") == 0) {
        response = resp_ok();

    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "unknown command '%s'", cmd->argv[0]);
        response = resp_error(msg);
    }

    cmd_free(cmd);
    return response;
}

static void client_read(Server *srv, int slot)
{
    Client *c = &srv->clients[slot];
    ssize_t n = recv(c->fd, c->buf + c->buf_len,
                     SERVER_BUF_SIZE - c->buf_len - 1, 0);
    if (n <= 0) {
        close(c->fd);
        c->active = 0;
        return;
    }
    c->buf_len += (int)n;
    c->buf[c->buf_len] = '\0';

    /* process complete lines */
    char *start = c->buf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        char *response = dispatch(srv, start);
        if (response) {
            send(c->fd, response, strlen(response), 0);
            /* close connection on QUIT */
            if (strcmp(response, "+OK\r\n") == 0 &&
                strncmp(start, "QUIT", 4) == 0) {
                free(response);
                close(c->fd);
                c->active = 0;
                return;
            }
            free(response);
        }
        start = nl + 1;
    }

    /* shift remaining partial data */
    int remaining = (int)(c->buf + c->buf_len - start);
    if (remaining > 0 && start != c->buf)
        memmove(c->buf, start, remaining);
    c->buf_len = remaining;
    c->buf[c->buf_len] = '\0';
}

int server_run(int port, const char *aof_path)
{
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    Server srv;
    memset(&srv, 0, sizeof(srv));
    srv.port = port;

    srv.store = store_create();
    if (!srv.store) {
        fprintf(stderr, "failed to create store\n");
        return 1;
    }

    if (aof_path) {
        srv.aof = aof_open(aof_path, srv.store);
        if (!srv.aof)
            fprintf(stderr, "warning: could not open AOF file %s\n", aof_path);
    }

    srv.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv.listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(srv.listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(srv.listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(srv.listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv.listen_fd);
        return 1;
    }

    if (listen(srv.listen_fd, 64) < 0) {
        perror("listen");
        close(srv.listen_fd);
        return 1;
    }

    printf("cachedb listening on port %d\n", port);
    if (aof_path)
        printf("persistence: %s\n", aof_path);

    time_t last_evict = time(NULL);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv.listen_fd, &rfds);
        int maxfd = srv.listen_fd;

        for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
            if (srv.clients[i].active) {
                FD_SET(srv.clients[i].fd, &rfds);
                if (srv.clients[i].fd > maxfd)
                    maxfd = srv.clients[i].fd;
            }
        }

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int nready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (nready < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        /* accept new connections */
        if (FD_ISSET(srv.listen_fd, &rfds)) {
            struct sockaddr_in ca;
            socklen_t cl = sizeof(ca);
            int cfd = accept(srv.listen_fd, (struct sockaddr *)&ca, &cl);
            if (cfd >= 0) {
                set_nonblocking(cfd);
                int placed = 0;
                for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
                    if (!srv.clients[i].active) {
                        srv.clients[i].fd      = cfd;
                        srv.clients[i].buf_len = 0;
                        srv.clients[i].active  = 1;
                        placed = 1;
                        break;
                    }
                }
                if (!placed) {
                    /* server full */
                    const char *msg = "-ERR max clients reached\r\n";
                    send(cfd, msg, strlen(msg), 0);
                    close(cfd);
                }
            }
        }

        /* handle client data */
        for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
            if (srv.clients[i].active &&
                FD_ISSET(srv.clients[i].fd, &rfds)) {
                client_read(&srv, i);
            }
        }

        /* periodic expiry sweep */
        time_t now = time(NULL);
        if (now - last_evict >= SERVER_EVICT_SECS) {
            store_evict_expired(srv.store);
            last_evict = now;
        }
    }

    printf("\nshutting down...\n");

    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (srv.clients[i].active)
            close(srv.clients[i].fd);
    }
    close(srv.listen_fd);
    aof_close(srv.aof);
    store_destroy(srv.store);
    return 0;
}
