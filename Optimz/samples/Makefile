CC ?= clang
CFLAGS ?= -O2 -Wall -Wextra -pipe
LDFLAGS ?=

.PHONY: all clean

all: hello

hello: hello.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f hello

