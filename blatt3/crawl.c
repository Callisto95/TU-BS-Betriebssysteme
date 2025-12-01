// TODO: includes, global vars, etc

// TODO: implement helper functions: makes constraint checking easier

#define _DEFAULT_SOURCE

#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "argumentParser.h"

int isValidPath(const char* path) {
    struct stat status;
    stat(path, &status);

    if (S_ISDIR(status.st_mode) || S_ISREG(status.st_mode)) {
        return 1;
    }

    return 0;
}

static void crawl(char* path, const int maxDepth, const char pattern[], const char type, const int sizeMode,
                  const off_t size, regex_t* line_regex) {
    // SIZE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -size option
    // NAME_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -name option
    // MAXDEPTH_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -maxdepth option
    // TYPE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -type option
    // LINE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -line option

    DIR* directory = opendir(path);

    if (directory == NULL) {
        return;
    }

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

        // printf("%s | %s\n", newPath, current_entry->d_name);
        printf("%s\n", newPath);
        crawl(newPath, maxDepth - 1, pattern, type, sizeMode, size, line_regex);
    }

    closedir(directory);
}

// int main(const int argc, char* argv[]) {
//     argv++;
//     char** args = argv;
//     // initArgumentParser(argc, argv);
//
//     // char **args = buildArgv("command", NULL);
//     // char **args = buildArgv("command", "arg", NULL);
//
//     // this creates read-only memory
//     // char* args[] = {"command", "arg", "-name=value", NULL};
//     printf("init: %d\n", initArgumentParser(argc - 1, args));
//     // printf("init: %d\n", initArgumentParser(sizeof(args) / sizeof(args[0]), args));
//
//     // char* f_argv[] = {"command", "ok"};
//     // printf("%d\n", initArgumentParser(sizeof(f_argv) / sizeof(f_argv[0]), f_argv));
//     printf("command:  %s\n", getCommand());
//     printf("argCount: %d\n", getNumberOfArguments());
//     printf("0arg:     %s\n", getArgument(0));
//     printf("1arg:     %s\n", getArgument(1));
//     printf("0arg:     %s\n", getArgument(2));
//     printf("EValue:   %s\n", getValueForOption("empty"));
//     printf("OValue:   %s\n", getValueForOption("option"));
//     // TODO: invoke crawl() with args on all given paths
// }

int main(int argc, char* argv[]) {
    argc--;
    argv++;
    printf("parser init: %d\n", initArgumentParser(argc, argv));

    int i = 0;
    char* current_directory;
    while ((current_directory = getArgument(i)) != NULL) {
        crawl(current_directory, 0, "", 'd', 1, 2, NULL);
        i++;
    }
}
