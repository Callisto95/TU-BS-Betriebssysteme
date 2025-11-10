#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "halde.h"

extern char* memory;
// int main(int argc, char *argv[]) {
// 	fprintf(stderr, "0\n");
// 	halde_print();
//
// 	// char* m1 = halde_malloc(10);
// 	char* m1 = halde_malloc(100);
// 	printf("m1: is NULL %d\n", m1 == NULL);
// 	// printf("%d\n", (void*)((char*)m1 + (1024*1024 - 16)) <= (void*)(memory + 1024 * 1024));
// 	printf("mem-addr: %p\n", memory);
// 	printf("m1 -addr: %p\n", m1);
//
// 	fprintf(stderr, "1\n");
// 	halde_print();
// 	char* m2 = halde_malloc(100);
// 	printf("m2: is NULL %d\n", m2 == NULL);
// 	// printf("%d\n", (void*)((char*)m2 + (1024*1024 - 16)) <= (void*)(memory + 1024 * 1024));
// 	printf("mem-addr: %p\n", memory);
// 	printf("m2 -addr: %p\n", m2);
//
// 	printf("m2should: %p\n", m1 + 10 + 16);
// 	printf("delta: %lu\n", m2 - m1);
//
// 	fprintf(stderr, "2\n");
// 	halde_print();
//
// 	halde_free(m1);
// 	fprintf(stderr, "3\n");
// 	halde_print();
//
// 	halde_free(m2);
// 	fprintf(stderr, "4\n");
// 	halde_print();
//
// 	exit(EXIT_SUCCESS);
// }

#define k64 (1 << 16)

int main(int argc, char* argv[]) {
	// fprintf(stderr, "1\n");
	// halde_print();

	/*
	// merge without loosing some blocks
	char *p1 = halde_malloc((1<<16));
	char *p2 = halde_malloc((1<<16));
	char *p3 = halde_malloc((1<<16));
	char *p4 = halde_malloc((1<<16));
	char *p5 = halde_malloc((1<<16));
	char *p6 = halde_malloc((1<<16));
	char *p7 = halde_malloc((1<<16));
	char *p8 = halde_malloc((1<<16));
	char *p9 = halde_malloc((1<<16));
	char *p10 = halde_malloc((1<<16));

	fprintf(stderr, "1\n");
	halde_print();

	halde_free(p4);
	halde_free(p8);
	halde_free(p2);
	halde_free(p6);
	// when freeing something at end of list, bad things happen
	halde_free(p10);

	fprintf(stderr, "2\n");
	halde_print();

	char *p2_again = halde_malloc((1 << 16) * 1);
	assert (p2_again && "Malloc should be possible");
	assert (p2_again == p2 && "Expected to get the same memory region again");

	fprintf(stderr, "3\n");
	halde_print();

	char *p4_again = halde_malloc((1 << 16) * 1);
	assert (p4_again && "Malloc should be possible");
	assert (p4_again == p4 && "Expected to get the same memory region again");

	fprintf(stderr, "4\n");
	halde_print();

	char *p6_again = halde_malloc((1 << 16) * 1);
	assert (p6_again && "Malloc should be possible");
	assert (p6_again == p6 && "Expected to get the same memory region again");

	fprintf(stderr, "5\n");
	halde_print();

	char *p8_again = halde_malloc((1 << 16) * 1);
	assert (p8_again && "Malloc should be possible");
	assert (p8_again == p8 && "Expected to get the same memory region again");

	fprintf(stderr, "6\n");
	halde_print();

	char *p10_again = halde_malloc((1 << 16) * 1);
	assert (p10_again && "Malloc should be possible");
	assert (p10_again == p10 && "Expected to get the same memory region again");

	fprintf(stderr, "7\n");
	halde_print();
	*/

	return 0;
}
