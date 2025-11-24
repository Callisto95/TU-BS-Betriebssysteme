#include "argumentParser.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAXIMUM_ARGUMENTS 50
#define MAXIMUM_OPTIONS 50

#define false 0
#define true 1
#define bool int

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

void parseOption(char* fullArgument, char** name, char** value) {
    // skip leading '-'
    fullArgument += 1;

    char* equals = strchr(fullArgument, '=');

    // no equals in string
    if (!equals) {
        return;
    }

    // since strings are null terminated, adding a \0 in place of the '=' splits the string
    *equals = '\0';
    *name = fullArgument;
    *value = equals + 1;
}

int initArgumentParser(const int argc, char* argv[]) {
    if (argc == 0) {
        return -1;
    }

    command = argv[0];
    bool onlyOptions = false;

    for (int i = 1; i < argc; i++) {
        const char* currentArgument = argv[i];

        if (onlyOptions && currentArgument[0] != '-') {
            return -1;
        }

        if (!onlyOptions && currentArgument[0] == '-') {
            argumentCount = i - 1;
            onlyOptions = true;
            optionIndex = i;
        }
    }

    if (argumentCount == -1) {
        argumentCount = argc - 1;
    }

    arguments = argv;
    combinedCount = argc - 1;

    return 0;
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
    
    char* name = NULL;
    char* value = NULL;
    
    for (int i = optionIndex; i <= combinedCount; i++) {
        char* currentOption = arguments[i];
        parseOption(currentOption, &name, &value);

        if (stringsEqual(name, keyName)) {
            return value;
        }
    }

    return NULL;
}
