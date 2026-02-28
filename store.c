#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "store.h"

/* FNV-1a 32-bit hash */
static uint32_t fnv1a(const char *key)
{
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

static int entry_alive(const Entry *e)
{
    if (!e->alive)
        return 0;
    if (e->expires > 0 && time(NULL) >= e->expires)
        return 0;
    return 1;
}

Store *store_create(void)
{
    Store *s = malloc(sizeof(Store));
    if (!s)
        return NULL;

    s->capacity = STORE_INITIAL_CAPACITY;
    s->count    = 0;
    s->buckets  = calloc(s->capacity, sizeof(Entry));
    if (!s->buckets) {
        free(s);
        return NULL;
    }
    return s;
}

void store_destroy(Store *s)
{
    if (!s)
        return;
    for (size_t i = 0; i < s->capacity; i++) {
        if (s->buckets[i].key)
            free(s->buckets[i].key);
        if (s->buckets[i].value)
            free(s->buckets[i].value);
    }
    free(s->buckets);
    free(s);
}

static size_t probe(const Store *s, const char *key, int find_empty)
{
    size_t idx = fnv1a(key) % s->capacity;
    size_t start = idx;

    do {
        Entry *e = &s->buckets[idx];
        if (!e->key) {
            /* empty slot */
            if (find_empty)
                return idx;
            return s->capacity; /* not found */
        }
        if (strcmp(e->key, key) == 0)
            return idx;
        idx = (idx + 1) % s->capacity;
    } while (idx != start);

    return s->capacity; /* table full */
}

static int store_resize(Store *s)
{
    size_t new_cap = s->capacity * 2;
    Entry *new_buckets = calloc(new_cap, sizeof(Entry));
    if (!new_buckets)
        return -1;

    for (size_t i = 0; i < s->capacity; i++) {
        Entry *e = &s->buckets[i];
        if (!e->key || !entry_alive(e)) {
            if (e->key)   free(e->key);
            if (e->value) free(e->value);
            continue;
        }
        size_t idx = fnv1a(e->key) % new_cap;
        while (new_buckets[idx].key)
            idx = (idx + 1) % new_cap;
        new_buckets[idx] = *e;
    }

    free(s->buckets);
    s->buckets  = new_buckets;
    s->capacity = new_cap;
    return 0;
}

int store_set(Store *s, const char *key, const char *value, int ttl_seconds)
{
    if ((double)s->count / s->capacity >= STORE_LOAD_FACTOR) {
        if (store_resize(s) < 0)
            return -1;
    }

    size_t idx = probe(s, key, 0);
    if (idx < s->capacity) {
        /* update existing key */
        char *new_val = strdup(value);
        if (!new_val)
            return -1;
        free(s->buckets[idx].value);
        s->buckets[idx].value   = new_val;
        s->buckets[idx].expires = ttl_seconds > 0 ? time(NULL) + ttl_seconds : 0;
        s->buckets[idx].alive   = 1;
        return 0;
    }

    /* insert new */
    idx = probe(s, key, 1);
    if (idx >= s->capacity)
        return -1;

    s->buckets[idx].key   = strdup(key);
    s->buckets[idx].value = strdup(value);
    if (!s->buckets[idx].key || !s->buckets[idx].value) {
        free(s->buckets[idx].key);
        free(s->buckets[idx].value);
        s->buckets[idx].key = s->buckets[idx].value = NULL;
        return -1;
    }
    s->buckets[idx].expires = ttl_seconds > 0 ? time(NULL) + ttl_seconds : 0;
    s->buckets[idx].alive   = 1;
    s->count++;
    return 0;
}

char *store_get(Store *s, const char *key)
{
    size_t idx = probe(s, key, 0);
    if (idx >= s->capacity)
        return NULL;
    if (!entry_alive(&s->buckets[idx]))
        return NULL;
    return s->buckets[idx].value;
}

int store_del(Store *s, const char *key)
{
    size_t idx = probe(s, key, 0);
    if (idx >= s->capacity)
        return 0;
    if (!entry_alive(&s->buckets[idx]))
        return 0;

    free(s->buckets[idx].key);
    free(s->buckets[idx].value);
    memset(&s->buckets[idx], 0, sizeof(Entry));
    s->count--;
    return 1;
}

int store_exists(Store *s, const char *key)
{
    size_t idx = probe(s, key, 0);
    if (idx >= s->capacity)
        return 0;
    return entry_alive(&s->buckets[idx]);
}

void store_flush(Store *s)
{
    for (size_t i = 0; i < s->capacity; i++) {
        if (s->buckets[i].key) {
            free(s->buckets[i].key);
            free(s->buckets[i].value);
            memset(&s->buckets[i], 0, sizeof(Entry));
        }
    }
    s->count = 0;
}

char **store_keys(Store *s, size_t *out_count)
{
    *out_count = 0;
    char **result = malloc((s->count + 1) * sizeof(char *));
    if (!result)
        return NULL;

    size_t n = 0;
    for (size_t i = 0; i < s->capacity; i++) {
        if (entry_alive(&s->buckets[i]))
            result[n++] = strdup(s->buckets[i].key);
    }
    result[n] = NULL;
    *out_count = n;
    return result;
}

void store_evict_expired(Store *s)
{
    time_t now = time(NULL);
    for (size_t i = 0; i < s->capacity; i++) {
        Entry *e = &s->buckets[i];
        if (e->key && e->alive && e->expires > 0 && now >= e->expires) {
            free(e->key);
            free(e->value);
            memset(e, 0, sizeof(Entry));
            s->count--;
        }
    }
}
