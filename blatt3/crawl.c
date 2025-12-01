// TODO: includes, global vars, etc

// TODO: implement helper functions: makes constraint checking easier

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

bool checkRegex = false;
bool checkPattern = false;

int isSet(const int value, const int flag) {
    return (value & flag) == flag;
}

int isValidPath(const char* path) {
    struct stat status;
    stat(path, &status);

    if (S_ISDIR(status.st_mode) || S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

int isDir(const char* path) {
    struct stat status;
    lstat(path, &status);

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
    char copy[strlen(name)];
    strcpy(copy, name);

    const char* buf = basename(copy);

    return fnmatch(pattern, buf, 0) == 0;
}

int getFileSize(FILE* file) {
    if (file == NULL) {
        return 0;
    }

    const int currentPosition = ftell(file);

    fseek(file, 0, SEEK_END);

    const int fileSize = ftell(file);

    fseek(file, 0, currentPosition);

    return fileSize;
}

bool matchLines(const char* fileName, FILE* fp, const regex_t* line_regex) {
    char absolutePath[PATH_MAX];
    realpath(fileName, absolutePath);

    char* line = NULL;
    size_t length = 0; // unused, but required

    bool matchFound = false;

    int lineNumber = 0;
    while (getline(&line, &length, fp) != -1) {
        lineNumber++;
        const int result = regexec(line_regex, line, 0, NULL, REG_EXTENDED);

        if (result != 0) {
            continue;
        }

        matchFound = true;

        printf("%s:%d:%s", absolutePath, lineNumber, line);
    }

    return matchFound;
}

int checkFile(const char* file, const char pattern[], const int sizeMode, const off_t size, const regex_t* line_regex) {
    FILE* fp = fopen(file, "r");

    const int fileSize = getFileSize(fp);

    if (checkRegex) {
        matchLines(file, fp, line_regex);
    }

    fclose(fp);

    const bool nameMatches = checkPattern ? matchName(file, pattern) : true;
    const bool sizeMatches = size == 0 || (size >= 0 ? fileSize > size : fileSize < -size);

    return !checkRegex && nameMatches && sizeMatches;
}

static void crawl(char* path, const int maxDepth, const char pattern[], const char type, const int sizeMode,
                  const off_t size, regex_t* line_regex) {
    if (maxDepth < 0) {
        return;
    }

    if (!isValidPath(path)) {
        return;
    }

    if (isFile(path)) {
        if (isSet(type, ONLY_FILE) && checkFile(path, pattern, sizeMode, size, line_regex)) {
            printf("%s\n", path);
        }
        return;
    }

    // path must be a dir from now on

    if (isSet(type, ONLY_DIRECTORY) && matchName(path, pattern) && size == 0) {
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
        return ONLY_DIRECTORY;
    case 'f':
        return ONLY_FILE;
    default:
        return BOTH;
    }
}

char* getName(void) {
    char* name = getValueForOption("name");

    if (name == NULL) {
        return "*";
    }

    checkPattern = true;

    return name;
}

int getSize(void) {
    const char* sizeString = getValueForOption("size");

    if (sizeString == NULL) {
        return 0;
    }

    return strtol(sizeString, NULL, 10);
}

char* getLine(void) {
    char* line = getValueForOption("line");

    if (line == NULL) {
        return ".";
    }

    checkRegex = true;

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
    const int regexError = regcomp(&linePattern, line, 0);

    if (regexError != 0) {
        fprintf(stderr, "invalid regex: code %d\n", regexError);
    }

    if (size != 0 || checkRegex || checkPattern) {
        type = ONLY_FILE;
    }

    int i = 0;
    char* current_directory;
    while ((current_directory = getArgument(i)) != NULL) {
        crawl(current_directory, maxDepth, pattern, type, 0, size, &linePattern);
        i++;
    }
}
