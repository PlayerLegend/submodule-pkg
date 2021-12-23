#ifndef FLAT_INCLUDES
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#define FLAT_INCLUDES
#include "../../keyargs/keyargs.h"
#include "../root.h"
#include "../../range/def.h"
#include "../../window/def.h"
#include "../../keyargs/keyargs.h"
#include "../../immutable/immutable.h"
#endif

#include "../../table/table.h"
#define table_string_value immutable_text package_name;
#include "../../table/table-string.h"

#define LOG_PATH "pkg/system.log"

struct pkg_root {
    struct {
	//window_char name;
	const char * name;
	window_char path;
	window_char build_sh;
    }
	tmp;
    
    window_char path;
    table_string log;
    window_const_string protected_paths;
    immutable_namespace * namespace;
};

inline static bool file_exists(const char * path)
{
    struct stat s;
    return 0 == stat (path, &s);
}

#define PATH_SEPARATOR '/'