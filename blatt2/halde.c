#include "halde.h"

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/// Magic value for occupied memory chunks.
#define MAGIC ((void*)0xbaadf00d)

/// Size of the heap (in bytes).
#define SIZE (1024*1024*1)

/// Memory-chunk structure.
struct mblock {
	struct mblock* next;
	size_t size;
	char memory[];
};

#define MBLOCK_SIZE sizeof(struct mblock)

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

/// Helper function to visualize the current state of the free-memory list.
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

void* halde_malloc (const size_t size) {
	if (!head) {
		// simply not enough memory
		if (size > SIZE + MBLOCK_SIZE) {
			errno = ENOMEM;
			return NULL;
		}
		
		// use the initial pointer of the array as a starting block
		head = (struct mblock*) memory;
		head->size = size;
		head->next = NULL;
		return head->memory;
	}

	struct mblock* current = head;
	size_t global_size = current->size + MBLOCK_SIZE;
	
	// get to the last block
	while (current->next) {
		current = current->next;
		global_size += current->size + MBLOCK_SIZE;
	}

	// printf("%lu\n%lu\n%d\n", global_size, global_size + MBLOCK_SIZE + size, SIZE);

	if (global_size + MBLOCK_SIZE + size > SIZE) {
		errno = ENOMEM;
		return NULL;
	}

	struct mblock* new_block = (struct mblock*) (current->memory + global_size);
	current->next = new_block;
	new_block->size = size;
	new_block->next = NULL;
	
	return new_block->memory;
}

void halde_free (void *ptr) {
	//FREE_NOT_IMPLEMENTED_MARKER: remove this line to activate free related test cases

	//MERGE_NOT_IMPLEMENTED_MARKER: remove this line to activate merge related test cases

	// TODO: implement me!
	return;
}
