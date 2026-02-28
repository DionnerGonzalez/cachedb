#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persistence.h"
#include "store.h"

static void replay(FILE *fp, Store *s)
{
    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        /* strip newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* format:  SET key value [ttl]
                    DEL key
                    FLUSH */
        char *cmd = strtok(line, " ");
        if (!cmd)
            continue;

        if (strcmp(cmd, "SET") == 0) {
            char *key = strtok(NULL, " ");
            char *val = strtok(NULL, " ");
            char *ttl = strtok(NULL, " ");
            if (!key || !val)
                continue;
            store_set(s, key, val, ttl ? atoi(ttl) : 0);
        } else if (strcmp(cmd, "DEL") == 0) {
            char *key = strtok(NULL, " ");
            if (key)
                store_del(s, key);
        } else if (strcmp(cmd, "FLUSH") == 0) {
            store_flush(s);
        }
    }
}

AOF *aof_open(const char *path, Store *s)
{
    AOF *aof = malloc(sizeof(AOF));
    if (!aof)
        return NULL;

    aof->path = strdup(path);
    if (!aof->path) {
        free(aof);
        return NULL;
    }

    /* open for reading to replay, then switch to append */
    FILE *rfp = fopen(path, "r");
    if (rfp) {
        replay(rfp, s);
        fclose(rfp);
    }

    aof->fp = fopen(path, "a");
    if (!aof->fp) {
        free(aof->path);
        free(aof);
        return NULL;
    }

    return aof;
}

int aof_write_set(AOF *aof, const char *key, const char *value, int ttl)
{
    if (!aof || !aof->fp)
        return 0;

    int ret;
    if (ttl > 0)
        ret = fprintf(aof->fp, "SET %s %s %d\n", key, value, ttl);
    else
        ret = fprintf(aof->fp, "SET %s %s\n", key, value);

    fflush(aof->fp);
    return ret > 0 ? 0 : -1;
}

int aof_write_del(AOF *aof, const char *key)
{
    if (!aof || !aof->fp)
        return 0;
    int ret = fprintf(aof->fp, "DEL %s\n", key);
    fflush(aof->fp);
    return ret > 0 ? 0 : -1;
}

int aof_write_flush(AOF *aof)
{
    if (!aof || !aof->fp)
        return 0;
    int ret = fprintf(aof->fp, "FLUSH\n");
    fflush(aof->fp);
    return ret > 0 ? 0 : -1;
}

void aof_close(AOF *aof)
{
    if (!aof)
        return;
    if (aof->fp)
        fclose(aof->fp);
    free(aof->path);
    free(aof);
}
