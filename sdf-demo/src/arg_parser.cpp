#include "arg_parser.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#define next_and_check(i)                                                                                                                  \
    (i)++;                                                                                                                                 \
    assert((i) < argc)

void ArgParser::parseCommandLine(int argc, const char *argv[]) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0) {
            next_and_check(i);
            input_filename = argv[i];
        } else if (strcmp(argv[i], "-o") == 0) {
            next_and_check(i);
            output_filename = argv[i];
        } else if (strcmp(argv[i], "-v") == 0) {
            next_and_check(i);
            voxel_density = (float) atof(argv[i]);
        } else if (strcmp(argv[i], "-scale") == 0) {
            next_and_check(i);
            df_resolution_scale = (float) atof(argv[i]);
        } else if (strcmp(argv[i], "-no-parallel") == 0) {
            parallel = false;
        } else if (strcmp(argv[i], "-brick") == 0) {
            debug_brick = true;
        }
    }
}