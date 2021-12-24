#ifndef FLAT_INCLUDES
#include <stdbool.h>
#define FLAT_INCLUDES
#include "../keyargs/keyargs.h"
#endif

typedef struct pkg_root pkg_root;

pkg_root * pkg_root_open(const char * path);
void pkg_root_close (pkg_root * root);
