#define _DEFAULT_SOURCE

#include <dirent.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "argumentParser.h"

#define ONLY_FILE 1
#define ONLY_DIRECTORY 2
#define BOTH ONLY_FILE | ONLY_DIRECTORY

static bool checkLineRegex = false;
static bool checkNamePattern = false;

static int isSet(const int value, const int flag) {
    return (value & flag) == flag;
}

static int isValidPath(const char* path) {
    struct stat status;
    lstat(path, &status);

    if (S_ISDIR(status.st_mode) || S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

static int isFile(const char* path) {
    struct stat status;
    stat(path, &status);

    if (S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

static int matchName(const char* name, const char* pattern) {
    char copy[strlen(name)];
    strcpy(copy, name);

    const char* buf = basename(copy);

    return fnmatch(pattern, buf, 0) == 0;
}

static int getFileSize(FILE* file) {
    if (file == NULL) {
        return 0;
    }

    const int currentPosition = ftell(file);

    fseek(file, 0, SEEK_END);

    const int fileSize = ftell(file);

    fseek(file, 0, currentPosition);

    return fileSize;
}

static bool matchLines(const char* fileName, FILE* fp, const regex_t* line_regex) {
    char absolutePath[PATH_MAX];
    realpath(fileName, absolutePath);

    char* line = NULL;
    size_t length = 0; // unused, but required

    bool matchFound = false;

    int lineNumber = 0;
    while (getline(&line, &length, fp) != -1) {
        lineNumber++;
        const int result = regexec(line_regex, line, 0, NULL, 0);

        if (result != 0) {
            continue;
        }

        matchFound = true;

        printf("%s:%d:%s", absolutePath, lineNumber, line);
    }

    return matchFound;
}

static int checkFile(const char* file, const char pattern[], const off_t size, const regex_t* line_regex) {
    FILE* fp = fopen(file, "r");

    const int fileSize = getFileSize(fp);

    const bool nameMatches = checkNamePattern ? matchName(file, pattern) : true;
    const bool sizeMatches = size == 0 || (size >= 0 ? fileSize > size : fileSize < -size);

    if (checkLineRegex && nameMatches && sizeMatches) {
        matchLines(file, fp, line_regex);
    }

    fclose(fp);

    return !checkLineRegex && nameMatches && sizeMatches;
}

static void crawl(char* path, const int maxDepth, const char pattern[], const char type, const int _, const off_t size,
                  regex_t* line_regex) {
    if (maxDepth < 0) {
        return;
    }

    if (!isValidPath(path)) {
        return;
    }

    if (isFile(path)) {
        if (isSet(type, ONLY_FILE) && checkFile(path, pattern, size, line_regex)) {
            printf("%s\n", path);
        }
        return;
    }

    // path must be a dir from now on

    if (isSet(type, ONLY_DIRECTORY)) {
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

        crawl(newPath, maxDepth - 1, pattern, type, 0, size, line_regex);
    }

    closedir(directory);
}

static int getMaxDepth(void) {
    const char* depthString = getValueForOption("maxdepth");

    if (depthString == NULL) {
        return INT_MAX;
    }

    const long int result = strtol(depthString, NULL, 10);

    return result < 0 ? 0 : result;
}

static int getType(void) {
    const char* typeString = getValueForOption("type");

    if (typeString == NULL || strlen(typeString) != 1) {
        return BOTH;
    }

    switch (typeString[0]) {
    case 'd':
        return ONLY_DIRECTORY;
    case 'f':
        return ONLY_FILE;
    default:
        return BOTH;
    }
}

static char* getName(void) {
    char* name = getValueForOption("name");

    if (name == NULL) {
        return "*";
    }

    checkNamePattern = true;

    return name;
}

static int getSize(void) {
    const char* sizeString = getValueForOption("size");

    if (sizeString == NULL) {
        return 0;
    }

    return strtol(sizeString, NULL, 10);
}

static char* getLine(void) {
    char* line = getValueForOption("line");

    if (line == NULL) {
        return ".";
    }

    checkLineRegex = true;

    return line;
}

int main(const int argc, char* argv[]) {
    initArgumentParser(argc, argv);

    int type = getType();
    const int maxDepth = getMaxDepth();
    const char* pattern = getName();
    const int size = getSize();
    const char* line = getLine();

    regex_t linePattern;
    const int regexError = regcomp(&linePattern, line, REG_EXTENDED);

    if (regexError != 0) {
        fprintf(stderr, "invalid regex: code %d\n", regexError);
    }

    if (size != 0 || checkLineRegex || checkNamePattern) {
        type = ONLY_FILE;
    }

    int i = 0;
    char* current_directory;
    while ((current_directory = getArgument(i)) != NULL) {
        crawl(current_directory, maxDepth, pattern, type, 0, size, &linePattern);
        i++;
    }

    regfree(&linePattern);
}
