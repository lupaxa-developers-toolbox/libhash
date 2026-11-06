<p align="center">
  <a href="https://github.com/lupaxa-developers-toolbox">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/organisations/developers-toolbox/readme-logo.png" alt="Developers Toolbox" />
  </a>
</p>

<h1 align="center">Libhash</h1>

C library for a case-insensitive string map.

Each map stores values under string keys. ASCII letters in a key compare
without regard to case, so `Foo` and `foo` are one entry. The type is
opaque. The public header is `include/hash.h`. It uses C linkage, so a C++
translation unit can include it.

## Requirements

- GNU make.
- gcc, unless `CC` is set. The C skill uses gcc for syntax checks.
- cppcheck, for `make c-check`.

## Build

`make lib` writes the static archive `build/libhash.a`. The archive is
built as C11 with `-Wall -Wextra -Wpedantic -Werror`. The compiler is gcc
unless `CC` is set. `make clean` removes `build/`.

```bash
make lib
cc -std=c11 -Iinclude -o demo demo.c build/libhash.a
```

`make lib CC=clang` selects the compiler. `make lib CFLAGS=-g` adds flags to
that C11 build. `make` with no target prints the makefile help.

`make update` refreshes the makefile skills. `make lib` and `make test` do
not need that checkout. `make c-check` and `make version` do.

## Quick Start

Create a map, store a string, and destroy the map. Destroy frees the copy.

```c
#include "hash.h"

int main(void)
{
    hash *table = NULL;
    const char *text = NULL;

    if (hash_create(&table) != HASH_OK) {
        return 1;
    }

    if (hash_put_string(table, "name", "Ada") != HASH_OK) {
        hash_destroy(&table);
        return 1;
    }

    if (hash_get_string(table, "Name", &text) != HASH_OK) {
        hash_destroy(&table);
        return 1;
    }

    hash_destroy(&table);
    return 0;
}
```

## Keys

A key is a string, including an empty string. A NULL key is
`HASH_INVAL`. Comparison folds `A`–`Z` to `a`–`z` and leaves every other
byte unchanged. The spelling stored with the first successful put is the
one `hash_each` reports. A later put of the same key updates the value and
keeps that spelling.

## Values

A string is copied. `hash_get_string` returns the stored copy. It stays
valid until that key is replaced, deleted, or the map is destroyed. The
caller does not free it. An empty string is a value. A NULL string is
`HASH_INVAL`.

An `int` and a `time_t` are stored as themselves. Zero is a stored value.
A pointer is stored as given. The map does not copy it and does not free
it. A NULL pointer is a stored value, and a missing key is `HASH_INVAL`.

A get checks the type. The wrong type leaves the out-pointer unchanged.
A missing key does the same.

## Ownership

The map owns its entries, its copied strings, and any nested map stored
with `hash_put_hash`. Replacing or deleting that key destroys the child.
Destroying the parent destroys every child still stored.

`hash_destroy` on a map that is currently stored in a parent returns
`HASH_INVAL` and leaves it stored. A map cannot contain itself, an
ancestor, or a map that another parent already owns.

`hash_delete` of a missing key is success. It does not free a stored
pointer. The caller still owns that pointer.

> **Note:** Callers serialise use of a given map. `hash_each` must not
> modify the map it is walking.

## Return Codes

Every function that can fail returns an `int`.

| Code          | Value | Meaning                                                                                          |
| ------------- | ----- | ------------------------------------------------------------------------------------------------ |
| `HASH_OK`     | `0`   | The call succeeded.                                                                              |
| `HASH_NOMEM`  | `-1`  | Allocation failed. The map is unchanged. Create sets `*table` to NULL.                           |
| `HASH_INVAL`  | `-2`  | The map, key, or out-pointer is missing, the type does not match, or the nested map is refused.  |

`hash_destroy(NULL)` returns `HASH_INVAL`. `hash_destroy` on a pointer that
is already NULL returns `HASH_OK` and leaves it NULL.

## Functions

```c
typedef struct hash hash;

int hash_create(hash **table);
int hash_destroy(hash **table);
int hash_delete(hash *table, const char *key);
int hash_put_string(hash *table, const char *key, const char *value);
int hash_get_string(const hash *table, const char *key, const char **value);
int hash_put_int(hash *table, const char *key, int value);
int hash_get_int(const hash *table, const char *key, int *value);
int hash_put_time(hash *table, const char *key, time_t value);
int hash_get_time(const hash *table, const char *key, time_t *value);
int hash_put_ptr(hash *table, const char *key, void *value);
int hash_get_ptr(const hash *table, const char *key, void **value);
int hash_put_hash(hash *table, const char *key, hash *child);
int hash_get_hash(const hash *table, const char *key, hash **child);
int hash_each(const hash *table, int (*fn)(const char *key, void *extra),
              void *extra);
size_t hash_count(const hash *table);
```

| Name              | Purpose                                              |
| ----------------- | ---------------------------------------------------- |
| `hash_create`     | Allocate an empty map.                               |
| `hash_destroy`    | Free the map, its strings, and its nested maps.      |
| `hash_delete`     | Remove one key. A missing key is success.            |
| `hash_put_string` | Copy a string under a key.                           |
| `hash_get_string` | Read the stored string.                              |
| `hash_put_int`    | Store an int.                                        |
| `hash_get_int`    | Read the stored int.                                 |
| `hash_put_time`   | Store a time.                                        |
| `hash_get_time`   | Read the stored time.                                |
| `hash_put_ptr`    | Store a pointer without taking ownership.            |
| `hash_get_ptr`    | Read the stored pointer.                             |
| `hash_put_hash`   | Store a nested map and take ownership of it.         |
| `hash_get_hash`   | Read the nested map. The parent still owns it.       |
| `hash_each`       | Visit each key.                                      |
| `hash_count`      | Return how many keys are stored.                     |

`hash_count(NULL)` returns 0. `hash_each` on an empty map calls nothing.
The callback returns `HASH_OK` to continue. Any other return stops the walk
and becomes the result of `hash_each`.

## Empty Map

A new map has a count of 0. Destroying it returns `HASH_OK` and sets the
caller's pointer to NULL. Destroying that NULL pointer again returns
`HASH_OK`.

## Version

The version text is the macro `HASH_VERSION_STRING` in `include/hash.h`.
It is a dotted semantic version. `.bumpversion.toml` is the version source,
and a bump updates that macro.

After `make init`, `make version` prints the version from
`.bumpversion.toml`.

## Development

```bash
make update
make lib
make check
make clean
```

`make c-check` syntax-checks every `.c` and `.h` file, then runs cppcheck.
`make check` runs `c-check` and then the behaviour tests. `make test` runs
only the behaviour tests.

<a href="https://github.com/the-lupaxa-project">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/components/footer-for-child-orgs.svg" alt="The Lupaxa Project Footer" width="100%" />
</a>
