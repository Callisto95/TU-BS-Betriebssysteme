#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "plist.h"

#define MAX_ARGS 48

struct finished_process {
    pid_t pid;
    int status;
    const char* command;
};

char* commandDelimiters = " \t\n";

list backgroundProcesses;
struct finished_process* finishedProcess;

int walk_getFinishedProcess(pid_t pid, const char* cmd) {
    int status = 0;
    if (waitpid(pid, &status, WNOHANG) != 0) {
        finishedProcess = malloc(sizeof(struct finished_process));
        finishedProcess->pid = pid;
        finishedProcess->status = status;
        finishedProcess->command = cmd;
        return -1;
    }

    return 0;
}

int walk_printBackgroundProcesses(pid_t pid, const char* cmd) {
    printf("[%d] %s\n", pid, cmd);
    return 0;
}

bool stringsEqual(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

void printExit(char* argv[], const int argc, const int status) {
    fprintf(stderr, "Exitstatus [");
    for (int i = 0; i < argc; i++) {
        fprintf(stderr, "%s%s", argv[i], i == argc - 1 ? "" : " ");
    }
    fprintf(stderr, "] = %d\n", status);
}

bool handleInternal(char* argv[], int argc, int* status) {
    if (stringsEqual(argv[0], "cd")) {
        const char* path = argv[1];

        if (path != NULL) {
            if (chdir(path) == -1) {
                *status = errno;
                perror("cd");
            }
        }

        return true;
    }

    if (stringsEqual(argv[0], "jobs")) {
        walkList(&backgroundProcesses, walk_printBackgroundProcesses);
        return true;
    }

    return false;
}

bool handleExternal(const char* fullCommand, char* argv[MAX_ARGS], int* argc, int* status) {
    const int pid = fork();
    bool isBackground = false;

    if (stringsEqual(argv[*argc - 1], "&")) {
        isBackground = true;
        insertElement(&backgroundProcesses, pid, fullCommand);
        argv[*argc - 1] = NULL;
        *argc = *argc - 1;
    }

    if (pid == 0 && execvp(argv[0], argv) == -1) {
        const int errorNumber = errno;
        perror("exec");
        *status = errorNumber;
        return false;
    }

    if (pid < 0) {
        perror("fork");
    }

    if (isBackground) {
        return true;
    }

    waitpid(pid, status, 0);
    return false;
}

int main(void) {
    char cwd[PATH_MAX];
    while (true) {
        getcwd(cwd, PATH_MAX);
        fprintf(stderr, "%s: ", cwd);

        char* fullCommand = NULL;
        size_t length = 0;
        if (getline(&fullCommand, &length, stdin) == -1) {
            free(fullCommand);
            return 0;
        }

        if (strlen(fullCommand) > sysconf(_SC_LINE_MAX)) {
            continue;
        }

        const bool onlyShowResults = fullCommand[0] == '\n';

        if (!onlyShowResults) {
            // remove trailing new line
            fullCommand[strlen(fullCommand) - 1] = '\0';

            char commandCopy[strlen(fullCommand)];
            strcpy(commandCopy, fullCommand);

            char* argv[MAX_ARGS];

            // initialize strtok
            char* token = strtok(commandCopy, commandDelimiters);
            argv[0] = token;

            int argc = 1;
            while ((token = strtok(NULL, commandDelimiters)) != NULL && argc < MAX_ARGS - 1) {
                argv[argc++] = token;
            }
            argv[argc] = NULL; // exec needs NULL at the end

            if (argv[0] == NULL) {
                continue;
            }

            int status = 0;

            bool isBackground = false;

            if (handleInternal(argv, argc, &status)) {
                isBackground = true;
            } else {
                isBackground = handleExternal(fullCommand, argv, &argc, &status);
            }

            if (!isBackground) {
                printExit(argv, argc, status);
            }
        }

        walkList(&backgroundProcesses, walk_getFinishedProcess);
        while (finishedProcess != NULL) {
            printf("BackExitstatus [%s] = %d\n", finishedProcess->command, finishedProcess->status);
            char buffer[sysconf(_SC_LINE_MAX)];
            removeElement(&backgroundProcesses, finishedProcess->pid, buffer, sysconf(_SC_LINE_MAX));
            walkList(&backgroundProcesses, walk_getFinishedProcess);
            free(finishedProcess);
            finishedProcess = NULL;
        }

        free(fullCommand);
    }
}


/* TODO: helper implementation */
