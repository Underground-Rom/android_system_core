/*
 * Android "Almost" C Compiler.
 * This is a compiler for a small subset of the C language, intended for use
 * in scripting environments where speed and memory footprint are important.
 *
 * This code is based upon the "unobfuscated" version of the
 * Obfuscated Tiny C compiler, see the file LICENSE for details.
 *
 */

#include <ctype.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__arm__)
#include <unistd.h>
#endif

#include <acc/acc.h>


typedef int (*MainPtr)(int, char**);
// This is a separate function so it can easily be set by breakpoint in gdb.
int run(MainPtr mainFunc, int argc, char** argv) {
    return mainFunc(argc, argv);
}

int main(int argc, char** argv) {
    const char* inFile = NULL;
    bool printListing;
    FILE* in = stdin;
    int i;
    for (i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] == '-') {
            switch (arg[1]) {
                case 'S':
                    printListing = true;
                    break;
            default:
                fprintf(stderr, "Unrecognized flag %s\n", arg);
                return 3;
            }
        } else if (inFile == NULL) {
            inFile = arg;
        } else {
            break;
        }
    }

    if (! inFile) {
        fprintf(stderr, "input file required\n");
        return 2;
    }

    if (inFile) {
        in = fopen(inFile, "r");
        if (!in) {
            fprintf(stderr, "Could not open input file %s\n", inFile);
            return 1;
        }
    }

    fseek(in, 0, SEEK_END);
    size_t fileSize = (size_t) ftell(in);
    rewind(in);
    ACCchar* text = new ACCchar[fileSize];
    size_t bytesRead = fread(text, 1, fileSize, in);
    if (bytesRead != fileSize) {
        fprintf(stderr, "Could not read all of file %s\n", inFile);
    }

    ACCscript* script = accCreateScript();

    const ACCchar* scriptSource[] = {text};
    accScriptSource(script, 1, scriptSource, NULL);
    delete[] text;

    accCompileScript(script);

    MainPtr mainPointer = 0;

    accGetScriptLabel(script, "main", (ACCvoid**) & mainPointer);

    int result = accGetError(script);
    if (result == ACC_NO_ERROR) {
        fprintf(stderr, "Executing compiled code:\n");
        int codeArgc = argc - i + 1;
        char** codeArgv = argv + i - 1;
        codeArgv[0] = (char*) (inFile ? inFile : "stdin");
        result = run(mainPointer, codeArgc, codeArgv);
        fprintf(stderr, "result: %d\n", result);
    }

    accDeleteScript(script);

    return result;
}
