#ifndef KALLOC_H_
#define KALLOC_H_

#include "types.h"

/*
* Initialize physical memory management.
 */
void kinit();

/* 
 * Returns a free physical page.
 * Returns 0 if the memory cannot be allocated.
 */
uint64 kalloc();

/*
 * Decrement number of references of a physical page and if the number 
 * of references reaches 0 then mark it as free.
 */
void kdecref(uint64 pa); 

/*
 * Increment number of references of a physical page.
 */
void kincref(uint64 pa); 

/*
 * Check if page is uniquely referenced.
 */
int ksingleref(uint64 pa);

#endif
