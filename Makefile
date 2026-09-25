# Convenience Makefile delegating to just.
# Build/test/green/check recipes live in the Justfile (just build/test/green).

.PHONY: all build test green check clean

all: build

build:
	just build

test:
	just test

green:
	just green

check:
	just check

clean:
	just clean
