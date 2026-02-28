#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

/*
 * Parsed command from a client request.
 * argv[0] is the command name, argv[1..argc-1] are arguments.
 */
typedef struct command {
    int    argc;
    char **argv;
} Command;

/* Parse a line-based request into a Command. Returns NULL on error.
 * The returned Command must be freed with cmd_free(). */
Command *cmd_parse(const char *line);
void     cmd_free(Command *cmd);

/* Response building helpers. All return heap-allocated strings; caller frees. */
char *resp_ok(void);
char *resp_error(const char *msg);
char *resp_integer(long n);
char *resp_bulk(const char *s);        /* $N\r\ndata\r\n or $-1 for nil */
char *resp_array(char **items, size_t count);

#endif /* PROTOCOL_H */
