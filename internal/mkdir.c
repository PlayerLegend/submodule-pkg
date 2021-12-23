#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#define FLAT_INCLUDES
#include "../../range/def.h"
#include "../../window/def.h"
#include "../../window/printf.h"
#include "../../window/path.h"
#include "../../log/log.h"
#include "../../window/string.h"
#include "../../keyargs/keyargs.h"
#include "../../immutable/immutable.h"
#include "def.h"
#include "mkdir.h"

inline static bool _mkdir (const char * path)
{
    if (-1 == mkdir (path, 0755) && errno != EEXIST)
    {
	perror (path);
	return false;
    }
    else
    {
	return true;
    }
}

bool _path_mkdir (const range_const_char * path, bool make_target)
{
    window_char path_copy = {0};

    window_strcpy_range (&path_copy, path);

    char * i;

    for_range (i, path_copy.region)
    {
	if (*i == PATH_SEPARATOR)
	{
	    *i = '\0';
	    if (!_mkdir (path_copy.region.begin))
	    {
		log_fatal ("Could not create a directory for %s", path);
	    }
	    *i = PATH_SEPARATOR;
	    
	}
    }

    if (make_target && !_mkdir (path_copy.region.begin))
    {
	log_fatal ("Could not create directory %s", path);
    }

    return true;

fail:
    return false;
}
