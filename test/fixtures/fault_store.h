#ifndef FAULT_STORE_H
#define FAULT_STORE_H

#include <stddef.h>

#include <dl89.h>

#include "ref_store.h"

typedef struct fault_store fault_store;

fault_store *fault_store_new(ref_store *inner);
void fault_store_free(fault_store *fs);
dl89_store fault_store_dl89(fault_store *fs);

/* Fail the nth matching operation once; 0 disables failure. */
void fault_store_fail_scan_open(fault_store *fs, unsigned long n);
void fault_store_fail_scan_next(fault_store *fs, unsigned long n);
void fault_store_fail_insert(fault_store *fs, unsigned long n);

/* Invoke the hook at the start of every scan_open. */
void fault_store_set_hook(fault_store *fs, void (*hook)(void *ctx), void *ctx);

/* Validate callback arguments and close-once discipline. */
void fault_store_set_validating(fault_store *fs, int on);

unsigned long fault_store_live_scans(const fault_store *fs);
unsigned long fault_store_total_opens(const fault_store *fs);
unsigned long fault_store_total_closes(const fault_store *fs);
unsigned long fault_store_open_calls(const fault_store *fs);
unsigned long fault_store_next_calls(const fault_store *fs);
unsigned long fault_store_insert_calls(const fault_store *fs);
unsigned long fault_store_arg_errors(const fault_store *fs);

#endif
