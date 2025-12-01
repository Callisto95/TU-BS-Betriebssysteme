#include "argumentParser.h"

#include <stdio.h>
#include <string.h>

#define MAXIMUM_ARGUMENTS 50
#define MAXIMUM_OPTIONS 50

struct option {
    char* name;
    char* value;
};

char** arguments;
int optionIndex = -1;
int argumentCount = -1;
int combinedCount = -1;

char* command = NULL;

bool stringsEqual(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

void splitOption(char** fullOption) {
    char* equals = strchr(*fullOption, '=');

    // no equals in string, nothing to split
    // most likely '-empty='
    if (!equals) {
        return;
    }

    // since strings are null terminated, adding a \0 in place of the '=' splits the string
    *equals = '\0';

    // skip leading '-'
    *fullOption += 1;
}

/**
 * Initializes the ArgumentParser by modifying argv.
 *
 * @param argc
 * @param argv
 * @return PARSER_INIT_FAILURE or PARSER_INIT_SUCCESS
 */
int initArgumentParser(const int argc, char* argv[]) {
    if (argc == 0) {
        return PARSER_INIT_FAILURE;
    }

    command = argv[0];
    bool onlyOptions = false;

    for (int i = 1; i < argc; i++) {
        // no local variable for argv[i] as splitOption requires access to argv directly

        const bool startsWithDash = argv[i][0] == '-';
        const bool hasEquals = strchr(argv[i], '=') != NULL;

        if (onlyOptions) {
            splitOption(&argv[i]);
            if (!(startsWithDash && hasEquals)) {
                return PARSER_INIT_FAILURE;
            }
        } else {
            // the first option
            // everything else prior was an argument
            if (startsWithDash && hasEquals) {
                onlyOptions = true;
                argumentCount = i - 1;
                optionIndex = i;
                splitOption(&argv[i]);
            }
        }
    }

    // no options provided, everything in argv is arguments (except command)
    if (argumentCount == -1) {
        argumentCount = argc - 1;
    }

    arguments = argv;
    combinedCount = argc - 1;

    return PARSER_INIT_SUCCESS;
}

char* getCommand(void) {
    return command ? command : NULL;
}

int getNumberOfArguments(void) {
    return argumentCount;
}

char* getArgument(const int index) {
    if (index < 0 || index >= argumentCount) {
        return NULL;
    }

    return arguments[index + 1];
}

char* getValueForOption(const char* keyName) {
    if (optionIndex == -1) {
        return NULL;
    }

    for (int i = optionIndex; i <= combinedCount; i++) {
        char* currentOptionName = arguments[i];

        if (stringsEqual(currentOptionName, keyName)) {
            // the options are all split already
            // move to the end of the name and go over the NULL
            return currentOptionName + strlen(currentOptionName) + 1;
        }
    }

    return NULL;
}
