/* TODO: includes */

#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "plist.h"

char* commandDelimiters = " \t\n";

bool stringsEqual(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

#define MAX_ARGS 48

/* TODO: helpers (forward decls) for command parsing, job printing, book keeping */

int countDelimiters(const char* string) {
    int count = 0;
    for (int i = 0; i < strlen(string); i++) {
        const char character = string[i];
        if (character == ' ' || character == '\t') {
            count++;
        }
    }

    return count;
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

    return false;
}

int main(void) {
    // TODO: implement me
    // PROMPT_NOT_IMPLEMENTED_MARKER remove this line to enable prompt related testcases
    // CHILD_NOT_IMPLEMENTED_MARKER remove this line to enable testaces which execute commands
    // STATUS_NOT_IMPLEMENTED_MARKER remove this line to enable testcases for the status line after a child terminates
    // CD_NOT_IMPLEMENTED_MARKER remove this line to enable cd related testcases
    // BACKGROUND_NOT_IMPLEMENTED_MARKER remove this line to enable testcases for background tasks
    // JOBS_NOT_IMPLEMENTED_MARKER remove this line to enable cd related testcases
    char cwd[PATH_MAX];
    while (true) {
        getcwd(cwd, PATH_MAX);
        fprintf(stderr, "%s: ", cwd);

        char* fullCommand;
        size_t length = 0;
        if (getline(&fullCommand, &length, stdin) == -1) {
            return 0;
        }


        char* argv[MAX_ARGS];

        // initialize strtok
        char* token = strtok(fullCommand, commandDelimiters);
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

        if (!handleInternal(argv, argc, &status)) {
            const int pid = fork();

            if (pid == 0) {
                if (execvp(argv[0], argv) == -1) {
                    const int errorNumber = errno;
                    perror("exec");
                    return errorNumber;
                }
            }

            if (pid < 0) {
                perror("fork");
            }

            waitpid(pid, &status, 0);
        }


        printf("Exitstatus [");
        for (int i = 0; i < argc; i++) {
            printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
        }
        printf("] = %d\n", status);
    }
}


/* TODO: helper implementation */
