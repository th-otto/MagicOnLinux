#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#define GETTEXT_PACKAGE MagicOnLinux
#include "nls-enable.h"
DECLARE_DOMAIN(GETTEXT_PACKAGE);
#include "pofile.h"

char const program_name[] = "nlstool";
char const program_version[] = "1.0";

#define _STRINGIFY1(x) #x
#define _STRINGIFY(x) _STRINGIFY1(x)

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

static int make(const char *po_dir, const char *default_domain, const char *outfile_name)
{
	FILE *out = stdout;
	po_domain **languages;
	int i;
	int ret = TRUE;

	languages = po_init(po_dir);
	if (languages == NULL)
		return FALSE;
	if (outfile_name != NULL)
	{
		out = fopen(outfile_name, "w");
		if (out == NULL)
		{
			fprintf(stderr, "%s: %s\n", outfile_name, strerror(errno));
			return FALSE;
		}
	}
	for (i = 0; languages[i] != NULL; i++)
	{
		po_domain *domain = languages[i];

		if (po_create_translations(domain, po_dir, verbose) == FALSE)
			ret = FALSE;
		if (i == 0)
		{
			po_dump_keys(domain, out);
		} else
		{
			if (!po_verify_keys(languages[0], domain))
				ret = FALSE;
		}
		po_dump_translation(domain, out);
		po_delete_translations(domain);
	}
	po_dump_languages(default_domain, languages, out);
	po_exit(languages);
	if (out != stdout)
		fclose(stdout);
	return ret;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

enum nlstool_opt {
	OPT_VERBOSE = 'v',
	OPT_PODIR = 'p',
	OPT_KEYNAMES = 'k',
	OPT_OUTPUT = 'o',
	OPT_PACKAGE = 'd',
	OPT_VERSION = 'V',
	OPT_HELP = 'h',
	
	OPT_SETVAR = 0,
	OPT_OPTERROR = '?',
};

static struct option const long_options[] = {
	{ "verbose", no_argument, NULL, OPT_VERBOSE },
	{ "podir", required_argument, NULL, OPT_PODIR },
	{ "default-domain", required_argument, NULL, OPT_PACKAGE },
	{ "output", required_argument, NULL, OPT_OUTPUT },
	{ "version", no_argument, NULL, OPT_VERSION },
	{ "help", no_argument, NULL, OPT_HELP },
	{ NULL, no_argument, NULL, 0 }
};

/* ------------------------------------------------------------------------- */

static void usage(FILE *fp)
{
	fprintf(fp, _("%s - Create translations from PO files\n"), program_name);
	fprintf(fp, _("Usage: %s [<options>] [make|translate]\n"), program_name);
	fputs(      _("Commands are:\n"), fp);
	fputs(      _("  make                         read all files listed in LINGUAS,\n"
	              "                               and create C-source for translations\n"), fp);
	fputs(      _("  translate <lang> <files>     translate C-sources\n"), fp);
	fputs(        "\n", fp);
	fputs(      _("Options are:\n"), fp);
	fputs(      _("   -v, --verbose               emit some progress messages\n"), fp);
	fputs(      _("   -p, --podir <dir>           lookup po-files in <dir>\n"), fp);
	fputs(      _("   -o, --output <file>         write output to <file>\n"), fp);
	fputs(      _("   -d, --default-domain <name> specify packagename\n"), fp);
	fputs(      _("   -k, --keys <name>           name of the key string table\n"), fp);
	fputs(      _("       --version               print version and exit\n"), fp);
	fputs(      _("       --help                  print this help and exit\n"), fp);
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
	int ret = EXIT_SUCCESS;
	const char *outfile_name = NULL;
	const char *default_domain = "messages";
	const char *command = "make";

	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	{
#ifdef FORCE_LIBINTL
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), PACKAGE_LOCALE_DIR);
#else
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), NULL);
#endif
		textdomain(_STRINGIFY(GETTEXT_PACKAGE));
		bind_textdomain_codeset(_STRINGIFY(GETTEXT_PACKAGE), "UTF-8");
	}
#endif

	while ((c = getopt_long_only(argc, argv, "k:o:p:vhV", long_options, NULL)) != EOF)
	{
		switch ((enum nlstool_opt) c)
		{
		case OPT_PODIR:
			po_dir = optarg;
			break;
		
		case OPT_OUTPUT:
			outfile_name = optarg;
			break;
		
		case OPT_PACKAGE:
			default_domain = optarg;
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

	argc -= optind;
	argv += optind;
	if (argc > 0)
	{
		command = *argv++;
		argc--;
	}

	if (strcmp(command, "make") == 0)
	{
		if (argc != 0)
		{
			usage(stderr);
			ret = EXIT_FAILURE;
		} else if (!make(po_dir, default_domain, outfile_name))
		{
			ret = EXIT_FAILURE;
		}
    } else if (strcmp(command, "translate") == 0)
    {
        if (argc < 2)
        {
            usage(stderr);
			ret = EXIT_FAILURE;
        } else if (!po_translate(po_dir, default_domain, argv[0], argc - 1, &argv[1], verbose))
        {
			ret = EXIT_FAILURE;
	    }
	} else
	{
		fprintf(stderr, _("%s: unknown command %s\n"), program_name, command);
		ret = EXIT_FAILURE;
	}
	
	return ret;
}
