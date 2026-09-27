CC ?= cc
CFLAGS ?= -std=gnu11 -O2 -Wall -Wextra -Werror

.PHONY: all test clean

all: test_lexer

test_lexer: arino_lexer.c arino_lexer.h arino_opcodes.h fnv1a_x86_64.S test_lexer.c
	$(CC) $(CFLAGS) arino_lexer.c fnv1a_x86_64.S test_lexer.c -lm -o $@

test: test_lexer
	./test_lexer

clean:
	rm -f test_lexer
