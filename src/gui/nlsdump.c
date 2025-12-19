#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nls.h"
#include "pofile.h"

char const program_name[] = "nlsdump";
char const program_version[] = "1.0";

/*
 * program options
 */
static _BOOL verbose = FALSE;

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

void erroutv(const char *format, va_list args)
{
	vfprintf(stderr, format, args);
}

/* ------------------------------------------------------------------------- */

void errout(const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
	erroutv(format, args);
	va_end(args);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

enum nlsdump_opt {
	OPT_VERBOSE = 'v',
	OPT_PODIR = 'p',
	OPT_KEYNAMES = 'k',
	OPT_OUTPUT = 'o',
	OPT_VERSION = 'V',
	OPT_HELP = 'h',
	
	OPT_SETVAR = 0,
	OPT_OPTERROR = '?',
};

static struct option const long_options[] = {
	{ "verbose", no_argument, NULL, OPT_VERBOSE },
	{ "podir", required_argument, NULL, OPT_PODIR },
	{ "keys", required_argument, NULL, OPT_KEYNAMES },
	{ "output", required_argument, NULL, OPT_OUTPUT },
	{ "version", no_argument, NULL, OPT_VERSION },
	{ "help", no_argument, NULL, OPT_HELP },
	{ NULL, no_argument, NULL, 0 }
};

/* ------------------------------------------------------------------------- */

static void usage(FILE *fp)
{
	fprintf(fp, _("%s - Create translations from PO files\n"), program_name);
	fprintf(fp, _("Usage: %s [<options>] <file...>\n"), program_name);
	fprintf(fp, _("Options:\n"));
	fprintf(fp, _("   -v, --verbose               emit some progress messages\n"));
	fprintf(fp, _("   -p, --podir <dir>           lookup po-files in <dir>\n"));
	fprintf(fp, _("   -o, --output <file>         write output to <file>\n"));
	fprintf(fp, _("   -k, --keys <name>           name of the key string table\n"));
	fprintf(fp, _("       --version               print version and exit\n"));
	fprintf(fp, _("       --help                  print this help and exit\n"));
}

/* ------------------------------------------------------------------------- */

static void print_version(void)
{
	printf(_("%s version %s\n"), program_name, program_version);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	int c;
	const char *po_dir = NULL;
	int i;
	char **languages;
	int ret = EXIT_SUCCESS;
	FILE *out = stdout;
	const char *outfile_name = NULL;
	const char *key_string_name = NULL;

	while ((c = getopt_long_only(argc, argv, "k:o:p:vhV", long_options, NULL)) != EOF)
	{
		switch ((enum nlsdump_opt) c)
		{
		case OPT_PODIR:
			po_dir = optarg;
			break;
		
		case OPT_KEYNAMES:
			key_string_name = optarg;
			break;
		
		case OPT_OUTPUT:
			outfile_name = optarg;
			break;
		
		case OPT_VERBOSE:
			verbose = TRUE;
			break;
		
		case OPT_VERSION:
			print_version();
			return EXIT_SUCCESS;
		
		case OPT_HELP:
			usage(stdout);
			return EXIT_SUCCESS;

		case OPT_SETVAR:
			/* option which just sets a var */
			break;
		
		case OPT_OPTERROR:
		default:
			usage(stderr);
			return EXIT_FAILURE;
		}
	}

	languages = po_init(po_dir);
	if (languages == NULL)
	{
		ret = EXIT_FAILURE;
	} else
	{
		if (outfile_name != NULL)
		{
			out = fopen(outfile_name, "w");
			if (out == NULL)
			{
				fprintf(stderr, "%s: %s\n", outfile_name, strerror(errno));
				ret = EXIT_FAILURE;
			}
		}
		if (ret == EXIT_SUCCESS)
		{
			for (i = 0; languages[i] != NULL; i++)
			{
				char *lang = languages[i];
				po_domain domain;
				
				memset(&domain, 0, sizeof(domain));
				domain.key_string_name = key_string_name;
				if (po_create_hash(&domain, po_dir, lang, verbose) == FALSE)
					ret = EXIT_FAILURE;
				if (i == 0)
					po_dump_keys(&domain, out);
				po_dump_hash(&domain, out);
				po_delete_hash(&domain);
			}
			po_dump_languages(languages, out);
			po_exit(languages);
		}
		if (out != stdout)
			fclose(stdout);
	}
	
	return ret;
}
