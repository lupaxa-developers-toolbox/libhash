#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <time.h>

#define HASH_VERSION_STRING "0.1.0"

/* HASH_OK is success. HASH_NOMEM means allocation failed and the map was
   left unchanged, except hash_create which sets *table to NULL.
   HASH_INVAL means a missing map, a missing key or out-pointer, a type
   mismatch, a missing entry, or a nested map that cannot be stored. */
#define HASH_OK 0
#define HASH_NOMEM (-1)
#define HASH_INVAL (-2)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash hash;

/* Allocate an empty map. On allocation failure *table is NULL. */
int hash_create(hash **table);

/* Free the map, every copied string, and every nested map it owns.
   *table is set to NULL. Destroying an already NULL map is success.
   A map stored inside another map returns HASH_INVAL and stays stored. */
int hash_destroy(hash **table);

/* Remove one entry. A missing key is success and changes nothing.
   A string is freed. A nested map is destroyed. A pointer is not freed. */
int hash_delete(hash *table, const char *key);

/* Copy value under key. A later put with the same key, ignoring ASCII
   letter case, replaces the value and keeps the first spelling of the key.
   An empty string is a value. A NULL value is HASH_INVAL. On failure the
   previous value stays. */
int hash_put_string(hash *table, const char *key, const char *value);

/* Write the stored string to *value. The pointer is owned by the map and
   stays valid until that key is replaced, deleted, or the map is destroyed.
   A missing key or a different type leaves *value unchanged. */
int hash_get_string(const hash *table, const char *key, const char **value);

/* Store an int as an int. Zero is a stored value. */
int hash_put_int(hash *table, const char *key, int value);

/* Write the stored int to *value. A missing key or a different type leaves
   *value unchanged. */
int hash_get_int(const hash *table, const char *key, int *value);

/* Store a time as a time. Zero is a stored value. */
int hash_put_time(hash *table, const char *key, time_t value);

/* Write the stored time to *value. A missing key or a different type leaves
   *value unchanged. */
int hash_get_time(const hash *table, const char *key, time_t *value);

/* Store a pointer. The map does not copy it and does not free it. NULL is
   a stored value. */
int hash_put_ptr(hash *table, const char *key, void *value);

/* Write the stored pointer to *value. A missing key or a different type
   leaves *value unchanged. A stored NULL is success and writes NULL. */
int hash_get_ptr(const hash *table, const char *key, void **value);

/* Store child and take ownership of it. The parent destroys child when the
   key is replaced, deleted, or the parent is destroyed. A NULL child, the
   map itself, an ancestor, or a map already stored elsewhere is HASH_INVAL.
   Storing the same child under the same key again is success. */
int hash_put_hash(hash *table, const char *key, hash *child);

/* Write the stored child to *child. The parent still owns it. A missing key
   or a different type leaves *child unchanged. */
int hash_get_hash(const hash *table, const char *key, hash **child);

/* Call fn with each key. fn returns HASH_OK to continue. Any other return
   stops the walk and is returned. fn must not modify the map. An empty map
   is success and calls nothing. */
int hash_each(const hash *table, int (*fn)(const char *key, void *extra),
              void *extra);

/* Return the number of keys. A NULL map returns 0. */
size_t hash_count(const hash *table);

#ifdef __cplusplus
}
#endif

#endif /* HASH_H */
