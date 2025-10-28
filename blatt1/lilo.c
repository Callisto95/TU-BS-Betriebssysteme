#include <stdio.h>
#include <stdlib.h>

typedef struct list_element {
	unsigned int value;
	struct list_element* next;
} list_element_t;

typedef struct list {
	struct list_element* initial;
} list_t;

list_element_t* initialize_element(const int value, list_element_t* parent) {
	list_element_t* new_element = malloc(sizeof(list_element_t));

	if (!new_element) {
		perror("cannot continue: malloc error");
		// exit(EXIT_SUCCESS);
		return NULL;
	}

	new_element->value = value;
	new_element->next = NULL;

	if (parent) {
		parent->next = new_element;
	}

	return new_element;
}

int list_append(list_t* list, const int value) {
	if (value < 0) {
		return -1;
	}

	if (!list->initial) {
		list_element_t* new_element = initialize_element(value, NULL);

		if (!new_element) {
			return -1;
		}

		list->initial = new_element;
		return value;
	}

	list_element_t* current_element = list->initial;
	list_element_t* last_element = NULL;

	do {
		if (current_element->value == (unsigned int)value) {
			return -1;
		}

		last_element = current_element;
		current_element = current_element->next;
	} while (current_element);

	if (!initialize_element(value, last_element)) {
		return -1;
	}

	return value;
}

int list_pop(list_t* list) {
	if (!list->initial) {
		return -1;
	}

	const unsigned int value = list->initial->value;

	list_element_t* old_initial = list->initial;
	list->initial = old_initial->next;
	free(old_initial);

	return (int)value;
}

int main(void) {
	list_t list;
	list.initial = NULL;

	printf("insert 47: %d\n", list_append(&list, 47));
	printf("insert 11: %d\n", list_append(&list, 11));
	printf("insert 23: %d\n", list_append(&list, 23));
	printf("insert 11: %d\n", list_append(&list, 11));

	printf("remove: %d\n", list_pop(&list));
	printf("remove: %d\n", list_pop(&list));
	exit(EXIT_SUCCESS);
}
