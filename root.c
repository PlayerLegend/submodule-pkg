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
#include "../range/string.h"
#include "../window/def.h"
#include "../window/string.h"
#include "../window/alloc.h"
#include "../window/printf.h"
#include "../convert/source.h"
#include "../convert/fd/source.h"
#include "../convert/sink.h"
#include "../convert/fd/sink.h"
#include "../convert/getline.h"
#include "../keyargs/keyargs.h"
#include "../immutable/immutable.h"
#include "internal/def.h"
#include "internal/mkdir.h"
#include "../log/log.h"
#include "../lang/error/error.h"
#include "../lang/tree/tree.h"
#include "../lang/tokenizer/tokenizer.h"

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

static bool dump_log_to_interface(convert_sink * sink, table_string * log)
{
    table_string_bucket bucket;
    table_string_item * item;

    window_unsigned_char content_buffer = {0};
    sink->contents = &content_buffer.region.const_cast;
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
        
    if (!mkdir_recursive_parents(&log_file_path.region.const_cast))
    {
	log_fatal ("Could not create parent directories the package log");
    }
    
    log_fd = open (log_file_path.region.begin, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    if (log_fd < 0)
    {
	log_fatal ("Could not open the log file at %s", log_file_path.region.begin);
    }

    fd_sink log_fd_sink = fd_sink_init (.fd = log_fd);

    if (!dump_log_to_interface (&log_fd_sink.sink, &root->log))
    {
	log_fatal ("Could not write to log file");
    }

    assert (log_fd >= 0);

    convert_sink_clear (&log_fd_sink.sink);
        
    return true;
    
fail:

    if (log_fd >= 0)
    {
	close (log_fd);
    }
    
    return false;
}

static bool paren_config_entry_protected_paths (pkg_root * root, lang_tree_node * key)
{
    lang_tree_node * i = key->peer;

    while (i)
    {
	if (!i->is_text)
	{
	    lang_log_fatal(i->source_position, "Arguments to %s should be text", key->immutable.text);
	}

	*window_push (root->protected_paths) = table_string_include(root->log, i->immutable.text)->query.key;
	
	i = i->peer;
    }

    return true;

fail:
    return false;
}

static bool parse_config_entry (pkg_root * root, lang_tree_node * node)
{
    if (node->is_text)
    {
	lang_log_fatal (node->source_position, "Text node at root of config, this should be in parentheses");
    }

    lang_tree_node * key = node->child;

    if (!key)
    {
	return true;
    }

    if (!key->is_text)
    {
	lang_log_fatal (key->source_position, "Expected a text key, e.g. repos or protected-paths");
    }

    if (0 == strcmp (key->immutable.text, "protect-paths"))
    {
	if (!paren_config_entry_protected_paths (root, key))
	{
	    lang_log_fatal (key->source_position, "Could not parse a protected-paths item");
	}
    }
    else
    {
	lang_log_warning(key->source_position, "Unrecognized config key '%s'", key->immutable.text);
    }

    return true;

fail:
    return false;
}

static lang_tree_node * gen_tree (immutable_namespace * namespace, const char * path)
{
    int fd = open (path, O_RDONLY);

    if (fd < 0)
    {
	perror (path);
	return NULL;
    }

    window_unsigned_char read_buffer = {0};

    fd_source fd_source = fd_source_init (.fd = fd, .contents = &read_buffer);

    bool error = false;

    lang_tree_build_env build_env;

    lang_tree_build_start(&build_env);

    lang_tokenizer_state tokenizer_state = { .source = &fd_source.source };

    range_const_char token;

    while (tokenizer_read(&error, &token, &tokenizer_state))
    {
	if (!lang_tree_build_update(&build_env, &tokenizer_state.token_position, immutable_string_range(namespace, &token)))
	{
	    error = true;
	    break;
	}
    }

    lang_tree_node * retval = error ? NULL : lang_tree_build_finish(&build_env);

    window_clear (read_buffer);
    convert_source_clear (&fd_source.source);

    return retval;
}

static bool load_config (pkg_root * root)
{
    window_char config_path = {0};
    window_printf (&config_path, "%s/etc/pkg/config", root->path.region.begin);
    
    if (!file_exists(config_path.region.begin))
    {
	window_clear (config_path);
	return true;
    }

    immutable_namespace * config_namespace = immutable_namespace_new();

    lang_tree_node * config_root = gen_tree (config_namespace, config_path.region.begin);
    
    if (!config_root)
    {
	log_fatal ("Failed to load config %s", config_path);
    }

    lang_tree_node * i = config_root;
    
    do {
	if (!parse_config_entry(root, i))
	{
	    log_fatal ("Could not parse a config entry");
	}
    }
    while ( (i = i->peer) );

    window_clear (config_path);
    lang_tree_free (config_root);
    immutable_namespace_free(config_namespace);
    
    return true;

fail:
    window_clear (config_path);
    lang_tree_free (config_root);
    immutable_namespace_free(config_namespace);
    
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

static bool read_log_from_fd (pkg_root * root, int fd)
{
    window_unsigned_char read_buffer = {0};

    fd_source fd_source = fd_source_init (.fd = fd, .contents = &read_buffer);

    bool error = false;

    range_const_char line_range;
    window_char line_copy = {0};
    range_const_char end_seq;

    range_string_init (&end_seq, "\n");
    
    char * name;
    char * path;
    int line_number = 0;
    
    while (convert_getline(&error, &line_range, &fd_source.source, &end_seq))
    {
	line_number++;
	window_strcpy_range (&line_copy, &line_range);
	if (!parse_log_line (&name, &path, line_copy.region.begin))
	{
	    log_fatal ("Failed to parse log line %d: " RANGE_FORMSPEC, line_number, RANGE_FORMSPEC_ARG(line_range));
	}
	table_string_include(root->log, path)->value.package_name = immutable_string_from_log(root, name);
    }
    
    window_clear (read_buffer);
    
    return true;

fail:
    window_clear (read_buffer);
    return false;
}

static bool read_log (pkg_root * root)
{
    int log_fd = -1;
    
    window_char log_file_path = {0};

    set_log_file_path (&log_file_path, &root->path.region.const_cast);
    
    log_fd = open (log_file_path.region.begin, O_RDONLY);

    if (0 > log_fd)
    {
	if (errno == ENOENT)
	{
	    return true;
	}
	
	perror (log_file_path.region.begin);
	window_clear(log_file_path);
	log_fatal ("Could not open package log file");
    }

    if (!read_log_from_fd (root, log_fd))
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

    window_printf (&retval->path, "%s", path);

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
    window_clear (root->tmp);
    window_clear (root->path);
    window_clear (root->protected_paths);
    free (root);
}
