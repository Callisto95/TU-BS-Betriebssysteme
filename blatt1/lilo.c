#include <stdio.h>
#include <stdlib.h>

typedef struct list_element {
	// TODO: implement fields
} list_element_t;

typedef struct list {
	// TODO: implement fields
} list_t;


int list_append(list_t *list, int value) {
	// TODO: implement body
	return -1;
}

int list_pop(list_t *list) {
	// TODO: implement body
	return -1;
}

int main (void) {
	list_t list; //TODO: initialize

	printf("insert 47: %d\n", list_append(&list, 47));
	printf("insert 11: %d\n", list_append(&list, 11));
	printf("insert 23: %d\n", list_append(&list, 23));
	printf("insert 11: %d\n", list_append(&list, 11));

	printf("remove: %d\n", list_pop(&list));
	printf("remove: %d\n", list_pop(&list));
	exit(EXIT_SUCCESS);
}
