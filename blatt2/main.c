#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "halde.h"

extern char* memory;

#define k64 (1 << 16)

int main(int argc, char* argv[]) {
	// fprintf(stderr, "1\n");
	// halde_print();

	char* p1 = halde_malloc(16);
	char* p2 = halde_malloc(16);
	char* p3 = halde_malloc(16);

	fprintf(stderr, "no free blocks\n");
	halde_print();

	halde_free(p2);

	fprintf(stderr, "single free block\n");
	halde_print();

	halde_malloc(k64);

	fprintf(stderr, "single free block, end is smaller\n");
	halde_print();

	halde_free(p3);
	halde_free(p1);

	fprintf(stderr, "two free blocks\n");
	halde_print();

	return 0;
}
