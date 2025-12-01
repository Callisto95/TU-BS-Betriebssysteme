// TODO: includes, global vars, etc

// TODO: implement helper functions: makes constraint checking easier

#define _DEFAULT_SOURCE

#include <bsd/stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "argumentParser.h"

int isValidPath(const char* path) {
    struct stat status;
    stat(path, &status);

    // printf("%s: dir: %d, reg: %d\n", path, S_ISDIR(status.st_mode), S_ISREG(status.st_mode));

    if (S_ISDIR(status.st_mode) || S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

int isDir(const char* path) {
    struct stat status;
    stat(path, &status);

    if (S_ISDIR(status.st_mode)) {
        return 1;
    }

    return 0;
}

int isFile(const char* path) {
    struct stat status;
    stat(path, &status);

    if (S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

int checkFile(char* file, const char pattern[], const int sizeMode, const off_t size, regex_t* line_regex) {
    return 1;
}

int checkDirectory(char* directory, const char pattern[], const int sizeMode, const off_t size, regex_t* line_regex) {
    return 1;
}

static void crawl(char* path, const int maxDepth, const char pattern[], const char type, const int sizeMode,
                  const off_t size, regex_t* line_regex) {
    // SIZE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -size option
    // NAME_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -name option
    // TYPE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -type option
    // LINE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -line option

    if (maxDepth < 0) {
        return;
    }

    if (!isValidPath(path)) {
        return;
    }

    if (isFile(path)) {
        if (checkFile(path, pattern, sizeMode, size, line_regex)) {
            printf("%s\n", path);
        }
        return;
    }

    // path must be a dir from now on

    if (!checkDirectory(path, pattern, sizeMode, size, line_regex)) {
        return;
    }

    printf("%s\n", path);

    DIR* directory = opendir(path);
    struct dirent* current_entry;

    while ((current_entry = readdir(directory)) != NULL) {
        // filter out unneeded entries
        if (stringsEqual(current_entry->d_name, ".") || stringsEqual(current_entry->d_name, "..")) {
            continue;
        }

        char newPath[strlen(path) + strlen(current_entry->d_name) + 1];
        snprintf(newPath, PATH_MAX, "%s/%s", path, current_entry->d_name);

        if (!isValidPath(newPath)) {
            continue;
        }

        crawl(newPath, maxDepth - 1, pattern, type, sizeMode, size, line_regex);
    }

    closedir(directory);
}

int getMaxDepth(void) {
    char* depthString = getValueForOption("maxdepth");

    if (depthString == NULL) {
        return INT_MAX;
    }

    return strtonum(depthString, 0, INT_MAX, NULL);
}

int main(int argc, char* argv[]) {
    initArgumentParser(argc, argv);

    const int maxDepth = getMaxDepth();

    int i = 0;
    char* current_directory;
    while ((current_directory = getArgument(i)) != NULL) {
        crawl(current_directory, maxDepth, "", 'd', 1, 2, NULL);
        i++;
    }
}
