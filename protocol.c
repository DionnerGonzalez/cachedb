#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "protocol.h"

Command *cmd_parse(const char *line)
{
    if (!line || !*line)
        return NULL;

    /* make a mutable copy, strip trailing \r\n */
    char *buf = strdup(line);
    if (!buf)
        return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
        buf[--len] = '\0';

    /* tokenise by spaces */
    char *tokens[64];
    int   count = 0;
    char *p = buf;

    while (*p && count < 64) {
        while (*p == ' ') p++;
        if (!*p) break;

        char *start = p;
        if (*p == '"') {
            /* quoted token */
            start = ++p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            while (*p && *p != ' ') p++;
            if (*p == ' ') *p++ = '\0';
        }
        tokens[count++] = start;
    }

    if (count == 0) {
        free(buf);
        return NULL;
    }

    Command *cmd = malloc(sizeof(Command));
    if (!cmd) {
        free(buf);
        return NULL;
    }

    cmd->argc = count;
    cmd->argv = malloc(count * sizeof(char *));
    if (!cmd->argv) {
        free(cmd);
        free(buf);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        cmd->argv[i] = strdup(tokens[i]);
        /* uppercase the command name */
        if (i == 0) {
            for (char *c = cmd->argv[i]; *c; c++)
                *c = (char)toupper((unsigned char)*c);
        }
    }

    free(buf);
    return cmd;
}

void cmd_free(Command *cmd)
{
    if (!cmd)
        return;
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    free(cmd);
}

char *resp_ok(void)
{
    return strdup("+OK\r\n");
}

char *resp_error(const char *msg)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "-ERR %s\r\n", msg);
    return strdup(buf);
}

char *resp_integer(long n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), ":%ld\r\n", n);
    return strdup(buf);
}

char *resp_bulk(const char *s)
{
    if (!s)
        return strdup("$-1\r\n");

    size_t len = strlen(s);
    /* "$N\r\ndata\r\n" */
    size_t total = 64 + len + 4;
    char *buf = malloc(total);
    if (!buf)
        return NULL;
    snprintf(buf, total, "$%zu\r\n%s\r\n", len, s);
    return buf;
}

char *resp_array(char **items, size_t count)
{
    /* first pass: compute total size */
    size_t total = 32;
    for (size_t i = 0; i < count; i++)
        total += 32 + strlen(items[i]) + 4;

    char *out = malloc(total);
    if (!out)
        return NULL;

    int written = snprintf(out, total, "*%zu\r\n", count);
    for (size_t i = 0; i < count; i++) {
        char *part = resp_bulk(items[i]);
        if (!part) {
            free(out);
            return NULL;
        }
        size_t plen = strlen(part);
        memcpy(out + written, part, plen);
        written += (int)plen;
        free(part);
    }
    out[written] = '\0';
    return out;
}
