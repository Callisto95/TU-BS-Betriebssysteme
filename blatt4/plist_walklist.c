#include <stdlib.h>
#include "plist.h"

void walkList(list *list, int (*callback) (pid_t, const char *) ) {
	list_element* currentElement = list->head;
    while (currentElement != NULL) {
        if (callback(currentElement->pid, currentElement->cmdLine) != 0) {
            return;
        }
        
        currentElement = currentElement->next;
    }
}
