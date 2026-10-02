set shell := ["sh", "-eu", "-c"]

CC := env_var_or_default("CC", "cc")
GREEN := env_var_or_default("GREEN", "../green/.agent/tmp/build/green")
DIFF_N := env_var_or_default("DATALOG89_DIFF_N", "10000")
DIFF_SAN_N := env_var_or_default("DATALOG89_DIFF_SAN_N", "1000")

STRICT := "-std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2 -Wno-long-long"
INCS := "-Iinclude -Isrc -Itest -Itest/fixtures -Itest/differential"

# Library translation units; CORE omits the memory seam so the allocation
# fault fixture can be linked in its place.
CORE := "src/datalog89_eval.c src/datalog89_rule.c src/datalog89_join.c src/datalog89_fixpoint.c src/datalog89_status.c src/datalog89_plan.c"
MEM := "src/datalog89_priv_mem.c"
SRC := CORE + " " + MEM

default: build

# Compile every library translation unit and archive build/libdatalog89.a.
build:
	mkdir -p build
	@for f in {{SRC}}; do \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} -Iinclude -Isrc -c "$f" -o "build/$name.o" || exit 1; \
	done
	rm -f build/libdatalog89.a build/libdatalog89-fault.a
	@objs=""; for f in {{SRC}}; do \
	    name=$(basename "$f" .c); \
	    objs="$objs build/$name.o"; \
	done; \
	ar rcs build/libdatalog89.a $objs
	@objs=""; for f in {{CORE}}; do \
	    name=$(basename "$f" .c); \
	    objs="$objs build/$name.o"; \
	done; \
	ar rcs build/libdatalog89-fault.a $objs

# Compile the test-only plan introspection hooks; never archived into
# libdatalog89.a, so the production archive keeps a clean public surface.
testobj:
	mkdir -p build
	{{CC}} {{STRICT}} -Iinclude -Isrc -c src/datalog89_test.c -o build/datalog89_test.o

# Compile shared test fixtures. fault_mem is kept in its own archive so it
# can replace src/datalog89_priv_mem.o for the allocation-failure campaign.
fixtures: build
	@for f in test/fixtures/*.c; do \
	    [ -e "$f" ] || continue; \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -c "$f" -o "build/fix_$name.o" || exit 1; \
	done
	rm -f build/libdatalog89-fixtures.a build/libdatalog89-faultmem.a
	@objs=""; for f in test/fixtures/*.c; do \
	    [ -e "$f" ] || continue; \
	    name=$(basename "$f" .c); \
	    if [ "$name" = "fault_mem" ]; then continue; fi; \
	    objs="$objs build/fix_$name.o"; \
	done; \
	if [ -n "$objs" ]; then ar rcs build/libdatalog89-fixtures.a $objs; fi
	@if [ -e build/fix_fault_mem.o ]; then \
	    ar rcs build/libdatalog89-faultmem.a build/fix_fault_mem.o; \
	fi

# Compile-only gate: the public header must compile standalone in strict C89
# under both GCC and Clang (acceptance B01/B02/B04).
headers:
	mkdir -p build
	@for h in include/*.h; do \
	    [ -e "$h" ] || continue; \
	    printf 'headers: %s\n' "$h"; \
	    gcc {{STRICT}} -Iinclude -x c -include "$h" -c /dev/null -o /dev/null || exit 1; \
	    clang {{STRICT}} -Iinclude -x c -include "$h" -c /dev/null -o /dev/null || exit 1; \
	    gcc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Iinclude -x c -include "$h" -c /dev/null -o /dev/null || exit 1; \
	    clang -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Iinclude -x c -include "$h" -c /dev/null -o /dev/null || exit 1; \
	done

# Smoke test first: one end-to-end path through the library.
smoke: fixtures testobj
	{{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o build/smoke test/smoke.c build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a
	./build/smoke

# Unit tests, one small program per concern. test_allocfail links the core
# without src/datalog89_priv_mem.o so the fault allocator supplies the memory seam.
unit: fixtures testobj
	@for t in test/unit/test_*.c; do \
	    [ -e "$t" ] || continue; \
	    name=$(basename "$t" .c); \
	    if [ "$name" = "test_allocfail" ]; then \
	        {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o "build/$name" "$t" build/libdatalog89-fault.a build/libdatalog89-faultmem.a build/libdatalog89-fixtures.a build/datalog89_test.o || exit 1; \
	    else \
	        {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o "build/$name" "$t" build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a || exit 1; \
	    fi; \
	    ./build/$name || exit 1; \
	done

# Generated differential tests against the independent reference evaluator.
differential: fixtures testobj
	@if [ -e test/differential/test_diff.c ]; then \
	    {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o build/test_diff test/differential/test_diff.c test/differential/gen.c build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a || exit 1; \
	    DATALOG89_DIFF_N={{DIFF_N}} ./build/test_diff || exit 1; \
	fi
	@if [ -e test/differential/test_invalid.c ]; then \
	    {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o build/test_invalid test/differential/test_invalid.c build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a || exit 1; \
	    DATALOG89_DIFF_N={{DIFF_N}} ./build/test_invalid || exit 1; \
	fi
	@if [ -e test/differential/test_order.c ]; then \
	    {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o build/test_order test/differential/test_order.c test/differential/gen.c build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a || exit 1; \
	    DATALOG89_DIFF_N={{DIFF_N}} ./build/test_order || exit 1; \
	fi

# Stress correctness (bounded, non-benchmark workloads).
stress: fixtures testobj
	@if [ -e test/stress/test_stress.c ]; then \
	    {{CC}} {{STRICT}} -Wno-unused-function {{INCS}} -o build/test_stress test/stress/test_stress.c build/libdatalog89-fixtures.a build/datalog89_test.o build/libdatalog89.a || exit 1; \
	    ./build/test_stress || exit 1; \
	fi

# Run every test (smoke first, then unit, differential, stress).
test: smoke unit differential stress

# Whole suite under ASan+UBSan+LSan, with a reduced generated corpus.
sanitize:
	sh scripts/sanitize.sh

# Generate the GCC and Clang compilation databases, then run the green matrix.
green:
	sh scripts/gen_compile_db gcc build/gcc/compile_commands.json
	sh scripts/gen_compile_db clang build/clang/compile_commands.json
	{{GREEN}} check

# Green-compliance gate (the seven-cell matrix).
check: green

# Strict C89 portability baseline over all sources and headers.
baseline:
	make -f c89-baseline.mk check EXTRA="{{INCS}}"

# Lint shell scripts and check C/header formatting.
lint:
	@for f in scripts/* test/*.sh; do \
	    [ -e "$f" ] || continue; \
	    case "$f" in *.conf) continue ;; esac; \
	    shellcheck -s sh "$f" || exit 1; \
	    shellcheck -s bash "$f" || exit 1; \
	done
	@for f in src/*.c src/*.h include/*.h test/*.c test/fixtures/*.c test/unit/*.c test/stress/*.c test/differential/*.c; do \
	    [ -e "$f" ] || continue; \
	    clang-format --dry-run --Werror "$f" || { echo "needs formatting: $f"; exit 1; }; \
	done

# Rewrite sources to canonical formatting.
format:
	@for f in src/*.c src/*.h include/*.h test/*.c test/fixtures/*.c test/unit/*.c test/stress/*.c test/differential/*.c; do \
	    [ -e "$f" ] || continue; \
	    clang-format -i "$f"; \
	done

# Verify required external tools are available.
doctor:
	@command -v gcc >/dev/null 2>&1 || { echo "missing: gcc"; exit 1; }
	@command -v clang >/dev/null 2>&1 || { echo "missing: clang"; exit 1; }
	@command -v clang-tidy >/dev/null 2>&1 || { echo "missing: clang-tidy"; exit 1; }
	@command -v clang-format >/dev/null 2>&1 || { echo "missing: clang-format"; exit 1; }
	@command -v just >/dev/null 2>&1 || { echo "missing: just"; exit 1; }
	@command -v ar >/dev/null 2>&1 || { echo "missing: ar"; exit 1; }
	@echo "doctor: all prerequisites present"

clean:
	rm -rf build build-san

# Family API convention check: source rules plus this archive's
# exported-symbol classification.
api-convention: build
	sh scripts/check-api-convention.sh --symbols --lib .

# CONVENTIONS.md section 14 error-surface check.
error-convention:
	sh scripts/check-error-convention.sh --lib .
