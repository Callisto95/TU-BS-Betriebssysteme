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

sem_t pushUpdate;
sem_t counterLock;
sem_t workerLock;
bool exitApplication = false;
int workerCount;

// atomic_int boundaryPoints = ATOMIC_VAR_INIT(0);
// atomic_int interiorPoints = ATOMIC_VAR_INIT(0);
// atomic_int finishedWorkers = ATOMIC_VAR_INIT(0);
int boundaryPoints = 0;
int interiorPoints = 0;
int finishedWorkers = 0;

void finalizePoints(int boundary, int interior) {
    sem_wait(&counterLock);
    boundaryPoints += boundary;
    interiorPoints += interior;
    sem_post(&counterLock);
}

// pthread_start can only take one argument, but countPoints needs two
// wrapper for countPoints
void* threadWrapper(void* triangle) {
    sem_wait(&workerLock);

    countPoints(triangle, finalizePoints);
    free(triangle);

    sem_wait(&counterLock);
    finishedWorkers += 1;
    sem_post(&counterLock);

    sem_post(&workerLock);

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

    const int scannedVars = sscanf(line, "(%d,%d),(%d,%d),(%d,%d)", &x1, &y1, &x2, &y2, &x3, &y3);

    if (scannedVars != 6) {
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
    bool doExit = false;
    while (!doExit) {
        sem_wait(&pushUpdate);
        sem_wait(&counterLock);

        doExit = exitApplication;

        int activeWorkers;
        sem_getvalue(&workerLock, &activeWorkers);

        const int readInteriorPoints = interiorPoints;
        const int readBoundaryPoints = boundaryPoints;
        const int readActiveThreads = workerCount - activeWorkers;
        const int readFinishedWorkers = finishedWorkers;

        sem_post(&counterLock);

        printf("\rFound %d boundary and %d interior points, %d active threads, %d finished threads", readBoundaryPoints,
               readInteriorPoints, readActiveThreads, readFinishedWorkers);
        fflush(stdout);
    }

    return NULL;
}

pthread_t startOutputThread(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, outputStatus, NULL);
    return thread;
}

int main(const int argc, const char* argv[]) {
    sem_init(&pushUpdate, 0, 0);
    sem_init(&counterLock, 0, 1);

    workerCount = parseWorkerCount(argc, argv);

    sem_init(&workerLock, 0, workerCount);

    const pthread_t outputThread = startOutputThread();

    int readLines = 0;

    while (true) {
        char* readLine = NULL;
        size_t length = 0;
        if (getline(&readLine, &length, stdin) == -1) {
            free(readLine);

            // make sure everything is finished and output is done
            int rFinishedWorkers;
            do {
                sem_wait(&counterLock);
                rFinishedWorkers = finishedWorkers;
                sem_post(&counterLock);
            } while (rFinishedWorkers != readLines);

            sem_wait(&counterLock);
            exitApplication = true;
            sem_post(&counterLock);

            sem_post(&pushUpdate);
            pthread_join(outputThread, NULL);

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

        pthread_t thread;
        pthread_attr_t attributes;

        pthread_attr_init(&attributes);
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&thread, &attributes, threadWrapper, triangle) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        readLines++;
        free(readLine);
    }

    return 1;
}
