#ifndef FLAT_INCLUDES
#include <stddef.h>
#include <stdbool.h>
#define FLAT_INCLUDES
#include "../range/def.h"
#include "../window/def.h"
#include "../convert/source.h"
#include "../keyargs/keyargs.h"
#include "root.h"
#endif

bool pkg_install(pkg_root * root, convert_source * source);

