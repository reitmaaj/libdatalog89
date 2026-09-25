#!/bin/sh -eu
# check-error-convention.sh - enforce CONVENTIONS.md section 14.
#
# Usage:
#   check-error-convention.sh                source-level error checks
#   check-error-convention.sh --lib DIR      restrict to one library
#   check-error-convention.sh --conf FILE    use an alternate configuration
#
# The configuration is scripts/error-convention.conf. Only libraries listed
# there are audited; a library is added when it is converted, so that main
# stays green during the migration.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
CONF="$ROOT/scripts/error-convention.conf"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
LIB=""
while [ $# -gt 0 ]; do
    case "$1" in
        --lib)
            shift
            if [ $# -eq 0 ]; then
                printf 'check-error-convention: --lib needs a directory\n' >&2
                exit 2
            fi
            LIB=$1
            ;;
        --conf)
            shift
            if [ $# -eq 0 ]; then
                printf 'check-error-convention: --conf needs a file\n' >&2
                exit 2
            fi
            CONF=$1
            ;;
        *)
            printf 'check-error-convention: unknown argument: %s\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

if [ ! -f "$CONF" ]; then
    printf 'check-error-convention: configuration not found: %s\n' "$CONF" >&2
    exit 2
fi

fail=0

report()
{
    printf 'error-convention: %s\n' "$1" >&2
    fail=1
}

# Ordinary-outcome tokens: a positive enumerator MUST match this set; a
# negative enumerator MUST NOT.
ORDINARY='(END|AGAIN|NOT_FOUND|NOTFOUND|EXISTS|CONFLICT|DONE|BUSY|EMPTY|REJECT|REJECTED|PENDING|COMPLETE|EOF|RETRY)'

# Remove C comments so prose that merely mentions a token does not count as a
# public contract.
strip_comments()
{
    awk '
    BEGIN { inb = 0 }
    {
        line = $0
        out = ""
        i = 1
        n = length(line)
        while (i <= n) {
            c2 = substr(line, i, 2)
            if (inb) {
                if (c2 == "*/") { inb = 0; i += 2 } else { i += 1 }
            } else if (c2 == "/*") {
                inb = 1
                i += 2
            } else if (c2 == "//") {
                break
            } else {
                out = out substr(line, i, 1)
                i += 1
            }
        }
        print out
    }'
}

check_enum()
{
    dir=$1
    prefix=$2
    status_type=$3
    names=$4
    extra=$5

    found=0
    for h in $(find "$ROOT/$dir/include" -type f -name '*.h' 2>/dev/null | sort); do
        if ! grep -qE "enum[[:space:]]+${status_type}" "$h" 2>/dev/null; then
            continue
        fi
        found=1
        awk -v name="$status_type" -v ordinary="$ORDINARY" -v extra="$extra" '
            function check(tok, val,    n, a) {
                if (tok ~ /_OK$/ || tok == "OK") {
                    if (val != 0)
                        print "OK enumerator is not zero: " tok "=" val
                    return
                }
                if (val == 0)
                    return
                if (val > 0) {
                    if (tok ~ ("_" ordinary "$"))
                        return
                    if (extra != "-" && extra != "" && tok ~ ("_" extra "$"))
                        return
                    print "positive enumerator is not a recognized ordinary outcome: " tok "=" val
                } else if (tok ~ ("_" ordinary "$")) {
                    print "ordinary-outcome enumerator is negative: " tok "=" val
                }
            }
            BEGIN { inb = 0 }
            {
                if ($0 ~ ("enum[ \t]+" name))
                    inb = 1
                if (inb) {
                    line = $0
                    sub(/\/\/.*/, "", line)
                    sub(/\/\*.*\*\//, "", line)
                    if (match(line, /[A-Z][A-Z0-9_]*[ \t]*=[ \t]*-?[0-9]+/)) {
                        s = substr(line, RSTART, RLENGTH)
                        n = split(s, a, /[ \t]*=[ \t]*/)
                        check(a[1], a[2] + 0)
                    } else if (line ~ /^[ \t]*[A-Z][A-Z0-9_]*[ \t]*,?[ \t]*$/) {
                        s = line
                        gsub(/[ \t,]/, "", s)
                        print "enumerator without explicit value: " s
                    }
                }
                if (inb && $0 ~ /}/)
                    exit
            }
        ' "$h" > "$TMP/enum-out"
        while IFS= read -r msg; do
            [ -n "$msg" ] || continue
            report "$dir: $msg"
        done < "$TMP/enum-out"
        break
    done
    if [ "$found" = 0 ]; then
        report "$dir: public status enum $status_type not found"
    fi

    if [ "$names" = yes ]; then
        for sym in "${prefix}_status_name" "${prefix}_status_message"; do
            if ! grep -rqE "\\b${sym}[[:space:]]*\\(" "$ROOT/$dir/include" 2>/dev/null; then
                report "$dir: $sym is not declared in a public header"
            fi
        done
    fi
}

check_profile()
{
    dir=$1
    profile=$2

    if [ "$profile" = portable ]; then
        for h in $(find "$ROOT/$dir/include" -type f -name '*.h' 2>/dev/null | sort); do
            hits=$(strip_comments < "$h" | grep -nE '\berrno\b' || true)
            if [ -n "$hits" ]; then
                report "$dir: portable profile exposes errno in $h"
                printf '%s\n' "$hits" >&2
            fi
        done
    fi

    bad=$(grep -rnE '\berrno[[:space:]]*=[[:space:]]*[1-9][0-9]*' "$ROOT/$dir/src" 2>/dev/null || true)
    if [ -n "$bad" ]; then
        report "$dir: invented numeric value assigned to errno"
        printf '%s\n' "$bad" >&2
    fi

    bad=$(grep -rnE '#[[:space:]]*define[[:space:]]+[A-Z][A-Z0-9_]*[[:space:]]+2[0-9][0-9][0-9]([^0-9]|$)' \
        "$ROOT/$dir/include" 2>/dev/null || true)
    if [ -n "$bad" ]; then
        report "$dir: public macro claims the invented-errno range 2000-2999"
        printf '%s\n' "$bad" >&2
    fi
}

matched=0
while read -r dir prefix status_type profile names extra; do
    case "$dir" in
        ''|\#*) continue ;;
    esac
    if [ -n "$LIB" ] && [ "$dir" != "$LIB" ]; then
        continue
    fi
    matched=1
    if [ ! -d "$ROOT/$dir" ]; then
        report "$dir: configured library directory missing"
        continue
    fi
    check_profile "$dir" "$profile"
    if [ "$status_type" != "-" ]; then
        check_enum "$dir" "$prefix" "$status_type" "$names" "$extra"
    fi
done < "$CONF"

if [ -n "$LIB" ] && [ "$matched" -eq 0 ]; then
    report "$LIB: not configured in error-convention.conf"
fi

if [ "$fail" -ne 0 ]; then
    printf 'error-convention: FAILED\n' >&2
    exit 1
fi
printf 'error-convention: ok\n'
