CC      ?= cc
CFLAGS  ?= -O2 -g
PREFIX  ?= /usr/local

# Always-on flags live separately so `make CFLAGS=...` can't drop them.
BASE_CFLAGS := -std=c11 -D_GNU_SOURCE -Iinclude \
               -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes

BIN     := ubhealth
LIB_SRC := $(filter-out src/main.c,$(wildcard src/*.c src/checks/*.c))
LIB_OBJ := $(patsubst src/%.c,build/%.o,$(LIB_SRC))

all: $(BIN)

$(BIN): build/main.o $(LIB_OBJ)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c include/ubhealth.h
	@mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -c -o $@ $<

build/test_runner: tests/test_main.c $(LIB_OBJ)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test: build/test_runner
	./build/test_runner

# Rebuild everything with AddressSanitizer + UBSan and run the tests and the binary.
asan: clean
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	        LDFLAGS="-fsanitize=address,undefined" all test
	./$(BIN) --verbose > /dev/null; test $$? -le 2

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -rf build $(BIN)

.PHONY: all test asan install clean
