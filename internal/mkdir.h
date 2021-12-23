#ifndef FLAT_INCLUDES
#include <stdbool.h>
#define FLAT_INCLUDES
#include "../../range/def.h"
#endif

bool _path_mkdir (const range_const_char * path, bool make_target);
bool _mkdir_recursive (char * path, bool make_target);

#define mkdir_recursive_target(path) _mkdir_recursive (path, true)
#define mkdir_recursive_parents(path) _mkdir_recursive (path, false)
