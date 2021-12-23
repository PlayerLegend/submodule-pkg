#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define FLAT_INCLUDES
#include "../keyargs/keyargs.h"
#include "root.h"
#include "../range/def.h"
#include "../window/def.h"
#include "../window/alloc.h"
#include "../window/printf.h"
#include "../convert/def.h"
#include "../convert/fd.h"
#include "../keyargs/keyargs.h"
#include "../immutable/immutable.h"
#include "internal/def.h"
#include "internal/mkdir.h"
#include "../log/log.h"

/*static void load_log_file_path(pkg_root * root)
{
    buffer_printf (&root->tmp.path, "%s/" LOG_PATH, root->path.begin);
    }*/

static void set_log_file_path (window_char * target, const range_const_char * root_path)
{
    window_printf (target, RANGE_FORMSPEC "/" LOG_PATH, RANGE_FORMSPEC_ARG(*root_path));
}

static char * skip_isspace (char * input, bool pred)
{
    while (*input && pred == isspace(*input))
    {
	input++;
    }

    return input;
}

static bool dump_log_to_interface(convert_interface * sink, table_string * log)
{
    table_string_bucket bucket;
    table_string_item * item;

    window_unsigned_char content_buffer = {0};
    sink->write_range = &content_buffer.region.const_cast;
    bool error = false;
    
    for_table(bucket, *log)
    {
	for_table_bucket(item, bucket)
	{
	    if (item->value.package_name.text)
	    {
		window_printf_append(&content_buffer.signed_cast, "%s %s\n", item->value.package_name.text, item->query.key);

		if (!convert_drain(&error, sink))
		{
		    window_clear (content_buffer);
		    return false;
		}
		/*if (0 > dprintf (fd, "%s %s\n", item->value.package_name, item->query.key))
		{
		    perror ("dump_log_to_fd");
		    return false;
		    }*/
	    }
	}
    }
    
    window_clear (content_buffer);

    return true;
}

static bool dump_log(pkg_root * root)
{
    int log_fd = -1;

    window_char log_file_path = {0};

    set_log_file_path (&log_file_path, &root->path.region.const_cast);
        
    if (!mkdir_recursive_parents(log_file_path.region.begin))
    {
	log_fatal ("Could not create parent directories the package log");
    }
    
    log_fd = open (log_file_path.region.begin, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    if (log_fd < 0)
    {
	log_fatal ("Could not open the log file at %s", log_file_path.region.begin);
    }

    fd_interface log_fd_sink = fd_interface_init (.fd = log_fd);

    if (!dump_log_to_interface (&log_fd_sink.interface, &root->log))
    {
	log_fatal ("Could not write to log file");
    }

    assert (log_fd >= 0);

    convert_clear (&log_fd_sink.interface);
        
    return true;
    
fail:

    if (log_fd >= 0)
    {
	close (log_fd);
    }
    
    return false;
}

static bool paren_config_entry_protected_paths (pkg_root * root, paren_atom * key)
{
    paren_atom * i = key->peer;

    while (i)
    {
	if (!i->child.is_text)
	{
	    paren_fatal (i, "Arguments to %s should be text", key->child.text);
	}

	*buffer_push(root->protected_paths) = table_string_include(root->log, i->child.text)->query.key;
	
	i = i->peer;
    }

    return true;

fail:
    return false;
}

static bool parse_config_entry (pkg_root * root, paren_atom * atom)
{
    if (atom->child.is_text)
    {
	paren_fatal (atom, "Text node at root of config, this should be in parentheses");
    }

    paren_atom * key = atom->child.atom;

    if (!key->child.is_text)
    {
	paren_fatal (key, "Expected a text key, e.g. repos or protected-paths");
    }

    if (0 == strcmp (key->child.text, "protect-paths"))
    {
	if (!paren_config_entry_protected_paths (root, key))
	{
	    paren_fatal (atom, "Could not parse a protected-paths item");
	}
    }
    else
    {
	paren_warning (atom, "invalid config key '%s'", key->child.text);
    }

    return true;

fail:
    return false;
}

static bool load_config (pkg_root * root)
{
    buffer_printf (&root->tmp.path, "%s/etc/pkg/config", root->path.begin);
    const char * config_path = root->tmp.path.begin;

    if (!file_exists(config_path))
    {
	return true;
    }
    
    paren_atom * root_atom = paren_preprocessor(.filename = config_path);

    if (!root_atom)
    {
	log_fatal ("Failed to load config %s", config_path);
    }

    paren_atom * i = root_atom;

    do {
	if (!parse_config_entry(root, i))
	{
	    log_fatal ("Could not parse a config entry");
	}
    }
    while ( (i = i->peer) );

    paren_atom_free (root_atom);
    
    return true;

fail:
    paren_atom_free (root_atom);
    return false;
}

static bool parse_log_line (char ** name, char ** path, char * line)
{
    *name = skip_isspace (line, true);
    *path = skip_isspace(*name, false);

    if (!*path)
    {
	return false;
    }
    
    **path = '\0';
    
    *path = skip_isspace(*path + 1, true);

    return (*name != *path) && **path;
}

static bool read_log_fd (pkg_root * root, int fd)
{
    buffer_char read_buffer = {0};
    range_char line = {0};
    char * name;
    char * path;

    int line_number = 0;
    
    while (buffer_getline_fd (.line = &line,
			      .read.fd = fd,
			      .read.buffer = &read_buffer))
    {
	line_number++;
	*line.end = '\0';

	if (!parse_log_line (&name, &path, line.begin))
	{
	    log_fatal ("Failed to parse log line %d", line_number);
	}

	table_string_include(root->log, path)->value.package_name = table_string_include (root->log, name)->query.key;
    }

    free (read_buffer.begin);

    return true;

fail:
    free (read_buffer.begin);
    return false;
}

static bool read_log (pkg_root * root)
{
    int log_fd = -1;
    
    load_log_file_path (root);
    
    log_fd = open (root->tmp.path.begin, O_RDONLY);

    if (0 > log_fd)
    {
	if (errno == ENOENT)
	{
	    return true;
	}
	
	perror (root->path.begin);
	log_fatal ("Could not open package log file");
    }

    if (!read_log_fd (root, log_fd))
    {
	log_fatal ("Failed to parse package log file");
    }

    assert (log_fd >= 0);
    close (log_fd);

    return true;

fail:
    if (log_fd >= 0)
    {
	close (log_fd);
    }
    return false;
}

pkg_root * pkg_root_open(const char * path)
{
    pkg_root * retval = calloc (1, sizeof (*retval));

    buffer_printf (&retval->path, "%s", path);

    if (!load_config (retval))
    {
	log_fatal ("Failed to load config");
    }

    if (!read_log (retval))
    {
	log_fatal ("Failed to read package log file");
    }

    return retval;
    
fail:
    pkg_root_close (retval);
    return NULL;
}

void pkg_root_close (pkg_root * root)
{
    dump_log (root);
    
    table_string_clear(root->log);
    free (root->path.begin);
    free (root->tmp.path.begin);
    free (root->tmp.name.begin);
    free (root->tmp.build_sh.begin);
    free (root->protected_paths.begin);
    free (root);
}
