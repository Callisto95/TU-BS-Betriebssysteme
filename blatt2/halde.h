#pragma once

#include <sys/types.h>

/*
   halde_malloc() allocates size bytes and returns a pointer to the
   allocated memory. The memory is not cleared.

   RETURN VALUE: The value returned is a pointer
   to the allocated memory or NULL if the request fails. The
   errno will be set to indicate the error.
*/
void* halde_malloc(size_t size);

/*
   halde_free() frees the memory space pointed to by ptr, which must
   have been returned by a previous call to halde_malloc(). Otherwise,
   or if halde_free(ptr) has already been called before the program is
   aborted. If ptr is NULL, no operation is performed.

   RETURN VALUE: no value
*/
void halde_free(void* ptr);

/*
 * halde_print() is a non-standard function which prints the internal
 * state of the free list.
 *
 * This function can be used to debug the implementation and compare the
 * behavior with other implementations.
 */
void halde_print(void);
