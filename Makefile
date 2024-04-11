.PHONY: all clean gentests test_objects

target ?= debug

CC = gcc

cflags_release = -O3 -Wall -Wextra
cflags_debug = -O0 -Wall -Wextra -ggdb -g3 -DDEBUG -fsanitize=address -fno-omit-frame-pointer
cflags_test = $(cflags_debug) -DTEST

cflags  = ${cflags_${target}} ${CFLAGS}
ldflags = ${ldflags_${target}} ${LDFLAGS}

sources = $(filter-out src/test.c,$(wildcard src/*.c))

ifeq "test" "${target}"
objects = build/test/test.o $(patsubst src/%.c,build/test/%.o,$(filter-out src/main.c,$(sources)))
else
objects = $(patsubst src/%.c,build/${target}/%.o,$(sources))
endif

bin := shell

#
# Syntax:
# [target]: [dependent....]
#   [command]
#
# $@ - name of target
# $< - name of first dependensy
# $^ - all dependensies
#

.NOTPARALLEL: all build/${target}/${bin}

all: info build/${target}/${bin}

info:
	@echo "Building under target ${target}"
	@echo -e "CC = \t\t${CC}"
	@echo -e "CFLAGS  = \t${cflags}"
	@echo -e "LDFLAGS = \t${ldflags}"
	@echo "--------------------------------"


build/${target}:
	@mkdir -p "build/${target}"

build/${target}/%.o: src/%.c | build/${target}
	@printf "\e[1;32m%10s\e[0m %s\n" "compiling" "$<"
	@$(CC) $(cflags) -c $< -o $@

objects: $(objects)
	@printf "\e[1;32m%10s\e[0m %s\n" "linking" "$(bin) (build/${target}/${bin})"
	@$(CC) $(cflags) $(objects) -o build/$(target)/$(bin)

ifeq "test" "${target}"

build/${target}/${bin}: gentests objects

gentests: $(sources)
	./gentests

else

build/${target}/${bin}: objects

endif


clean:
	rm -rf build

