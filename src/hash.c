/* Case-insensitive string map.
   The public contract is in include/hash.h. Keys compare with ASCII letter
   case folded. Strings are copied. Integers and times are stored as
   themselves. Pointers are not freed. A nested map is owned by its parent. */

/* Quoted path so a syntax check without -I still finds the public header. */
#include "../include/hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum value_type {
    TYPE_STRING = 1,
    TYPE_INT,
    TYPE_TIME,
    TYPE_PTR,
    TYPE_HASH
};

union stored {
    char *string;
    int integer;
    time_t time_value;
    void *pointer;
    hash *child;
};

/* One key. key keeps the spelling of the first successful put. */
struct entry {
    struct entry *next;
    char *key;
    enum value_type type;
    union stored value;
};

/* owner is the parent map while this map is stored as a value. */
struct hash {
    struct entry **buckets;
    size_t bucket_count;
    size_t count;
    struct hash *owner;
};

#define INITIAL_BUCKETS 8u

static unsigned char fold_ascii(unsigned char text)
{
    if (text >= 'A' && text <= 'Z') {
        return (unsigned char)(text - 'A' + 'a');
    }
    return text;
}

static int same_key(const char *stored, const char *key)
{
    const unsigned char *left = (const unsigned char *)stored;
    const unsigned char *right = (const unsigned char *)key;

    while (1) {
        unsigned char a = fold_ascii(*left);
        unsigned char b = fold_ascii(*right);

        if (a != b) {
            return 0;
        }
        if (a == 0) {
            return 1;
        }
        left++;
        right++;
    }
}

static size_t hash_key(const char *key)
{
    const unsigned char *text = (const unsigned char *)key;
    uint64_t hash = 14695981039346656037ull;

    while (*text != 0) {
        hash ^= fold_ascii(*text);
        hash *= 1099511628211ull;
        text++;
    }
    return (size_t)hash;
}

static char *dup_text(const char *text)
{
    size_t length = strlen(text);
    char *copy;

    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static struct entry *lookup(const hash *table, const char *key)
{
    size_t index;
    struct entry *entry;

    if (table->bucket_count == 0) {
        return NULL;
    }
    index = hash_key(key) & (table->bucket_count - 1);
    for (entry = table->buckets[index]; entry != NULL; entry = entry->next) {
        if (same_key(entry->key, key)) {
            return entry;
        }
    }
    return NULL;
}

static int find_slot(hash *table, const char *key, struct entry ***link)
{
    size_t index = hash_key(key) & (table->bucket_count - 1);
    struct entry **cursor = &table->buckets[index];

    while (*cursor != NULL) {
        if (same_key((*cursor)->key, key)) {
            *link = cursor;
            return 1;
        }
        cursor = &(*cursor)->next;
    }
    *link = cursor;
    return 0;
}

static void destroy_contents(hash *table);

static void release_value(struct entry *entry)
{
    hash *child;

    if (entry->type == TYPE_STRING) {
        free(entry->value.string);
        entry->value.string = NULL;
        return;
    }
    if (entry->type != TYPE_HASH) {
        return;
    }
    child = entry->value.child;
    entry->value.child = NULL;
    if (child == NULL) {
        return;
    }
    child->owner = NULL;
    destroy_contents(child);
    free(child);
}

static void destroy_contents(hash *table)
{
    size_t index;

    if (table->buckets == NULL) {
        table->count = 0;
        table->bucket_count = 0;
        return;
    }
    for (index = 0; index < table->bucket_count; index++) {
        struct entry *entry = table->buckets[index];

        while (entry != NULL) {
            struct entry *next = entry->next;

            release_value(entry);
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    table->buckets = NULL;
    table->bucket_count = 0;
    table->count = 0;
}

static int grow(hash *table)
{
    size_t next_count;
    size_t index;
    struct entry **buckets;

    if (table->bucket_count > SIZE_MAX / 2) {
        return HASH_NOMEM;
    }
    next_count = table->bucket_count * 2;
    buckets = calloc(next_count, sizeof *buckets);
    if (buckets == NULL) {
        return HASH_NOMEM;
    }
    for (index = 0; index < table->bucket_count; index++) {
        struct entry *entry = table->buckets[index];

        while (entry != NULL) {
            struct entry *next = entry->next;
            size_t slot = hash_key(entry->key) & (next_count - 1);

            entry->next = buckets[slot];
            buckets[slot] = entry;
            entry = next;
        }
    }
    free(table->buckets);
    table->buckets = buckets;
    table->bucket_count = next_count;
    return HASH_OK;
}

static void abandon(enum value_type type, union stored value)
{
    if (type == TYPE_STRING) {
        free(value.string);
    }
}

static int store(hash *table, const char *key, enum value_type type,
                 union stored value)
{
    struct entry **link = NULL;
    struct entry *entry;

    if (table->count >= table->bucket_count) {
        (void)grow(table);
    }
    if (find_slot(table, key, &link)) {
        if (type == TYPE_HASH) {
            value.child->owner = table;
        }
        release_value(*link);
        (*link)->type = type;
        (*link)->value = value;
        return HASH_OK;
    }

    entry = calloc(1, sizeof *entry);
    if (entry == NULL) {
        abandon(type, value);
        return HASH_NOMEM;
    }
    entry->key = dup_text(key);
    if (entry->key == NULL) {
        free(entry);
        abandon(type, value);
        return HASH_NOMEM;
    }
    entry->type = type;
    entry->value = value;
    if (type == TYPE_HASH) {
        value.child->owner = table;
    }
    *link = entry;
    table->count++;
    return HASH_OK;
}

static int is_above(const hash *node, const hash *start)
{
    const hash *cursor;

    for (cursor = start; cursor != NULL; cursor = cursor->owner) {
        if (cursor == node) {
            return 1;
        }
    }
    return 0;
}

int hash_create(hash **table)
{
    hash *created;

    if (table == NULL) {
        return HASH_INVAL;
    }
    created = calloc(1, sizeof *created);
    if (created == NULL) {
        *table = NULL;
        return HASH_NOMEM;
    }
    created->buckets = calloc(INITIAL_BUCKETS, sizeof *created->buckets);
    if (created->buckets == NULL) {
        free(created);
        *table = NULL;
        return HASH_NOMEM;
    }
    created->bucket_count = INITIAL_BUCKETS;
    *table = created;
    return HASH_OK;
}

int hash_destroy(hash **table)
{
    if (table == NULL) {
        return HASH_INVAL;
    }
    if (*table == NULL) {
        return HASH_OK;
    }
    if ((*table)->owner != NULL) {
        return HASH_INVAL;
    }
    destroy_contents(*table);
    free(*table);
    *table = NULL;
    return HASH_OK;
}

int hash_delete(hash *table, const char *key)
{
    struct entry **link = NULL;
    struct entry *entry;

    if (table == NULL || key == NULL) {
        return HASH_INVAL;
    }
    if (!find_slot(table, key, &link)) {
        return HASH_OK;
    }
    entry = *link;
    *link = entry->next;
    table->count--;
    release_value(entry);
    free(entry->key);
    free(entry);
    return HASH_OK;
}

int hash_put_string(hash *table, const char *key, const char *value)
{
    union stored stored;
    char *copy;

    if (table == NULL || key == NULL || value == NULL) {
        return HASH_INVAL;
    }
    copy = dup_text(value);
    if (copy == NULL) {
        return HASH_NOMEM;
    }
    stored.string = copy;
    return store(table, key, TYPE_STRING, stored);
}

int hash_get_string(const hash *table, const char *key, const char **value)
{
    struct entry *entry;

    if (table == NULL || key == NULL || value == NULL) {
        return HASH_INVAL;
    }
    entry = lookup(table, key);
    if (entry == NULL || entry->type != TYPE_STRING) {
        return HASH_INVAL;
    }
    *value = entry->value.string;
    return HASH_OK;
}

int hash_put_int(hash *table, const char *key, int value)
{
    union stored stored;

    if (table == NULL || key == NULL) {
        return HASH_INVAL;
    }
    stored.integer = value;
    return store(table, key, TYPE_INT, stored);
}

int hash_get_int(const hash *table, const char *key, int *value)
{
    struct entry *entry;

    if (table == NULL || key == NULL || value == NULL) {
        return HASH_INVAL;
    }
    entry = lookup(table, key);
    if (entry == NULL || entry->type != TYPE_INT) {
        return HASH_INVAL;
    }
    *value = entry->value.integer;
    return HASH_OK;
}

int hash_put_time(hash *table, const char *key, time_t value)
{
    union stored stored;

    if (table == NULL || key == NULL) {
        return HASH_INVAL;
    }
    stored.time_value = value;
    return store(table, key, TYPE_TIME, stored);
}

int hash_get_time(const hash *table, const char *key, time_t *value)
{
    struct entry *entry;

    if (table == NULL || key == NULL || value == NULL) {
        return HASH_INVAL;
    }
    entry = lookup(table, key);
    if (entry == NULL || entry->type != TYPE_TIME) {
        return HASH_INVAL;
    }
    *value = entry->value.time_value;
    return HASH_OK;
}

int hash_put_ptr(hash *table, const char *key, void *value)
{
    union stored stored;

    if (table == NULL || key == NULL) {
        return HASH_INVAL;
    }
    stored.pointer = value;
    return store(table, key, TYPE_PTR, stored);
}

int hash_get_ptr(const hash *table, const char *key, void **value)
{
    struct entry *entry;

    if (table == NULL || key == NULL || value == NULL) {
        return HASH_INVAL;
    }
    entry = lookup(table, key);
    if (entry == NULL || entry->type != TYPE_PTR) {
        return HASH_INVAL;
    }
    *value = entry->value.pointer;
    return HASH_OK;
}

int hash_put_hash(hash *table, const char *key, hash *child)
{
    struct entry *existing;
    union stored stored;

    if (table == NULL || key == NULL || child == NULL) {
        return HASH_INVAL;
    }
    if (is_above(child, table)) {
        return HASH_INVAL;
    }
    existing = lookup(table, key);
    if (existing != NULL && existing->type == TYPE_HASH &&
        existing->value.child == child) {
        return HASH_OK;
    }
    if (child->owner != NULL) {
        return HASH_INVAL;
    }
    stored.child = child;
    return store(table, key, TYPE_HASH, stored);
}

int hash_get_hash(const hash *table, const char *key, hash **child)
{
    struct entry *entry;

    if (table == NULL || key == NULL || child == NULL) {
        return HASH_INVAL;
    }
    entry = lookup(table, key);
    if (entry == NULL || entry->type != TYPE_HASH) {
        return HASH_INVAL;
    }
    *child = entry->value.child;
    return HASH_OK;
}

int hash_each(const hash *table, int (*fn)(const char *key, void *extra),
              void *extra)
{
    size_t index;

    if (table == NULL || fn == NULL) {
        return HASH_INVAL;
    }
    for (index = 0; index < table->bucket_count; index++) {
        struct entry *entry = table->buckets[index];

        while (entry != NULL) {
            struct entry *next = entry->next;
            int result = fn(entry->key, extra);

            if (result != HASH_OK) {
                return result;
            }
            entry = next;
        }
    }
    return HASH_OK;
}

size_t hash_count(const hash *table)
{
    if (table == NULL) {
        return 0;
    }
    return table->count;
}
