#!/bin/sh -eu
# sanitize.sh - build and run the whole suite under ASan+UBSan+LSan.
#
# The generated differential corpus and the stress sizes are reduced so the
# instrumented run stays bounded. Any sanitizer report or failing test aborts
# the script.

set -eu

CC="${CC:-cc}"

compile() {
    "$CC" -std=c89 -pedantic-errors -Wall -Wextra -Werror \
        -Wconversion -Wsign-conversion -Wstrict-prototypes \
        -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow \
        -Wformat=2 -Wno-long-long -Iinclude -Isrc -Itest -Itest/fixtures \
        -Itest/differential -fsanitize=address,undefined \
        -fno-omit-frame-pointer -g "$@"
}

mkdir -p build-san

for f in src/*.c; do
    name=$(basename "$f" .c)
    compile -c "$f" -o "build-san/$name.o"
done

set --
for f in src/*.c; do
    name=$(basename "$f" .c)
    if [ "$name" = "datalog89_priv_mem" ] || [ "$name" = "datalog89_test" ]; then
        continue
    fi
    set -- "$@" "build-san/$name.o"
done
rm -f build-san/libdatalog89-fault.a
ar rcs build-san/libdatalog89-fault.a "$@"

set --
for f in src/*.c; do
    name=$(basename "$f" .c)
    if [ "$name" = "datalog89_test" ]; then
        continue
    fi
    set -- "$@" "build-san/$name.o"
done
rm -f build-san/libdatalog89.a
ar rcs build-san/libdatalog89.a "$@"

for f in test/fixtures/*.c; do
    name=$(basename "$f" .c)
    compile -Wno-unused-function -c "$f" -o "build-san/fix_$name.o"
done

set --
for f in test/fixtures/*.c; do
    name=$(basename "$f" .c)
    if [ "$name" = "fault_mem" ]; then
        continue
    fi
    set -- "$@" "build-san/fix_$name.o"
done
rm -f build-san/libdatalog89-fixtures.a build-san/libdatalog89-faultmem.a
ar rcs build-san/libdatalog89-fixtures.a "$@"
ar rcs build-san/libdatalog89-faultmem.a build-san/fix_fault_mem.o

ASAN_OPTIONS=detect_leaks=1
export ASAN_OPTIONS

compile -Wno-unused-function -o build-san/smoke test/smoke.c \
    build-san/libdatalog89-fixtures.a build-san/datalog89_test.o \
    build-san/libdatalog89.a
"./build-san/smoke"

for t in test/unit/test_*.c; do
    name=$(basename "$t" .c)
    if [ "$name" = "test_allocfail" ]; then
        compile -Wno-unused-function -o "build-san/$name" "$t" \
            build-san/libdatalog89-fault.a build-san/libdatalog89-faultmem.a \
            build-san/libdatalog89-fixtures.a build-san/datalog89_test.o
    else
        compile -Wno-unused-function -o "build-san/$name" "$t" \
            build-san/libdatalog89-fixtures.a build-san/datalog89_test.o \
            build-san/libdatalog89.a
    fi
    "./build-san/$name"
done

compile -Wno-unused-function -o build-san/test_diff \
    test/differential/test_diff.c test/differential/gen.c \
    build-san/libdatalog89-fixtures.a build-san/datalog89_test.o \
    build-san/libdatalog89.a
DATALOG89_DIFF_N=1000 "./build-san/test_diff"

compile -Wno-unused-function -o build-san/test_invalid \
    test/differential/test_invalid.c build-san/libdatalog89-fixtures.a \
    build-san/datalog89_test.o build-san/libdatalog89.a
DATALOG89_DIFF_N=1000 "./build-san/test_invalid"

compile -Wno-unused-function -o build-san/test_order \
    test/differential/test_order.c test/differential/gen.c \
    build-san/libdatalog89-fixtures.a build-san/datalog89_test.o \
    build-san/libdatalog89.a
DATALOG89_DIFF_N=1000 "./build-san/test_order"

compile -Wno-unused-function -o build-san/test_stress test/stress/test_stress.c \
    build-san/libdatalog89-fixtures.a build-san/datalog89_test.o \
    build-san/libdatalog89.a
DATALOG89_STRESS_N=200 "./build-san/test_stress"

echo "sanitize: OK"
