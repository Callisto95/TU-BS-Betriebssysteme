	// TODO: includes, global vars, etc

	// TODO: implement helper functions: makes constraint checking easier

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "argumentParser.h"

// static void crawl(char path[], int maxDepth, const char pattern[], char type,
//                   int sizeMode, off_t size, regex_t * line_regex) {
	// TODO: implement body
	// CRAWL_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl related testcases
	// SIZE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -size option
	// NAME_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -name option
	// MAXDEPTH_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -maxdepth option
	// TYPE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -type option
	// LINE_NOT_IMPLEMENTED_MARKER: remove this line to activate crawl testcases using -line option
// }

// static void die(const char *msg) {
//     perror(msg);
//     exit(EXIT_FAILURE);
// }
//
// static char **addArg(char **argv, size_t i, const char *str) {
//     argv = realloc(argv, (i + 2) * sizeof(*argv));
//     if (argv == NULL)
//         die("realloc");
//     argv[i] = strdup(str);
//     if (argv[i] == NULL)
//         die("strdup");
//     return argv;
// }
//
// static char  __attribute__ ((unused)) **buildArgv(const char str0[], ...) {
//     if (str0 == NULL) {
//         fputs("argv must have at least one element\n", stderr);
//         exit(EXIT_FAILURE);
//     }
//     size_t i = 0;
//     char **argv = addArg(NULL, i++, str0);
//     va_list strings;
//     va_start(strings, str0);
//     for (;;) {
//         const char *str = va_arg(strings, const char *);
//         if (str == NULL)
//             break;
//         argv = addArg(argv, i++, str);
//     }
//     va_end(strings);
//     argv[i] = NULL;
//     return argv;
// }

int main(const int argc, char* argv[]) {
    argv++;
    char** args = argv;
    // initArgumentParser(argc, argv);

    // char **args = buildArgv("command", NULL);
    // char **args = buildArgv("command", "arg", NULL);

    // this creates read-only memory
    // char* args[] = {"command", "arg", "-name=value", NULL};
    printf("init: %d\n", initArgumentParser(argc - 1, args));
    // printf("init: %d\n", initArgumentParser(sizeof(args) / sizeof(args[0]), args));

    // char* f_argv[] = {"command", "ok"};
    // printf("%d\n", initArgumentParser(sizeof(f_argv) / sizeof(f_argv[0]), f_argv));
    printf("command:  %s\n", getCommand());
    printf("argCount: %d\n", getNumberOfArguments());
    printf("0arg:     %s\n", getArgument(0));
    printf("1arg:     %s\n", getArgument(1));
    printf("0arg:     %s\n", getArgument(2));
    printf("EValue:   %s\n", getValueForOption("empty"));
    printf("OValue:   %s\n", getValueForOption("option"));
    // TODO: invoke crawl() with args on all given paths
}
