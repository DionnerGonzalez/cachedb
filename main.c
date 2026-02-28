#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --port  <port>   TCP port to listen on (default: 6388)\n"
        "  --persist <file> Enable AOF persistence to <file>\n"
        "  --help           Show this message\n"
        "\n"
        "Example:\n"
        "  %s --port 6388 --persist ./data.aof\n"
        "\n",
        prog, prog);
}

int main(int argc, char *argv[])
{
    int         port     = 6388;
    const char *aof_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "invalid port: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--persist") == 0 && i + 1 < argc) {
            aof_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    return server_run(port, aof_path);
}
