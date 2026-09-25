# 0003 — status scenarios (CONVENTIONS.md section 14)

## SCENARIO DL-STATUS-1 — signed partition

GIVEN a fallible libdl89 operation
WHEN it succeeds THEN it returns `DL89_OK`, which is zero
AND WHEN it fails THEN it returns a negative `dl89_status`.

## SCENARIO DL-STATUS-2 — explicit stable values

GIVEN the public `dl89_status` enum
WHEN it is inspected
THEN every enumerator carries an explicit numeric value
AND `DL89_OK` is zero and every failure is negative.

## SCENARIO DL-STATUS-3 — the operational return type

GIVEN a public fallible operation
WHEN it is declared
THEN it returns `dl89_status` rather than a bare `int`.

## SCENARIO DL-STATUS-4 — inspection functions are total

GIVEN `dl89_status_name` and `dl89_status_message`
WHEN they receive a known or unknown value
THEN each returns a non-NULL pointer to immutable static storage
AND `dl89_status_name` returns the stable token for known values and
    `"UNKNOWN"` otherwise.

## SCENARIO DL-STATUS-5 — portable profile

GIVEN libdl89 declares the portable status profile
WHEN any operation runs
THEN the library neither reads nor writes `errno`.
