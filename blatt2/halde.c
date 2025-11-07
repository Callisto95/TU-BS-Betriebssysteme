#include "halde.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

/// Magic value for occupied memory chunks.
#define MAGIC ((void*)0xbaadf00d)

/// Size of the heap (in bytes).
#define SIZE (1024*1024*1)

/// Memory-chunk structure.
struct mblock {
	struct mblock *next;
	size_t size;
	char memory[];
};

/// Canary wrapper around the memory, used to detect writes beyond mem.
/// That is, if we write beyond the boundaries of mem, then we overwrite
/// the canary (enabling the tests to detect this edge case).
struct canary {
	char mem[SIZE];
	int can;
};
struct canary canary = {.mem={0},.can = 0xdeadb33f};

/// Heap-memory area. Due to the conversion sizeof(memory) does not work, sizeof(canary.mem) works.
char *memory = (char *) canary.mem;

/// Pointer to the first element of the free-memory list.
static struct mblock *head;

/// Helper function to visualise the current state of the free-memory list.
void halde_print(void) {
	struct mblock* lauf = head;

	// Empty list
	if ( head == NULL ) {
		fprintf(stderr, "(empty)\n");
		return;
	}

	// Print each element in the list
	fprintf(stderr, "HEAD:  ");
	while ( lauf ) {
		fprintf(stderr, "(addr: 0x%08zx, off: %7zu, ", (uintptr_t) lauf, (uintptr_t)lauf - (uintptr_t)memory);
		fflush(stderr);
		fprintf(stderr, "size: %7zu)", lauf->size);
		fflush(stderr);

		if ( lauf->next != NULL ) {
			fprintf(stderr, "\n  -->  ");
			fflush(stderr);
		}
		lauf = lauf->next;
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}

void *halde_malloc (size_t size) {
	//MALLOC_NOT_IMPLEMENTED_MARKER: remove this line to activate malloc related test cases
	// TODO: implement me!
	return NULL;
}

void halde_free (void *ptr) {
	//FREE_NOT_IMPLEMENTED_MARKER: remove this line to activate free related test cases

	//MERGE_NOT_IMPLEMENTED_MARKER: remove this line to activate merge related test cases

	// TODO: implement me!
	return;
}
