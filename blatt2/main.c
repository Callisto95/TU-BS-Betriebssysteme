#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "halde.h"

extern char* memory;
int main(int argc, char *argv[]) {
	halde_print();
	fprintf(stderr, "\n");

	char* m1 = halde_malloc(1024*1024 - 16);
	printf("m1: is NULL %d\n", m1 == NULL);
	printf("%d\n", (void*)((char*)m1 + (1024*1024 - 16)) <= (void*)(memory + 1024 * 1024));
	printf("mem-addr: 0x%08zx\n", (uintptr_t)memory);
	printf("m1 -addr: 0x%08zx\n", (uintptr_t) m1);

	halde_print();
	fprintf(stderr, "\n");
	char* m2 = halde_malloc(10);
	printf("m2: is NULL %d\n", m2 == NULL);
	printf("%d\n", (void*)((char*)m2 + (1024*1024 - 16)) <= (void*)(memory + 1024 * 1024));

	halde_print();
	fprintf(stderr, "\n");

	halde_free(m1);
	halde_free(m2);
	halde_print();

	exit(EXIT_SUCCESS);
}
