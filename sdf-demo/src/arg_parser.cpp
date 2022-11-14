#include "arg_parser.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#define next_and_check(i)                                                                                                                  \
    (i)++;                                                                                                                                 \
    assert((i) < argc)

void ArgParser::parseCommandLine(int argc, const char *argv[]) {
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-i")) {
            next_and_check(i);
            input_filename = argv[i];
        } else if (!strcmp(argv[i], "-o")) {
            next_and_check(i);
            output_filename = argv[i];
        } else if (!strcmp(argv[i], "-v")) {
            next_and_check(i);
            voxel_density = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-scale")) {
            next_and_check(i);
            DF_resolution_scale = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-d")) {
            next_and_check(i);
            display_distance = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-outside")) {
            outside_only = true;
        } else if (!strcmp(argv[i], "-sample")) {
            sample_mode = true;
        } else if (!strcmp(argv[i], "-parallel")) {
            parallel = true;
        }
    }
}