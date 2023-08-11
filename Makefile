.PHONY: all clean

CC = clang
CFAGS = -O3 -Wall -Wextra
CFAGS_DEBUG = -O0 -Wall -Wextra -ggdb -g3 -DDEBUG

SOURCES = $(wildcard src/*.c src/**/*.c)

RELEASE_OBJECTS = $(patsubst src/%.c,build/release/%.o,$(SOURCES))
DEBUG_OBJECTS = $(patsubst src/%.c,build/debug/%.o,$(SOURCES))

RELEASE_BIN = build/release/myshell
DEBUG_BIN = build/debug/myshell

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


clean:
	rm -rf build

