#ifndef FLAT_INCLUDES
#include <stdbool.h>
#define FLAT_INCLUDES
#include "../../range/def.h"
#endif

bool _path_mkdir (const range_const_char * path, bool make_target);

#define mkdir_recursive_target(path) _path_mkdir (path, true)
#define mkdir_recursive_parents(path) _path_mkdir (path, false)
