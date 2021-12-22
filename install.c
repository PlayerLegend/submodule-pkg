#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#define FLAT_INCLUDES
#include "../range/def.h"
#include "../window/def.h"
#include "../convert/def.h"
#include "../keyargs/keyargs.h"
#include "root.h"
#include "install.h"
#include "../range/string.h"
#include "../window/alloc.h"
#include "../window/string.h"
#include "../keyargs/keyargs.h"
#include "internal.h"
#include "../tar/common.h"
#include "../tar/read.h"
#include "../log/log.h"
#include "../path/path.h"
#include "../immutable/immutable.h"
#include "../convert/fd.h"

/*static bool read_file_contents (window_char * output, tar_state * state, int fd)
{
    return tar_read_region(.buffer = output, .fd = fd, .size = state->file.size);
    }*/

static bool verify_build_sh_name (tar_state * state)
{
    if (state->type != TAR_FILE)
    {
	log_fatal ("Expected a file for build.sh");
    }

    const char * basename = state->path.region.begin + range_strchr (&state->path.region.const_cast, PATH_SEPARATOR);
    
    if (basename == state->path.region.end)
    {
	basename = state->path.region.begin;
    }
    else
    {
	basename++;
    }

    if (0 != strcmp (basename, "build.sh"))
    {
	log_fatal ("First file is not named build.sh -- the package info file");
    }

    return true;

fail:
    return false;
}

typedef struct {
    window_char pkg_name;
}
    build_sh_info;

static const char * get_build_sh_name (pkg_root * root, const range_const_unsigned_char * contents)
{
    range_const_char name = contents->char_cast.const_cast;

    #define NAME_LINE_PREFIX_TEXT "PKG_NAME="
    #define NAME_LINE_PREFIX_LEN 9
    
    name.begin += range_strstr_string(&contents->char_cast.const_cast, NAME_LINE_PREFIX_TEXT);
    
    if (range_count (name) < NAME_LINE_PREFIX_LEN)
    {
	log_fatal ("No PKG_NAME assignment in build.sh");
    }

    name.begin += NAME_LINE_PREFIX_LEN;
    name.end = name.begin + range_strchr (&name, '\n');

    if (range_is_empty (name))
    {
	log_fatal ("PKG_NAME assignment in build.sh is empty");
    }

    if (*name.begin == '"')
    {
	if (range_count (name) <= 2)
	{
	    log_fatal ("PKG_NAME is an empty or incomplete quoted string");
	}
	
	name.end--;

	if (*name.end != *name.begin)
	{
	    log_fatal ("PKG_NAME is an unterminated quoted string");
	}

	name.begin++;
    }

    assert (range_count (name) > 0);
    
    const char * i;

    for_range (i, name)
    {
	if (isspace(*i))
	{
	    log_fatal ("Whitespace in package name");
	}
    }
    

    return immutable_range (root->namespace, &name);
    
fail:
    return NULL;
}

#define take_ownership(...) keyargs_call(take_ownership, __VA_ARGS__)
keyargs_declare_static(bool, take_ownership,
		       pkg_root * root;
		       const char * package_name;
		       const char * file_path;);

keyargs_define_static(take_ownership)
{
    table_string_item * path_item = table_string_include (args.root->log, args.file_path);

    if (path_item->value.package_name && path_item->value.package_name != args.package_name)
    {
	log_fatal ("Path %s is owned by %s, cannot allocate for %s", args.file_path, path_item->value.package_name, args.package_name);
    }
    else
    {
	path_item->value.package_name = args.package_name;
	return true;
    }

fail:
    return false;
}

static bool is_protected_compare (const char * check, const char * arg)
{
    while (*check && *check == *arg)
    {
	check++;
	arg++;
    }

    if (check[0] == arg[0])
    {
	return true;
    }

    if ((arg[0] == '\0' || (arg[0] == PATH_SEPARATOR && arg[1] == '\0'))
	&& check[0] == PATH_SEPARATOR)
    {
	return true;
    }
    
    return false;
}

static bool is_protected(pkg_root * root, const char * check)
{
    const char ** protect_arg;
    
    for_range (protect_arg, root->protected_paths.region)
    {
        if (is_protected_compare (check, *protect_arg))
	{
	    return true;
	}
    }

    return false;
}

static bool write_file (const char * path, tar_state * state)
{
    assert (state->type == TAR_FILE);
    
    range_const_unsigned_char file_part;

    fd_interface out_interface = fd_interface_init(.fd = open (path, O_WRONLY | O_CREAT, state->mode), .write_range = &file_part);
    
    if (out_interface.fd < 0)
    {
	perror (path);
	log_fatal ("Could not open ouput file");
    }

    while (tar_read_file_part (&file_part, state))
    {
	if (!convert_drain (&out_interface.interface))
	{
	    log_fatal ("Failed to write to file");
	}
    }

    if (state->type == TAR_ERROR || state->source->error)
    {
	log_fatal ("A read error occurred");
    }

    return true;
    
fail:
    return false;
}

typedef struct {
    pkg_pack_compression_type compression_type;
    tar_state state;
}
    compressed_tar_state;

bool tar_update_compressed_fd (compressed_tar_state * state)
{
    return false;
}

bool pkg_install(pkg_root * root, convert_interface * interface)
{
    tar_state state = { .source = interface };
    window_unsigned_char build_sh_contents = {0};

    while (tar_update (&state) && state.type != TAR_FILE) // skip to first file, which should be build.sh
    {
	if (state.type != TAR_DIR)
	{
	    log_fatal ("Expected only directories preceeding build.sh in this tar");
	}
    }

    if (state.type == TAR_ERROR)
    {
	log_fatal ("Encountered an error while searching for a file");
    }

    if (!verify_build_sh_name(&state)) // verify that this file is build.sh
    {
	log_fatal ("Invalid file for build.sh");
    }

    if (!tar_read_file_whole(&build_sh_contents, &state))
    {
	log_fatal ("Failed to read build.sh from package");
    }
    
    if (!parse_build_sh(root, root->tmp.build_sh.begin))
    {
	log_fatal ("Failed to parse build.sh");
    }

    while (tar_update_fd(&state, fd))
    {
	path_cat (&root->tmp.path, root->path.begin, state.path.begin);
	//window_printf (&root->tmp.path, "%s/%s", root->path.begin, state.path.begin);

	log_normal ("Installing %s: %s", root->tmp.name.begin, root->tmp.path.begin);
	
	switch (state.type)
	{
	case TAR_DIR:
	    
	    if (!path_mkdir_target(root->tmp.path.begin))
	    {
		log_fatal ("Could not create a package path");
	    }
	    break;

	case TAR_FILE:

	    if (file_exists (root->tmp.path.begin) && is_protected (root, state.path.begin))
	    {
		log_normal ("File %s exists and is protected", root->tmp.path.begin);
		tar_skip_file(&state, fd);
		break;
	    }
	    
	    if (!path_mkdir_parents(root->tmp.path.begin))
	    {
		log_fatal ("Could not create a package path");
	    }

	    if (!take_ownership (root))
	    {
		log_fatal ("Cannot install file %s", root->tmp.path.begin);
	    }
	    
	    if (!write_file (root->tmp.path.begin, &state, fd))
	    {
		log_fatal ("Could not write file %s", root->tmp.path.begin);
	    }
	    break;

	default:
	    continue;
	}
    }

    tar_cleanup (&state);
    
    return true;

fail:
    tar_cleanup (&state);
    return false;
}

