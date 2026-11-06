# Static library build. The c skill syntax-checks and runs cppcheck.
# It does not compile or link.

override CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
override CPPCHECK_FLAGS += -Iinclude --std=c11

HASH_BUILD := build
HASH_LIB := $(HASH_BUILD)/libhash.a
HASH_OBJ := $(HASH_BUILD)/hash.o
HASH_TEST := $(HASH_BUILD)/test_hash

.PHONY: lib test check clean

lib: $(HASH_LIB)

$(HASH_OBJ): src/hash.c include/hash.h
	mkdir -p $(HASH_BUILD)
	$(CC) $(CFLAGS) -c -o $@ src/hash.c

$(HASH_LIB): $(HASH_OBJ)
	$(AR) rcs $@ $(HASH_OBJ)

$(HASH_TEST): tests/test_hash.c include/hash.h $(HASH_LIB)
	$(CC) $(CFLAGS) -o $@ tests/test_hash.c $(HASH_LIB) $(LDFLAGS)

test: $(HASH_TEST)
	./$(HASH_TEST)

check: $(if $(filter c,$(SKILLS)),c-check) test

clean:
	rm -rf $(HASH_BUILD)
