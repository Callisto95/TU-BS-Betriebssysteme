#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "halde.h"

extern char* memory;

#define k64 (1 << 16)

int main(void) {
    // merge without loosing some blocks
    char* p1 = halde_malloc((1 << 16));
    char* p2 = halde_malloc((1 << 16));
    char* p3 = halde_malloc((1 << 16));
    char* p4 = halde_malloc((1 << 16));
    char* p5 = halde_malloc((1 << 16));
    char* p6 = halde_malloc((1 << 16));
    char* p7 = halde_malloc((1 << 16));
    char* p8 = halde_malloc((1 << 16));
    char* p9 = halde_malloc((1 << 16));
    char* p10 = halde_malloc((1 << 16));

    halde_free(p4);
    halde_free(p8);
    halde_free(p2);
    halde_free(p6);
    halde_free(p10);

    halde_print();

    char* p2_again = halde_malloc((1 << 16) * 1);
    assert(p2_again && "Malloc should be possible");
    assert(p2_again == p2 && "Expected to get the same memory region again");

    halde_print();

    char* p4_again = halde_malloc((1 << 16) * 1);
    assert(p4_again && "Malloc should be possible");
    assert(p4_again == p4 && "Expected to get the same memory region again");

    char* p6_again = halde_malloc((1 << 16) * 1);
    assert(p6_again && "Malloc should be possible");
    assert(p6_again == p6 && "Expected to get the same memory region again");

    char* p8_again = halde_malloc((1 << 16) * 1);
    assert(p8_again && "Malloc should be possible");
    assert(p8_again == p8 && "Expected to get the same memory region again");

    char* p10_again = halde_malloc((1 << 16) * 1);
    assert(p10_again && "Malloc should be possible");
    assert(p10_again == p10 && "Expected to get the same memory region again");

    halde_free(p4_again);
    halde_free(p8_again);
    halde_free(p2_again);
    halde_free(p6_again);
    halde_free(p10_again);


    halde_free(p1);
    halde_free(p3);
    halde_free(p5);
    halde_free(p7);
    halde_free(p9);
    return 0;
}
