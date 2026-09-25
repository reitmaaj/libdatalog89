# c89-baseline.mk - reusable strict C89 portability conformance check.
#
# Drop into any project root and run:
#     make -f c89-baseline.mk check
# or rename to "Makefile" and run:
#     make check
#
# Every compile uses -Werror, so ANY diagnostic fails the build:
#     -std=c89 -pedantic-errors -Wall -Wextra -Werror
#     -Wconversion -Wsign-conversion -Wstrict-prototypes
#     -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2 -Wno-long-long
# (-Warith-conversion deliberately excluded; -Wconversion already enables
#  -Wsign-conversion under GCC, spelling both is explicit policy.)
#
# Overridable variables (all additive or configuration only, never weaken):
#     CC      C compiler                  (default: cc)
#     SRC     directories to scan         (default: .)
#     EXTRA   extra additive flags        (default: empty, e.g. EXTRA="-Isrc")
#     PRUNE   find -prune patterns        (default: .agent and build dirs)

CC      ?= cc
SRC     ?= .
EXTRA   ?=
PRUNE   ?= \( -path '*/.agent' -o -path '*/build' \)

BASELINE := -std=c89 -pedantic-errors -Wall -Wextra -Werror \
    -Wconversion -Wsign-conversion -Wstrict-prototypes \
    -Wmissing-prototypes -Wold-style-definition \
    -Wundef -Wshadow -Wformat=2 -Wno-long-long $(EXTRA)

CFILES := $(shell find $(SRC) $(PRUNE) -prune -o -name '*.c' -print)
HFILES := $(shell find $(SRC) $(PRUNE) -prune -o -name '*.h' -print)

.PHONY: check check-c check-h selftest

# Default: full enforcement gate.
check: check-c check-h
	@echo "c89-baseline: OK - all sources and headers conform."

check-c:
	@fail=0; \
	for f in $(CFILES); do \
	    printf 'c89-baseline: %s\n' "$$f"; \
	    $(CC) $(BASELINE) -c "$$f" -o /dev/null || fail=1; \
	done; \
	if [ "$$fail" -ne 0 ]; then \
	    echo "c89-baseline: FAIL: a source emitted a diagnostic" >&2; \
	    exit 1; \
	fi

check-h:
	@fail=0; \
	for f in $(HFILES); do \
	    printf 'c89-baseline: header %s\n' "$$f"; \
	    $(CC) $(BASELINE) -x c -include "$$f" -c /dev/null -o /dev/null || fail=1; \
	done; \
	if [ "$$fail" -ne 0 ]; then \
	    echo "c89-baseline: FAIL: a header is not self-contained" >&2; \
	    exit 1; \
	fi

# Negative + positive self-test proving enforcement works.
selftest:
	@tmp=$$(mktemp -d); \
	printf 'int add(int, int);\nint add(int a, int b){return a+b;}\n' > "$$tmp/good.c"; \
	printf 'long x; int y = x;\n' > "$$tmp/bad.c"; \
	if $(CC) $(BASELINE) -c "$$tmp/bad.c" -o /dev/null 2>/dev/null; then \
	    echo "c89-baseline: FAIL: non-conforming snippet accepted" >&2; \
	    rm -rf "$$tmp"; exit 1; \
	fi; \
	if ! $(CC) $(BASELINE) -c "$$tmp/good.c" -o /dev/null 2>/dev/null; then \
	    echo "c89-baseline: FAIL: conforming snippet rejected" >&2; \
	    rm -rf "$$tmp"; exit 1; \
	fi; \
	rm -rf "$$tmp"; \
	echo "c89-baseline: selftest OK"
