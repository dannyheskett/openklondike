// Dedicated translation unit for minih264's implementation. Its internal
// symbols collide with minimp4 if both implementations are compiled together,
// so each lives in its own .c file.
//
// The vendored upstream header trips warnings under -Wall -Wextra; they are
// suppressed here so the project's own code stays warning-clean.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
#pragma GCC diagnostic ignored "-Wint-conversion"

#define MINIH264_IMPLEMENTATION
#include "minih264e.h"

#pragma GCC diagnostic pop
