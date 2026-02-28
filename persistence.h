#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "store.h"

typedef struct aof {
    FILE *fp;
    char *path;
} AOF;

/* Open (or create) the AOF file and replay existing entries into store.
 * Returns NULL on failure. */
AOF *aof_open(const char *path, Store *s);

/* Append a SET command to the AOF file. */
int aof_write_set(AOF *aof, const char *key, const char *value, int ttl);

/* Append a DEL command to the AOF file. */
int aof_write_del(AOF *aof, const char *key);

/* Append a FLUSH command to the AOF file. */
int aof_write_flush(AOF *aof);

void aof_close(AOF *aof);

#endif /* PERSISTENCE_H */
