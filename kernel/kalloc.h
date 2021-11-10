#ifndef KALLOC_H_
#define KALLOC_H_

/*
* Initialize physical memory management.
*/
void kinit();

/* 
* Returns a free physical page.
* Returns 0 if the memory cannot be allocated.
*/
void* kalloc(void);

/*
* Decrement number of references of a physical page and if the number 
* of references reaches 0 then mark it as free.
*/
void kdecref(void* pa); 

/*
* Increment number of references of a physical page.
*/
void kincref(void* pa); 

#endif