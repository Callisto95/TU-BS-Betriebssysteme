#include <stdio.h>
#include <stdlib.h>

typedef struct list_element {
	int value;
	struct list_element* next;
} list_element_t;

typedef struct list {
	struct list_element* head;
} list_t;

/**
 * @brief Allocates and initializes a new list_t.
 */
list_t* allocate_and_initialize_list(void) {
	list_t* list = malloc(sizeof(list_t));

	if (list) {
		list->head = NULL;
	}

	return list;
}

/**
 * @brief Allocates and initializes a new list_element_t.
 *
 * @details When parent is not NULL, set the new element as the parents next element.
 *
 * @param value The value of the element.
 * @param parent The optional parent of the element. Can be NULL.
 */
list_element_t* allocate_and_initialize_element(const int value, list_element_t* parent) {
	list_element_t* new_element = malloc(sizeof(list_element_t));

	if (!new_element) {
		perror("cannot continue: malloc error");
		return NULL;
	}

	new_element->value = value;
	new_element->next = NULL;

	if (parent) {
		parent->next = new_element;
	}

	return new_element;
}

/**
 * @brief Append the value to the end of the list. No negative or duplicate entries are allowed.
 *
 * @details Appends the value at the end of the given list. If the value is negative or already in the list return -1.
 * Otherwise, the value itself is returned.
 *
 * @param list The list to add the value to.
 * @param value The value to add. Can't be negative or a duplicate.
 * @return Parameter value or -1 if it couldn't be added.
 */
int list_append(list_t* list, const int value) {
	if (value < 0) {
		return -1;
	}

	if (!list->head) {
		list_element_t* new_element = allocate_and_initialize_element(value, NULL);

		if (!new_element) {
			return -1;
		}

		list->head = new_element;
		return value;
	}

	list_element_t* current_element = list->head;
	list_element_t* last_element = NULL;

	do {
		if (current_element->value == value) {
			return -1;
		}

		last_element = current_element;
		current_element = current_element->next;
	} while (current_element);

	if (!allocate_and_initialize_element(value, last_element)) {
		return -1;
	}

	return value;
}

/**
 * @brief Remove and return the first value from the list.
 *
 * @return The first value or -1 if the list is empty.
 */
int list_pop(list_t* list) {
	if (!list->head) {
		return -1;
	}

	const int value = list->head->value;

	list_element_t* old_initial = list->head;
	list->head = old_initial->next;
	free(old_initial);

	return value;
}

int main(void) {
	list_t* list = allocate_and_initialize_list();

	if (!list) {
		exit(EXIT_SUCCESS);
	}

	printf("insert 47: %d\n", list_append(list, 47));
	printf("insert 11: %d\n", list_append(list, 11));
	printf("insert 23: %d\n", list_append(list, 23));
	printf("insert 11: %d\n", list_append(list, 11));

	printf("remove: %d\n", list_pop(list));
	printf("remove: %d\n", list_pop(list));
	exit(EXIT_SUCCESS);
}
