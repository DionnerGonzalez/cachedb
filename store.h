#ifndef STORE_H
#define STORE_H

#include <stdint.h>
#include <time.h>

#define STORE_INITIAL_CAPACITY  256
#define STORE_LOAD_FACTOR       0.72

typedef struct entry {
    char    *key;
    char    *value;
    time_t   expires;   /* 0 = no expiry */
    int      alive;
} Entry;

typedef struct store {
    Entry   *buckets;
    size_t   capacity;
    size_t   count;
} Store;

Store  *store_create(void);
void    store_destroy(Store *s);

int     store_set(Store *s, const char *key, const char *value, int ttl_seconds);
char   *store_get(Store *s, const char *key);
int     store_del(Store *s, const char *key);
int     store_exists(Store *s, const char *key);
void    store_flush(Store *s);

/* Returns a NULL-terminated array of key strings. Caller must free each key
 * and the array itself. */
char  **store_keys(Store *s, size_t *out_count);

/* Remove expired keys. Called periodically from the event loop. */
void    store_evict_expired(Store *s);

#endif /* STORE_H */
