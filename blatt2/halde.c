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

/**
 * @brief Fully remove an mblock from memory.
 *
 * @details Zero out the memory block as well as any metadata in the mblock.
 */
void delete_block(struct mblock* block) {
    char* current = block->memory;
    while (current < block->memory + block->size) {
        *current = 0;
        current++;
    }

    block->next = NULL;
    block->size = 0;
}

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
        struct mblock* new_head = (struct mblock*)memory;

        // earlier allocations filled the memory
        if (new_head->next == MAGIC) {
            return NULL;
        }

        head = new_head;

        head->size = SIZE - MBLOCK_SIZE;
        head->next = NULL;
    }

    struct mblock* current = head;
    struct mblock* previous = NULL;

    while (current) {
        // first fit
        if (current->size >= size) {
            break;
        }

        previous = current;
        current = current->next;
    }

    // no memory available
    if (!current) {
        errno = ENOMEM;
        return NULL;
    }

    // remaining space in block
    const long int remaining_space = current->size - MBLOCK_SIZE - size;

    // does another block fit in the space after? if yes, create one
    // also prevent implicit casts to unsigned
    if (remaining_space >= (long long)MBLOCK_SIZE) {
        struct mblock* new_block = (struct mblock*)((char*)current + MBLOCK_SIZE + size);
        new_block->size = remaining_space;
        new_block->next = current->next;

        if (previous) {
            // there exists a block before the new one
            // update the pointer accordingly
            previous->next = new_block;
        } else {
            // no block is in front of the new block, so new block becomes the first
            head = new_block;
        }
    } else {
        // no space to create a block, so either...
        if (previous) {
            // ...skip the empty space...
            previous->next = current->next;
        } else {
            // ...or we were at the first block
            head = current->next;
        }
    }

    // allocate block
    current->size = size;
    current->next = MAGIC;

    return current->memory;
}

void halde_free(void* ptr) {
    if (!ptr) {
        return;
    }

    struct mblock* block = ptr;
    block--;

    // block is not an allocated block
    if (block->next != MAGIC) {
        abort();
    }

    if (!head) {
        // memory was filled, no end block exists
        block->next = NULL;
        head = block;
        return;
    }

    if (head > block) {
        // head (beginning of the list) is after the block, making our block the new head

        if ((char*)block + MBLOCK_SIZE + block->size == (char*)head) {
            // head is right after the block
            block->size = block->size + MBLOCK_SIZE + head->size;
            block->next = head->next;
        } else {
            // head is not right after the block
            block->next = head;
        }

        head = block;
        return;
    }

    // block is after head
    // this means there's a pointer going from _before_ block to _after_
    // or
    // block is after the end of the list

    struct mblock* current = head;
    while (current) {
        if (current < block && current->next > block) {
            // 3 options:
            // 1: current is right before block
            // 2: current->next is right after block
            // 3: neither, meaning there are allocated blocks before and after block

            // 2 before 1, so that we're pulling free blocks from the back to the front (if possible)

            int has_merged = 0;

            // 2: current->next is right after block
            if ((char*)block + MBLOCK_SIZE + block->size == (char*)current->next) {
                struct mblock* next_block = current->next;
                block->next = next_block->next;
                block->size = block->size + MBLOCK_SIZE + next_block->size;

                delete_block(next_block);
                current->next = block;

                has_merged = 1;
            }

            // 1: current is right before block
            if ((char*)current + MBLOCK_SIZE + current->size == (char*)block) {
                // expand current to include block
                current->size = current->size + MBLOCK_SIZE + block->size;

                delete_block(block);

                has_merged = 1;
            }

            // no merge possible, block is surrounded by allocated blocks;
            if (has_merged == 0) {
                block->next = current->next;
                current->next = block;
            }

            return;
        }

        current = current->next;
    }

    // if we reached this point, then block is after head, but head has no next
    block->next = NULL;
    head->next = block;
}
