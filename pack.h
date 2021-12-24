#ifndef FLAT_INCLUDES
#include <stdbool.h>
#include <stddef.h>
#define FLAT_INCLUDES
#include "../keyargs/keyargs.h"
#include "../range/def.h"
#include "../window/def.h"
#include "../convert/source.h"
#include "../convert/sink.h"
#endif

typedef struct pkg_pack_state pkg_pack_state;

keyargs_declare(pkg_pack_state*, pkg_pack_new,
		convert_sink * sink;
		const char * script_path;);

#define pkg_pack_new(...) keyargs_call(pkg_pack_new,__VA_ARGS__)

bool pkg_pack_path(pkg_pack_state * state, const char * path);

bool pkg_pack_finish (pkg_pack_state * state);
