#include "halde.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/// Magic value for occupied memory chunks.
#define MAGIC ((void*)0xbaadf00d)

/// Size of the heap (in bytes).
#define SIZE (1024 * 1024 * 1)

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
struct canary canary = {.mem = {0}, .can = 0xdeadb33f};

/// Heap-memory area. Due to the conversion sizeof(memory) does not work, sizeof(canary.mem) works.
char* memory = (char*)canary.mem;

/// Pointer to the first element of the free-memory list.
static struct mblock* head;

/// Helper function to visualize the current state of the free-memory list.
void halde_print(void) {
	struct mblock* lauf = head;

	// Empty list
	if (head == NULL) {
		fprintf(stderr, "(empty)\n");
		return;
	}

	// Print each element in the list
	fprintf(stderr, "HEAD:  ");
	while (lauf) {
		fprintf(stderr, "(addr: 0x%08zx, off: %7zu, ", (uintptr_t)lauf, (uintptr_t)lauf - (uintptr_t)memory);
		fflush(stderr);
		fprintf(stderr, "size: %7zu)", lauf->size);
		fflush(stderr);

		if (lauf->next != NULL) {
			fprintf(stderr, "\n  -->  ");
			fflush(stderr);
		}
		lauf = lauf->next;
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}

void* halde_malloc(const size_t size) {
	// align: lower 4 bits must be 0
	// const size_t ALIGN_MASK = MBLOCK_SIZE - 1;

	// size_t size = size_;
	// if (size & ALIGN_MASK) {
	// 	// size can't be used properly in conjunction with MBLOCK_SIZE
	// 	// this helps later when assigning memory sections
	// 	size = (size & ~ALIGN_MASK) + MBLOCK_SIZE;
	// }

	/* no alloc
	 * *head
	 * [size][next][   mem   ]
	 * [1008][NULL][         ]
	 * 1024-(1*16)-(16*0)
	 */
	
	/* one alloc
	 *             *m1        *head
	 * [size][next][   mem   ][size][next][   mem   ]
	 * [ 16 ][FOOD][         ][ 976][NULL][         ]
	 *                        1024-(2*16)-(16*1)
	 */
	
	/* two alloc
	 *             *m1                    *m2        *head
	 * [size][next][   mem   ][size][next][   mem   ][size][next][   mem   ]
	 * [ 16 ][FOOD][         ][ 16 ][FOOD][         ][ 944][NULL][         ]
	 *                                               1024-(3*16)-(16*2)
	 */

	/* free space
	 *             *m1        *head                              *m2        *1->
	 * [size][next][   mem   ][size][next][   mem   ][size][next][   mem   ][size][next][   mem   ]
	 * [ 16 ][FOOD][         ][ 16 ][*1->][         ][ 16 ][FOOD][         ][ 912][NULL][         ]
	 *                                                                      1024-(4*16)-(16*3)
	 */
	
	if (!head) {
		// use the initial pointer of the memory as the starting block
		head = (struct mblock*)memory;
		head->size = SIZE - MBLOCK_SIZE;
		head->next = NULL;
	}

	struct mblock* current = head;

	while (current) {
		// first fit
		if (current->size >= size) {
			break;
		}

		current = current->next;
	}

	// no memory available
	if (!current) {
		errno = ENOMEM;
		return NULL;
	}

	struct mblock* new_block = (struct mblock*)((char*)current + MBLOCK_SIZE + size);
	
	// printf("mem %p\n", memory);
	// printf("cur %p\n", (void*) current);
	// printf("new %p\n", (void*) new_block);
	// printf("max %p\n", memory + SIZE);
	// printf("del %lu\n", new_block - current);
	// fflush(stdout);

	new_block->size = current->size - MBLOCK_SIZE - size;
	new_block->next = NULL;

	head = new_block;

	current->size = size;
	current->next = MAGIC;

	return current->memory;
}

void halde_free(void* ptr) {
	//FREE_NOT_IMPLEMENTED_MARKER: remove this line to activate free related test cases

	//MERGE_NOT_IMPLEMENTED_MARKER: remove this line to activate merge related test cases

	// TODO: implement me!
	return;
}
