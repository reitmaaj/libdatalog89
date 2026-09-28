# 0003 — status acceptance (CONVENTIONS.md section 14)

Run `just test` and `just green` in `libdatalog89`.

## Must exhibit

- A1. `DATALOG89_OK == 0` and every `datalog89_status` failure is negative.
- A2. Every `datalog89_status` enumerator carries an explicit numeric value.
- A3. The public fallible operations return `datalog89_status`.
- A4. `datalog89_status_name()` returns `"OK"`, a stable failure token, and
      `"UNKNOWN"` for an unknown value; it is total and allocation-free.
- A5. `datalog89_status_message()` returns non-NULL static text for every input.
- A6. `just check-error-convention --lib libdatalog89` reports
      `error-convention: ok`.

## Must reject (unacceptable behavior)

- B1. A `datalog89_status` failure with a positive value.
- B2. A `datalog89_status` enumerator without an explicit value.
- B3. `datalog89_status_name()` returning NULL for an unknown value.
- B4. A public fallible operation declared with a bare `int` return type.
- B5. Any `errno` read or write in the library.
