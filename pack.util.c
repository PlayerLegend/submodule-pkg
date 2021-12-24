#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#define FLAT_INCLUDES
#include "../keyargs/keyargs.h"
#include "../range/def.h"
#include "../convert/sink.h"
#include "../convert/fd/sink.h"
#include "pack.h"
#include "../log/log.h"

#define STDOUT_FILENO 1

void help_message(const char * program_name)
{
    log_stderr ("%s - create a package from one or more directories", program_name);
}

int main(int argc, char * argv[])
{
    const char * script_path = NULL;

    static struct option long_options[] =
	{
	    {"help", no_argument, 0, 'h'},
	    {"compress", required_argument, 0, 'c'},
	    {"script", required_argument, 0, 's'},
	};

    int c;
    int option_index = 0;

    while ((c = getopt_long (argc, argv, "hc:s:", long_options, &option_index)))
    {
	if (c == -1)
	{
	    break;
	}

	switch (c)
	{
	case 'h':
	    help_message (argv[0]);
	    goto fail;

	case 's':
	    if (script_path)
	    {
		log_fatal ("Multiple script arguments is invalid");
	    }

	    script_path = optarg;
	}
    }

    if (!script_path)
    {
	log_fatal ("No script path specified, use -s or --script");
    }

    fd_sink fd_sink = fd_sink_init (.fd = STDOUT_FILENO);
    
    pkg_pack_state * state = pkg_pack_new(.sink = &fd_sink.sink,
					  .script_path = script_path);

    if (!state)
    {
	log_fatal ("Failed to create pkg-pack state");
    }

    for (int i = optind; i < argc; i++)
    {
	if (!pkg_pack_path(state, argv[i]))
	{
	    log_fatal ("Failed to add a path");
	}
    }

    if (!pkg_pack_finish(state))
    {
	log_fatal ("Failed to finish package");
    }
    
    return 0;

fail:
    return 1;
}
