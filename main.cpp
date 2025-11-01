#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "Debug.h"
#include "conversion.h"
#include "preferences.h"
#include "MagiCPrint.h"
#include "MagiCSerial.h"
#include "EmulationMain.h"


const char *argnames[] =
{
    nullptr,
    "config-file",
    nullptr,
    nullptr,
    "program"
};

const char *descriptions[] =
{
    "                display help text and exit",
    "  open configuration file in editor and exit",
    "         configuration file (default: ~/.config/magiclinux.conf)",
    "        write configuration file with default values and exit",
    "      choose editor program for -e option, default is gnome-text-editor"
};

static void print_opt(const struct option *options)
{
    puts("Usage:");
    int index = 0;
    while(options->name != nullptr)
    {
        if (options->val != 0)
        {
            printf("  -%c, ", options->val);
        }
        else
        {
            printf("      ");
        }
        printf("--%s", options->name);
        if (options->has_arg == required_argument)
        {
            printf("=%s", argnames[index]);
        }
        else
        if (options->has_arg == optional_argument)
        {
            printf(" [%s]", argnames[index]);
        }
        printf("%s\n", descriptions[index]);
        options++;
        index++;
    }
}


int main(int argc, char *argv[])
{
    int c;
    const char *config = "~/.config/magiclinux.conf";
    const char *editor = "gnome-text-editor";
    bool bRunEditor = false;
    bool bWriteConf = false;

    for (;;)
    {
        // optind, initialised to 1, is an external variable from getopt()
        // and next index to be processed.
        int long_option_index = 0;
        static const struct option long_options[] =
        {
            {"help",         no_argument,       nullptr, 'h' },
            {"config",       required_argument, nullptr, 'c' },
            {"config-edit",  no_argument,       nullptr, 'e' },
            {"config-write", no_argument,       nullptr, 'w' },
            {"editor",       required_argument, nullptr,  0 },
            {nullptr,        0,                 nullptr,  0 }
        };
        c = getopt_long(argc, argv, "hc:ew",
                        long_options, &long_option_index);

        if (c == -1)
        {
            // No more options. if optind < argc, then argv[optind ..]
            // are additional parameters
            break;
        }

        switch (c)
        {
            case 0:
                if (long_option_index == 4)
                {
                    editor = optarg;
                }
                printf("option %s", long_options[long_option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'h':
                print_opt(long_options);
                return 0;
                break;

            case 'e':
                bRunEditor = true;
                break;

            case 'c':
                config = optarg;
                break;

            case 'w':
                bWriteConf = true;
                break;

            case '?':
                // parsing error
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (optind < argc)
    {
        printf("additional options ignored: ");
        while (optind < argc)
        {
            printf("%s ", argv[optind++]);
        }
        printf("\n");
    }

    if (bWriteConf)
    {
        Preferences::init(config, true);
        return 0;
    }

    if (bRunEditor)
    {
        char command[1024];
        sprintf(command, "%s %s", editor, config);
        (void) system(command);
        return 0;
    }

/*
    printf("__UINTPTR_MAX__ == 0x%lx\n", __UINTPTR_MAX__);
#if __UINTPTR_MAX__ == 0xFFFFFFFFFFFFFFFF
    printf("64-bit mode\n");
#else
    printf("32-bit mode\n");
#endif
*/

    #if 0
    extern int GuiMyAlert(const char *msg_text, const char *info_txt, int nButtons);
    GuiMyAlert("Greetings!", "This is MagicOnLinux", 3);
    return 0;
    #endif

    DebugInit(NULL /* stderr */);
    if (Preferences::init(config, false))
    {
        fputs("There were syntax errors in configuration file\n", stderr);
    }
    CConversion::init();
    CMagiCPrint::init();
    CMagiCSerial::init();
    EmulationInit();
    EmulationOpenWindow();
    EmulationRun();
    EmulationRunSdl();
    CMagiCPrint::exit();
    CMagiCSerial::exit();

    return 0;
}
