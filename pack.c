#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#define FLAT_INCLUDES
#include "../keyargs/keyargs.h"
#include "root.h"
#include "../range/def.h"
#include "../range/string.h"
#include "../window/def.h"
#include "../window/string.h"
#include "../window/path.h"
#include "../convert/sink.h"
#include "../tar/common.h"
#include "../window/alloc.h"
#include "../tar/write.h"
#include "../log/log.h"
#include "pack.h"

#define FILE_BUFFER_SIZE (int)1e6

struct pkg_pack_state {
    convert_sink * package_sink;
    window_unsigned_char tar_buffer;
    size_t name_skip;
};

static void pkg_pack_state_free (pkg_pack_state * state)
{
    window_clear (state->tar_buffer);
    free (state);
}

keyargs_define(pkg_pack_new)
{
    pkg_pack_state * retval = calloc (1, sizeof(*retval));
    tar_type type = TAR_ERROR;
    window_unsigned_char buffer = {0};

    retval->package_sink = args.sink;
    
    if (!tar_write_sink_path(.sink = retval->package_sink,
			     .path = args.script_path,
			     .buffer = &buffer,
			     .override_name = "build.sh",
			     .detect_type = &type))
    {
	goto fail;
    }

    if (type != TAR_FILE)
    {
	log_fatal ("Script path does not refer to a file: %s", args.script_path);
    }

    window_clear (buffer);
    
    return retval;

fail:
    window_clear (buffer);
    pkg_pack_state_free (retval);
    return NULL;
}
/*
static bool flush_buffer_to_fd (int fd, buffer_unsigned_char * buffer)
{
    if (range_is_empty(*buffer))
    {
	return true;
    }
    
    size_t wrote_size = 0;
    long int size;

    while (0 < (size = buffer_write (.fd = fd,
				     .buffer = &buffer->char_cast.range_cast.const_cast,
				     .wrote_size = &wrote_size)))
    {}

    if (size < 0)
    {
	log_fatal ("Failed to write buffer to output");
    }

    buffer_rewrite (*buffer);

    return true;

fail:
    return false;
}

static bool flush_tar_buffer (pkg_pack_state * state)
{
    if (state->dzip_deflate_state)
    {
	dzip_deflate(.state = state->dzip_deflate_state,
		     .input = &state->tar_buffer.range_cast.const_cast,
		     .output = &state->compress_buffer);
	buffer_rewrite(state->tar_buffer);
	log_debug ("used deflate");
	return true;
    }
    else
    {
	return flush_buffer_to_fd (state->output_fd, &state->tar_buffer);
    }
}

static bool flush_compress_buffer (pkg_pack_state * state)
{
    return flush_buffer_to_fd (state->output_fd, &state->compress_buffer);
}

static bool optional_flush_buffers (pkg_pack_state * state)
{
    if (range_count (state->tar_buffer) >= FILE_BUFFER_SIZE)
    {
	if (!flush_tar_buffer(state))
	{
	    log_fatal ("Failed to flush the tar buffer");
	}
    }

    if (range_count (state->compress_buffer) >= FILE_BUFFER_SIZE)
    {
	if (!flush_compress_buffer(state))
	{
	    log_fatal ("Failed to flush the compress buffer");
	}
    }

    return true;

fail:
    return false;
    }*/

/*static bool pkg_pack_path_sub(pkg_pack_state * state, const char * path);
static bool pkg_pack_file(pkg_pack_state * state, const char * path, size_t size)
{
    int file_fd = open (path, O_RDONLY);

    if (file_fd < 0)
    {
	perror(path);
	log_fatal ("Could not open file %s", path);
    }

    size_t io_size;
    size_t start_size;
    
    optional_flush_buffers (state);

    while (true)
    {
	start_size = range_count(state->tar_buffer);
	
	while (0 < (io_size = buffer_read (.buffer = &state->tar_buffer.char_cast,
					   .initial_alloc_size = FILE_BUFFER_SIZE,
					   .max_buffer_size = FILE_BUFFER_SIZE,
					   .fd = file_fd)))
	{}

	if (io_size < 0)
	{
	    log_fatal ("Failed to read from file %s", path);
	}

	if (start_size == (size_t)range_count(state->tar_buffer))
	{
	    tar_write_padding(&state->tar_buffer.char_cast, size);
	    break;
	}

	optional_flush_buffers (state);
    }

    close (file_fd);
    
    return true;

fail:
    window_clear (contents);
    convert_source_clear (&source.source);
    return false;
    }*/

static bool pkg_pack_dir(window_unsigned_char * file_buffer, pkg_pack_state * state, const char * path);
static bool pkg_pack_path_sub(window_unsigned_char * file_buffer, pkg_pack_state * state, const char * path)
{
    log_debug ("packing path %s", path);

    tar_type type;

    if (state->name_skip > strlen(path))
    {
	log_fatal ("Name skip too long, %zu > %zu", state->name_skip, strlen(path));
    }

    if (!tar_write_sink_path (.sink = state->package_sink,
			      .path = path,
			      .buffer = file_buffer,
			      .override_name = path + state->name_skip,
			      .detect_type = &type))
    {
	log_fatal ("Failed to write path");
    }

    if (type == TAR_DIR)
    {
	return pkg_pack_dir (file_buffer, state, path);
    }
    else
    {
	return true;
    }

fail:
    return false;
}

static bool pkg_pack_dir(window_unsigned_char * file_buffer, pkg_pack_state * state, const char * path)
{
    DIR * dir = opendir (path);
    struct dirent * ent;

    window_char path_buffer = {0};

    window_strcpy (&path_buffer, path);

    size_t base_path_length = range_count (path_buffer.region);
    range_const_char ent_name;

    #define PATH_SEPARATOR '/'
    
    while ( (ent = readdir (dir)) )
    {
	if (0 == strcmp (ent->d_name, "..") || 0 == strcmp (ent->d_name, "."))
	{
	    continue;
	}

	path_buffer.region.end = path_buffer.region.begin + base_path_length;
	assert (path_buffer.region.end >= path_buffer.alloc.begin && path_buffer.region.end < path_buffer.alloc.end);
	
	range_string_init(&ent_name, ent->d_name);
	window_path_cat (&path_buffer, PATH_SEPARATOR, &ent_name);

	if (!pkg_pack_path_sub (file_buffer, state, path_buffer.region.begin))
	{
	    goto fail;
	}
    }

    window_clear (path_buffer);
    closedir (dir);
    return true;

fail:
    window_clear (path_buffer);
    closedir (dir);
    return false;
}


bool pkg_pack_path(pkg_pack_state * state, const char * path)
{
    state->name_skip = strlen (path);
    window_unsigned_char file_buffer = {0};
    
    bool retval = pkg_pack_path_sub (&file_buffer, state, path);

    window_clear (file_buffer);

    return retval;
}

bool pkg_pack_finish (pkg_pack_state * state)
{
    return tar_write_sink_end(state->package_sink);
}
