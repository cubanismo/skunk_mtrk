/*
 * qsort - parts adapted from Doug Schmidt's sort challenge posting
 *	   thanks Doug
 *
 *		++jrb	bammi@dsrgsun.ces.cwru.edu
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#ifdef __GNUC__
#  ifdef minix
     void *alloca(unsigned long);
     typedef unsigned long size_t;
#  endif
#  define INLINE inline
#else
#  define INLINE /* */
#endif
#ifndef _COMPILER_H
#include <compiler.h>
#endif

/* macros for incrementing/decrementing void pointers */
#define INC(v, size) v = (void *)(((char *)v) + size)
#define DEC(v, size) v = (void *)(((char *)v) - size)

    /* the next 4 #defines implement a very fast in-line stack abstraction */
    
#define MAKE_STACK(S) \
    ((stack_node *) alloca((size_t)(sizeof(stack_node) * (S))))
#define PUSH(LOW,HIGH) \
    top->lo = LOW; top->hi = HIGH; top++
#define POP(LOW,HIGH) \
    --top; LOW = top->lo; HIGH = top->hi
#define STACK_NOT_EMPTY \
    (stack < top)                

INLINE static void swap __PROTO((unsigned char *, unsigned char *, size_t));
INLINE static void Move __PROTO((unsigned char *, unsigned char *, size_t));
INLINE static void insert_sort __PROTO((void *, void *, size_t, int (*)(const void *, const void *)));
INLINE static void *find_pivot __PROTO((void *, void *, size_t, int (*)(const void *, const void *)));

/* Discontinue quicksort algorithm when partition gets below this size. */

#ifndef MAX_THRESH
#define MAX_THRESH 15
#endif

/* Data structure for stack of unfulfilled obligations. */
typedef struct 
{
    void *lo;
    void *hi;
} stack_node;

static void *scratch;	/* scratch mem */

/* swap two elements of size n each */
INLINE static void swap(a, b, n)
unsigned char *a, *b;
size_t n;
{
    unsigned char t;
    while(n--)
    {
	t    = *a;
        *a++ = *b;
	*b++ = t;
    }
}


/* move src to dest */
INLINE static void Move(src, dst, n)
unsigned char *src, *dst;
size_t n;
{
    while(n--)
	*dst++ = *src++;
}


/* Once main array is partially sorted by quicksort the remainder is 
   completely sorted using insertion sort, since this is efficient 
   for partitions below MAX_THRESH size. BASE points to the beginning 
   of the array to sort, and END_PTR points at the very last element in
   the array (*not* one beyond it!). */

INLINE static void 
#if __STDC__
    insert_sort(void *base, void *end_ptr, size_t siz, int (*cmp)(const void *,
								  const void *))
#else
    insert_sort(base, end_ptr, siz, cmp)
    void *base, *end_ptr;
    size_t siz;
    int (*cmp)();
#endif
{
    void *run_ptr;
    void *tmp_ptr = end_ptr;
    void *temp = scratch;

    /* Find the largest element in the array and put it at the 
       end of the array.  This acts like a sentinel, and it speeds
       up the inner loop of insertion sort. */

    for (run_ptr = (void *)((char *)end_ptr - siz); run_ptr >= base;
	 DEC(run_ptr, siz))
	if ((*cmp)(run_ptr, tmp_ptr) > 0) 
	    tmp_ptr = run_ptr;

    if(tmp_ptr != end_ptr)
	{ swap (tmp_ptr, end_ptr, siz); }

    /* Typical insertion sort, but we run from the `right-hand-side'
       downto the `left-hand-side.' */
    
    for (run_ptr = (void *)((char *)end_ptr - siz); run_ptr > base;
	 DEC(run_ptr, siz))
    {
	tmp_ptr = (void *)((char *)run_ptr - siz);
	Move(tmp_ptr, temp, siz);

	/* Select the correct location for the new element, 
	   by sliding everyone down by 1 to make room! */
	
#if 0
	while ((*cmp)(temp , ((char *)tmp_ptr += siz)) > 0)
#else
	while (((INC(tmp_ptr, siz)), (*cmp)(temp, tmp_ptr)) > 0)
#endif
	    Move(tmp_ptr, ((unsigned char *)tmp_ptr - siz), siz);

	Move(temp, (unsigned char *)tmp_ptr - siz, siz);
    }
}

/* Return the median value selected from among the 
   LOW, MIDDLE, and HIGH.  Rearrange LOW and HIGH so
   the three values are sorted amongst themselves. 
   This speeds up later work... */

INLINE static void *
#if __STDC__
    find_pivot(void *low, void *high, size_t siz, int (*cmp)(const void *,
							     const void *))
#else
    find_pivot(low, high, siz, cmp)
    void *low, *high;
    size_t siz;
    int (*cmp)();
#endif
{
    void *middle = (void *) ((char *)low + ((((char *)high - (char *)low)/siz) >> 1) * siz);
    
    if ((*cmp)(middle, low) < 0)
	{ swap (middle, low, siz); }
    if ((*cmp)(high, middle) < 0)
	{ swap (middle, high, siz); }
    else 
	return middle;
    
    if ((*cmp)(middle, low)  < 0)
	{ swap (middle, low, siz); }
    
    return middle;
}

/* Order elements using quicksort.  This implementation incorporates
   4 optimizations discussed in Sedgewick:
   
   1. Non-recursive, using an explicit stack of log (n) pointers that 
   indicate the next array partition to sort.
   
   2. Choses the pivot element to be the median-of-three, reducing
   the probability of selecting a bad pivot value.
   
   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
   insertion sort to sort within the partitions.  This is a
   big win, since insertion sort is faster for small, mostly
   sorted array segements.
   
   4. The larger of the 2 sub-partitions are always pushed onto the
   stack first, with the algorithm then concentrating on the
   smaller partition.  This *guarantees* no more than log (n)
   stack size is needed! */

void qsort(base, total_elems, size, cmp)
void *base;
size_t total_elems;
size_t size;
int (*cmp) __PROTO((const void *, const void *));
{
    if (total_elems > 1)
    {
	void        *lo;
	void        *hi;
	void        *left_ptr;
	void        *right_ptr;
	void	    *pivot;
	size_t	    Thresh = MAX_THRESH * size;
	stack_node  *stack;
	stack_node  *top;  
	size_t max_stack_size = 1;

	/* Calculate the exact stack space required in the worst case.
	   This is approximately equal to the log base 2 of TOTAL_ELEMS. */
	
	{   size_t i;	/* this helps the compiler */
	    for (i = 1; i < total_elems; i += i) 
	        max_stack_size++;
	}
	/* Create the stack, or die trying! */
	if ((stack = MAKE_STACK (max_stack_size)) != NULL)
	    top = stack;
	else
	    return;

	/* allocate scratch */
	if((scratch = (void *)alloca(size)) == (void *)0)
		return;	/* die */
	
	lo = base;
	hi = (void *) ((char *)lo + (total_elems - 1) * size);

	do {
	    next: if((char *)hi <= ((char *)lo + Thresh)) /* correct unsigned comapare */
	    {
		insert_sort(lo, hi, size, cmp);
		if(STACK_NOT_EMPTY)
		    {  POP(lo, hi); goto next; }
		else
		    break;
	    }
	    /* otherwise next iteration of qsort */
	    /* Select the median-of-three here. This allows us to
	       skip forward for the LEFT_PTR and RIGHT_PTR. */
	    pivot = find_pivot (lo, hi, size, cmp);
	    left_ptr  = (void *)((char *)lo + size);
	    right_ptr = (void *)((char *)hi - size); 

	    /* Here's the famous ``collapse the walls'' section of
	       quicksort.  Gotta like those tight inner loops! */
	    do { /* partition loop */  /* see knuth for <= */
		while ((left_ptr < hi) && ((*cmp)(left_ptr, pivot) <= 0))
		    INC(left_ptr, size);
		
		while ((right_ptr > lo) && ((*cmp)(pivot, right_ptr) <= 0))
		    DEC(right_ptr, size);
		
		if (left_ptr < right_ptr) 
                {
		    swap (left_ptr, right_ptr, size);
		    INC(left_ptr,  size);
		    DEC(right_ptr, size);
                }
		else if (left_ptr == right_ptr) 
                {
		    INC(left_ptr, size); 
		    DEC(right_ptr, size); 
		    break;
                }
            } while (left_ptr <= right_ptr);
	    
	    if (((char *)right_ptr - (char *)lo) > ((char *)hi - (char *)left_ptr)) 
            {                   /* push larger left table */
		PUSH (lo, right_ptr);
		lo = left_ptr;
            }
	    else 
            {                   /* push larger right table */
		PUSH (left_ptr, hi);
		hi = right_ptr;
            }
        } while(1);	/* when stack is empty it'll break out */
    } /* if total_elems > 1 */
}
