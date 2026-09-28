/* fault_mem.c - allocation-failure injection behind the internal memory seam.
 *
 * This fixture defines datalog89_priv_mem_alloc/realloc/free. The
 * allocation-failure campaign links the core library without
 * src/datalog89_priv_mem.o so these symbols replace the default allocator.
 * Allocation number n (1-based) returns NULL once; 0 disables failure. Counters
 * keep running while failure is disabled, which lets a test measure how many
 * allocations a path performs.
 */

#include <stdlib.h>

#include "datalog89_priv.h"
#include "fault_mem.h"

static unsigned long fm_count;
static unsigned long fm_fail_at;
static unsigned long fm_frees;

void fault_mem_reset(void)
{
    fm_count = 0;
    fm_fail_at = 0;
    fm_frees = 0;
}

void fault_mem_fail_at(unsigned long n)
{
    fm_fail_at = n;
}

void fault_mem_disable(void)
{
    fm_fail_at = 0;
}

unsigned long fault_mem_allocations(void)
{
    return fm_count;
}

unsigned long fault_mem_frees(void)
{
    return fm_frees;
}

static int should_fail(void)
{
    fm_count = fm_count + 1;
    if (fm_fail_at == 0)
    {
        return 0;
    }
    return fm_count == fm_fail_at;
}

void *datalog89_priv_mem_alloc(size_t size)
{
    if (size == 0)
    {
        size = 1;
    }
    if (should_fail() != 0)
    {
        return NULL;
    }
    return malloc(size);
}

void *datalog89_priv_mem_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        size = 1;
    }
    if (should_fail() != 0)
    {
        return NULL;
    }
    return realloc(ptr, size);
}

void datalog89_priv_mem_free(void *ptr)
{
    if (ptr != NULL)
    {
        fm_frees = fm_frees + 1;
    }
    free(ptr);
}
