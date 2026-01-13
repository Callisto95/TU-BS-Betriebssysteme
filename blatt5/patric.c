#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "triangle.h"

// start: --------------- lilo.c ---------------
struct tri_list_element {
    struct triangle* value;
    struct tri_list_element* next;
};

struct tri_list {
    struct tri_list_element* head;
};

struct tri_list_element* tri_allocate_and_initialize_element(struct triangle* value, struct tri_list_element* parent) {
    struct tri_list_element* new_element = malloc(sizeof(struct tri_list_element));

    if (!new_element) {
        perror("cannot continue: malloc error");
        exit(EXIT_FAILURE);
    }

    new_element->value = value;
    new_element->next = NULL;

    if (parent) {
        parent->next = new_element;
    }

    return new_element;
}

bool tri_list_append(struct tri_list* list, struct triangle* value) {
    if (list->head == NULL) {
        struct tri_list_element* new_element = tri_allocate_and_initialize_element(value, NULL);

        list->head = new_element;
        return value;
    }

    struct tri_list_element* current_element = list->head;
    struct tri_list_element* last_element = NULL;

    do {
        if (current_element->value == value) {
            return -1;
        }

        last_element = current_element;
        current_element = current_element->next;
    } while (current_element);

    tri_allocate_and_initialize_element(value, last_element);

    return value;
}

struct triangle* tri_list_pop(struct tri_list* list) {
    if (!list->head) {
        return NULL;
    }

    struct triangle* value = list->head->value;

    struct tri_list_element* old_initial = list->head;
    list->head = old_initial->next;
    free(old_initial);

    return value;
}
// end  : --------------- lilo.c ---------------

sem_t pushUpdate;
sem_t counterLock;

pthread_t* workers;
int workerCount;

// start: --------------- worker list ---------------
void insertWorker(const pthread_t worker) {
    bool couldInsert = false;

    for (int i = 0; i < workerCount; i++) {
        // printf("iW: \n%lu\n", workers[i]);
        
        if (workers[i] == 0) {
            workers[i] = worker;
            couldInsert = true;
            break;
        }
    }

    if (!couldInsert) {
        fprintf(stderr, "could not insert worker into list!");
    }
}

void deleteWorker(const pthread_t worker) {
    // keep list sorted
    // end of list is always 0 (if not filled)
    int moveStart = -1;

    for (int i = 0; i < workerCount; i++) {
        if (workers[i] == worker) {
            moveStart = i;
            break;
        }
    }

    if (moveStart == -1) {
        fprintf(stderr, "worker is not in list!");
        return;
    }

    for (int x = moveStart; x < workerCount; x++) {
        if (x + 1 == workerCount) {
            workers[x] = 0;
        } else {
            workers[x] = workers[x + 1];
        }
    }
}

bool allWorkersUsed(void) {
    return workers[workerCount - 1] != 0;
}

int countActiveWorkers(void) {
    for (int i = 0; i < workerCount; i++) {
        // printf("cAW: \n%lu\n", workers[i]);
        
        if (workers[i] == 0) {
            return i;
        }
    }

    return workerCount;
}

// end  : --------------- worker list ---------------

// atomic_int boundaryPoints = ATOMIC_VAR_INIT(0);
// atomic_int interiorPoints = ATOMIC_VAR_INIT(0);
// atomic_int finishedWorkers = ATOMIC_VAR_INIT(0);
int boundaryPoints = 0;
int interiorPoints = 0;
int finishedWorkers = 0;

void finalizePoints(int boundary, int interior) {
    // atomic_fetch_add(&boundaryPoints, boundary);
    // atomic_fetch_add(&interiorPoints, interior);
    sem_wait(&counterLock);
    boundaryPoints += boundary;
    interiorPoints += interior;
    sem_post(&counterLock);
}

// pthread_start can only take one argument, but countPoints needs two
// wrapper for countPoints
void* countWrapper(void* triangle) {
    countPoints(triangle, finalizePoints);
    free(triangle);
    sem_wait(&counterLock);
    finishedWorkers += 1;
    deleteWorker(pthread_self());
    sem_post(&counterLock);
    sem_post(&pushUpdate);
    return NULL;
}

int parseWorkerCount(const int argc, const char* argv[]) {
    if (argc == 1) {
        fprintf(stderr, "no worker count given\n");
        exit(EXIT_FAILURE);
    }
    const int count = (int)strtol(argv[1], NULL, 10);

    if (errno == ERANGE) {
        perror("worker value is out of range!\n");
        exit(EXIT_FAILURE);
    }

    if (count < 0) {
        fprintf(stderr, "cannot use a negative number of worker threads\n");
        exit(EXIT_FAILURE);
    }

    return count;
}

struct triangle* getTriangle(const char* line) {
    int x1, x2, x3 = INT_MIN;
    int y1, y2, y3 = INT_MIN;

    sscanf(line, "(%d,%d),(%d,%d),(%d,%d)", &x1, &y1, &x2, &y2, &x3, &y3);

    if (x1 == INT_MIN || x2 == INT_MIN || x3 == INT_MIN || y1 == INT_MIN || y2 == INT_MIN || y3 == INT_MIN) {
        return NULL;
    }

    struct triangle* triangle = calloc(1, sizeof(struct triangle));
    triangle->point[0].x = x1;
    triangle->point[0].y = y1;
    triangle->point[1].x = x2;
    triangle->point[1].y = y2;
    triangle->point[2].x = x3;
    triangle->point[2].y = y3;

    return triangle;
}

void* outputStatus(void* _) {
    while (true) {
        sem_wait(&pushUpdate);
        printf("\rFound %d boundary and %d interior points, %d active threads, %d finished threads", boundaryPoints,
               interiorPoints, countActiveWorkers(), finishedWorkers);
    }
}

void startOutputThread(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, outputStatus, NULL);
}

int main(const int argc, const char* argv[]) {
    sem_init(&pushUpdate, false, 0);
    sem_init(&counterLock, false, 1);

    workerCount = parseWorkerCount(argc, argv);
    workers = calloc(workerCount, sizeof(pthread_t));

    if (workers == NULL) {
        perror("allocation failed, cannot continue");
        exit(EXIT_FAILURE);
    }

    struct tri_list tri_buffer;
    tri_buffer.head = NULL;

    startOutputThread();

    while (true) {
        char* readLine = NULL;
        size_t length = 0;
        if (getline(&readLine, &length, stdin) == -1) {
            free(readLine);
            exit(EXIT_SUCCESS);
        }

        if (strlen(readLine) > sysconf(_SC_LINE_MAX)) {
            continue;
        }

        struct triangle* triangle = getTriangle(readLine);

        if (triangle == NULL) {
            fprintf(stderr, "invalid tri format\n");
            continue;
        }

        tri_list_append(&tri_buffer, triangle);

        struct triangle* current_tri;
        while (!allWorkersUsed() && (current_tri = tri_list_pop(&tri_buffer)) != NULL) {
            pthread_t thread;
            pthread_attr_t attributes;

            pthread_attr_init(&attributes);
            pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
            if (pthread_create(&thread, &attributes, countWrapper, current_tri) != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
            
            sem_wait(&counterLock);
            insertWorker(thread);
            sem_post(&counterLock);
        }
    }

    // TODO: remove marker
    // ACTIVATE_MEDIUM_TESTCASES_NOT_IMPLEMENTED_MARKER: remove this line to enable medium size testdata
    // ACTIVATE_LARGE_TESTCASES_NOT_IMPLEMENTED_MARKER: remove this line to enable large size testdata
    // ACTIVATE_DYNAMIC_TESTCASES_NOT_IMPLEMENTED_MARKER: remove this line to enable dynamically generated testdata
    // MULTITHREADING_NOT_IMPLEMENTED_MARKER: remove this line to activate testcases which expect multiple worker
    // threads
    return 1;
}
