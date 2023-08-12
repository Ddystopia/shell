.PHONY: all clean gentests test_objects

CC = clang
CFAGS = -O3 -Wall -Wextra
CFAGS_DEBUG = -O0 -Wall -Wextra -ggdb -g3 -DDEBUG

SOURCES = $(filter-out src/test.c,$(wildcard src/*.c src/**/*.c))

RELEASE_OBJECTS = $(patsubst src/%.c,build/release/%.o,$(SOURCES))
DEBUG_OBJECTS = $(patsubst src/%.c,build/debug/%.o,$(SOURCES))
TEST_OBJECTS = build/test/test.o $(patsubst src/%.c,build/test/%.o,$(filter-out src/main.c,$(SOURCES)))

RELEASE_BIN = build/release/myshell
DEBUG_BIN = build/debug/myshell
TEST_BIN = build/test_myshell

#
# Syntax:
# [target]: [dependent....]
#   [command]
#
# $@ - name of target
# $< - name of first dependensy
# $^ - all dependensies
#


all: debug

release: $(RELEASE_BIN)

debug: $(DEBUG_BIN)

test: $(TEST_BIN)


build/release:
	mkdir -p build/release

build/release/%.o: src/%.c | build/release
	$(CC) $(CFAGS) -c $< -o $@

$(RELEASE_BIN): $(RELEASE_OBJECTS)
	$(CC) $(CFAGS) $^ -o $@


build/debug:
	mkdir -p build/debug

build/debug/%.o: src/%.c | build/debug
	$(CC) $(CFAGS_DEBUG) -c $< -o $@

$(DEBUG_BIN): $(DEBUG_OBJECTS)
	$(CC) $(CFAGS_DEBUG) $^ -o $@


build/test:
	mkdir -p build/test

gentests: $(SOURCES)
	./gentests

build/test/%.o: src/%.c | build/test
	$(CC) -DTEST $(CFAGS_DEBUG) -c $< -o $@


.NOTPARALLEL: $(TEST_BIN)
$(TEST_BIN): gentests test_objects

test_objects: $(TEST_OBJECTS)
	$(CC) -DTEST $(CFAGS_DEBUG) $(TEST_OBJECTS) -o $(TEST_BIN)


clean:
	rm -rf build

