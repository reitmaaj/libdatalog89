#ifndef FAULT_MEM_H
#define FAULT_MEM_H

#include <stddef.h>

void fault_mem_reset(void);
void fault_mem_fail_at(unsigned long n);
void fault_mem_disable(void);
unsigned long fault_mem_allocations(void);
unsigned long fault_mem_frees(void);

#endif
