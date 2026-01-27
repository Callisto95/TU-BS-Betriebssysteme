/*
 * Fehlerbehandlung weggelassen bei:
 * 		- sigaction -> Fehler kann nur durch eigene Argumente verursacht werden.
 * 		- sigemptyset -> "
 * 		- gettimeofday -> " */

/* TODO: implement main */
/* ARGUMENT_NOT_IMPLEMENTED_MARKER remove to activate argument parsing tests */
/* BASICA_NOT_IMPLEMENTED_MARKER remove to activate simple straight forward test runs without SIGQUIT and without
 * testing for correct timestamps */
/* BASICB_NOT_IMPLEMENTED_MARKER remove to activate simple straight forward test runs without SIGQUIT */
/* SIGQUIT_NOT_IMPLEMENTED_MARKER remove to enable testaces which send SIGTERM to stop a race */

#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

sem_t newRound;
volatile sig_atomic_t currentRoundCounter = -1;
volatile sig_atomic_t userPoints = 0;

void ticker_sigUser1(int signum) {
    userPoints += 1;
}

void ticker_sigint(int signum) {
    currentRoundCounter += 1;
    sem_post(&newRound);
}

void ticker_sigquit(int signum) {
    write(STDOUT_FILENO, "race cancelled\n", 15);
    exit(EXIT_SUCCESS);
}

struct timeval getFastestRound(struct timeval roundTimes[], const int maxRounds) {
    struct timeval fastestRound = {.tv_sec = INT64_MAX, .tv_usec = INT64_MAX};

    for (int i = 0; i < maxRounds; i++) {
        const struct timeval currentRound = roundTimes[i];

        if (currentRound.tv_sec < fastestRound.tv_sec) {
            fastestRound = currentRound;
            continue;
        }

        if (currentRound.tv_sec == fastestRound.tv_sec && currentRound.tv_usec < fastestRound.tv_usec) {
            fastestRound = currentRound;
        }
    }

    return fastestRound;
}
void createSignalHandlers(void) {
    struct sigaction intHandler;
    intHandler.sa_handler = ticker_sigint;
    // don't block other signals while handler is running
    // signals would be processed after handler is finished
    // handled signal is automatically deferred
    sigemptyset(&intHandler.sa_mask);
    intHandler.sa_flags = 0;
    sigaction(SIGINT, &intHandler, NULL);
    
    struct sigaction quitHandler;
    quitHandler.sa_handler = ticker_sigquit;
    sigemptyset(&quitHandler.sa_mask);
    quitHandler.sa_flags = 0;
    sigaction(SIGQUIT, &quitHandler, NULL);
    
    struct sigaction user1Handler;
    user1Handler.sa_handler = ticker_sigUser1;
    sigemptyset(&user1Handler.sa_mask);
    user1Handler.sa_flags = 0;
    sigaction(SIGUSR1, &user1Handler, NULL);
}

int main(const int argc, char* argv[]) {
    if (argc != 2) {
        printf("No rounds given or too many arguments.");
        exit(EXIT_FAILURE);
    }

    sem_init(&newRound, 0, 0);
    
    const int maxRounds = (int)strtol(argv[1], NULL, 10);
    struct timeval roundTimes[maxRounds];

    createSignalHandlers();

    printf("ready, awaiting SIGINT (pid: %d)\n", getpid());
    fflush(stdout);

    struct timeval startTime;
    bool isInterrupted = false;
    while (true) {
        // skip check if previous wait was stopped due to SIGINT
        if (!isInterrupted && currentRoundCounter == maxRounds) {
            break;
        }

        isInterrupted = sem_wait(&newRound) == -1;
        
        // if interrupted is discarded, but correct timing is needed
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);

        if (isInterrupted) {
            continue;
        }

        if (currentRoundCounter == 0) {
            gettimeofday(&startTime, NULL);
            printf("starting race\n");
            fflush(stdout);
            continue;
        }
        
        const int currentRoundIndex = currentRoundCounter - 1;

        struct timeval* currentRound = &roundTimes[currentRoundIndex];
        
        struct timeval previousRound;
        if (currentRoundCounter == 1) {
            previousRound = startTime;
        } else {
            timeradd(&startTime, &roundTimes[currentRoundIndex - 1], &previousRound);
        }

        timersub(&currentTime, &previousRound, currentRound);

        printf("lap %03d: 0:%02ld.%04ld\n", currentRoundCounter, currentRound->tv_sec, currentRound->tv_usec);
        fflush(stdout);
    }

    struct timeval endTime;
    gettimeofday(&endTime, NULL);

    struct timeval deltaTime;
    timersub(&endTime, &startTime, &deltaTime);

    printf("sum: 0:%02ld.%04ld\n", deltaTime.tv_sec, deltaTime.tv_usec);

    const struct timeval fastestRound = getFastestRound(roundTimes, maxRounds);
    printf("fastest: 0:%02ld.%04ld\n", fastestRound.tv_sec, fastestRound.tv_usec);
    printf("points: %d\n", userPoints);
}
