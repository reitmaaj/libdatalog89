#ifndef REF_EVAL_H
#define REF_EVAL_H

#include <stddef.h>

#include <dl89.h>

/* Independent worklist evaluator used only as a test oracle. Each newly
 * inserted fact drives every rule/body-position in which it can appear; the
 * remaining body atoms are scanned from the store. Returns 0 on success and
 * 1 if a store callback failed. Shares no source with libdl89. */
int ref_eval_run(dl89_store store, const dl89_rule *rules, size_t rule_count);

#endif
