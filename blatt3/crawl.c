// TODO: includes, global vars, etc

// TODO: implement helper functions: makes constraint checking easier

#define _DEFAULT_SOURCE

#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fnmatch.h>

#include "argumentParser.h"

#define FILE 1
#define DIRECTORY 2
#define BOTH FILE | DIRECTORY

int isSet(const int value, const int flag) {
    return (value & flag) == flag;
}

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

int matchName(const char* name, const char* pattern) {
    return fnmatch(pattern, name, 0) == 0;
}

int checkFile(const char* file, const char pattern[], const int sizeMode, const off_t size, regex_t* line_regex) {
    return matchName(file, pattern);
}

static void crawl(char* path, const int maxDepth, const char pattern[], const char type, const int sizeMode,
                  const off_t size, regex_t* line_regex) {
    // SIZE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -size option
    // LINE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -line option

    if (maxDepth < 0) {
        return;
    }

    if (!isValidPath(path)) {
        return;
    }

    if (isFile(path)) {
        if (isSet(type, FILE) && checkFile(path, pattern, sizeMode, size, line_regex)) {
            printf("%s\n", path);
        }
        return;
    }

    // path must be a dir from now on

    if (isSet(type, DIRECTORY) && matchName(path, pattern)) {
        printf("%s\n", path);
    }

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
    const char* depthString = getValueForOption("maxdepth");

    if (depthString == NULL) {
        return INT_MAX;
    }

    const long int result = strtol(depthString, NULL, 10);

    return result < 0 ? 0 : result;
}

int getType(void) {
    const char* typeString = getValueForOption("type");

    if (typeString == NULL || strlen(typeString) != 1) {
        return BOTH;
    }

    switch (typeString[0]) {
    case 'd':
        return DIRECTORY;
    case 'f':
        return FILE;
    default:
        return BOTH;
    }
}

char* getName(void) {
    char* name = getValueForOption("name");

    return name == NULL ? "*" : name;
}

int main(const int argc, char* argv[]) {
    initArgumentParser(argc, argv);

    const int maxDepth = getMaxDepth();
    const int type = getType();
    const char* pattern = getName();

    int i = 0;
    char* current_directory;
    while ((current_directory = getArgument(i)) != NULL) {
        crawl(current_directory, maxDepth, pattern, type, 1, 2, NULL);
        i++;
    }
}
