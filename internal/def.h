#ifndef FLAT_INCLUDES
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#define FLAT_INCLUDES
#include "../../keyargs/keyargs.h"
#include "../root.h"
#include "../../range/def.h"
#include "../../window/def.h"
#include "../../window/string.h"
#include "../../keyargs/keyargs.h"
#include "../../immutable/immutable.h"
#endif

#include "../../table/table.h"
#define table_string_value immutable_text package_name;
#include "../../table/table-string.h"

#define LOG_PATH "pkg/system.log"

struct pkg_root {
    window_char tmp;
    window_char path;
    table_string log;
    window_const_string protected_paths;
};

inline static bool file_exists(const char * path)
{
    struct stat s;
    return 0 == stat (path, &s);
}

inline static immutable_text immutable_string_from_log (pkg_root * root, const char * string)
{
    return (immutable_text){ .text = table_string_include (root->log, string)->query.key };
}

inline static immutable_text immutable_string_range_from_log (pkg_root * root, const range_const_char * string)
{
    window_strcpy_range (&root->tmp, string);
    return immutable_string_from_log (root, root->tmp.region.begin);
}
#define PATH_SEPARATOR '/'
