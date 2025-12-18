#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "Debug.h"
#include "conversion.h"
#include "preferences.h"
#include "MagiCPrint.h"
#include "MagiCSerial.h"
#include "EmulationRunner.h"

#if !defined(DEFAULT_EDITOR)
#define DEFAULT_EDITOR "xdg-open"
#endif

#if !defined(DEFAULT_CONFIG)
#define DEFAULT_CONFIG "~/.config/magiclinux.conf"
#endif


const char *argnames[] =
{
    nullptr,
    "config-file",
    nullptr,
    nullptr,
    "[wxh][x][b][ip]",
    "w[xh]",
    "abs|rel",
    "size",
    "EN|DE|FR",
    "path",
    "program",
    "atari_txtfile",
    "host_txtfile"
};

const char *descriptions[] =
{
    "                     display help text and exit",
    "       open configuration file in editor and exit",
    "              configuration file (default: ~/.config/magiclinux.conf)",
    "             write configuration file with default values and exit",
    " e.g. 640x400x2ip or 800x600 or 8, overrides config file",
    "            e.g. 2x2 or 2 or 2x4, overrides config file",
    "       mouse mode: absolute or relative",
    "             Atari RAM size, e.g. 512k or 4M or 3m",
    "            language for Atari localisation, either EN or DE or FR",
    "              location of C: drive (root fs)",
    "           choose editor program for -e option, to override xdg-open",
    "  convert text file from Atari to host format",
    "   convert text file from host to Atari format"
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


// return 0 if OK
static int eval_geometry(const char *geometry, int *mode, int *width, int *height)
{
    unsigned w, h, b;
    char c1, c2;
    bool ip;
    bool wh = true;

    int n = sscanf(geometry, "%ux%ux%u%c%c", &w, &h, &b, &c1, &c2);

    if ((n == 5) && (tolower(c1) == 'i') && (tolower(c2) == 'p'))
    {
        ip = true;
    }
    else
    if (n == 3)
    {
        ip = false;
    }
    else
    if (n == 2)
    {
        ip = false;
        b = 24;
    }
    else
    if ((n == 1) && ((n = sscanf(geometry, "%u%c%c", &b, &c1, &c2)) >= 1))
    {
        ip = ((n == 3) && (tolower(c1) == 'i') && (tolower(c2) == 'p'));
        w = -1;
        h = -1;
        wh = false;
    }
    else
    {
        printf("malformed geometry argument\n");
        return 3;
    }

    if (wh)
    {
        if ((w < ATARI_SCREEN_WIDTH_MIN)  || (w > ATARI_SCREEN_WIDTH_MAX) ||
            (h < ATARI_SCREEN_HEIGHT_MIN) || (h > ATARI_SCREEN_HEIGHT_MAX))
        {
            printf("Invalid Atari screen size\n");
            return 4;
        }
    }

    if (b == 24)
    {
        b = 32;
    }
    if ((b == 32) && !ip)
    {
        *mode = atariScreenMode16M;       // 32-bit true colour
    }
    else
    if ((b == 16) && !ip)
    {
        *mode = atariScreenModeHC;       // 16-bit high colour
    }
    else
    if ((b == 8) && !ip)
    {
        *mode = atariScreenMode256;       // 8-bit with 256 colour palette
    }
    else
    if ((b == 4) && !ip)
    {
        *mode = atariScreenMode16;       // 4-bit
    }
    else
    if ((b == 4) && ip)
    {
        *mode = atariScreenMode16ip;       // 4-bit interleaved plane
    }
    else
    if ((b == 2) && ip)
    {
        *mode = atariScreenMode4ip;       // 2-bit interleaved plane
    }
    else
    if ((b == 1) && !ip)
    {
        *mode = atariScreenMode2;       // monochrome
    }

    if (*mode < 0)
    {
        printf("unsupported colour mode\n");
        return 3;
    }

    *width = w;      // use instead of those in config file
    *height = h;

    return 0;
}



// return 0 if OK
static int eval_stretch(const char *stretch, int *stretch_x, int *stretch_y)
{
    unsigned x, y;

    int n = sscanf(stretch, "%ux%ux", &x, &y);
    if (n == 2)
    {
        // OK
    }
    else
    if (n == 1)
    {
        y = x;
    }
    else
    {
        printf("malformed stretch argument\n");
        return 3;
    }

    if ((x < 1) || (x > 8) ||
        (y < 1) || (x > 16))
    {
        printf("Invalid stretch factors\n");
        return 4;
    }

    *stretch_x = x;      // use instead of those in config file
    *stretch_y = y;

    return 0;
}


// return 0 if OK
static int eval_mouse_mode(const char *str, int *p_relative_mouse)
{
    if (!strcasecmp(str, "abs"))
    {
        *p_relative_mouse = 0;
        return 0;
    }
    else
    if (!strcasecmp(str, "rel"))
    {
        *p_relative_mouse = 1;
        return 0;
    }

    printf("Invalid mouse mode: %s\n", str);
    return 5;
}


// return 0 if OK
static int eval_memsize(const char *str, int *atari_memsize)
{
    unsigned m;
    unsigned factor = 1;
    char c;

    int n = sscanf(str, "%u%c", &m, &c);
    if (n == 2)
    {
        c = toupper(c);
        if (c == 'K')
        {
            factor = 1024;
        }
        else
        if (c == 'M')
        {
            factor = 1024 * 1024;
        }
        else
        {
            printf("memory size quantum may be k or m or nothing\n");
            return 1;
        }
    }
    else
    if (n != 1)
    {
        printf("malformed memory size argument\n");
        return 2;
    }

    m *= factor;

    if ((m < ATARI_RAM_SIZE_MIN) || (m > ATARI_RAM_SIZE_MAX))
    {
        printf("Invalid memory size %u: must be between %uk and %um\n", m, ATARI_RAM_SIZE_MIN >> 10, ATARI_RAM_SIZE_MAX >> 20);
        return 4;
    }

    *atari_memsize = m;      // use instead of those in config file

    return 0;
}


static int localise(const char *arg_lang)
{
    // We must get the preferences first to know the location of the Atari root file system.
    // Also we can set localisation from config file.
    // command line has precedence
    if (arg_lang == nullptr)
    {
        arg_lang = Preferences::AtariLanguage;
    }
    if ((arg_lang[0] != '\0') && (arg_lang[0] != '0'))
    {
        // Note that the shell script converts language code to uppercase.
        char cmd[PATH_MAX + 32];
        // cut long string to 10 characters, avoiding overflow
        sprintf(cmd, "%s/LANG/LOCALISE.SH %.10s", Preferences::AtariRootfsPath, arg_lang);
        //puts(cmd);
        //exit(0);
        int ret = system(cmd);
        if (ret != 0)
        {
            fputs("Localisation change failed\n", stderr);
            return 4;
        }
    }

    return 0;
}


int main(int argc, char *argv[])
{
    int c;
    const char *arg_geometry = nullptr;
    const char *arg_stretch = nullptr;
    const char *arg_mouse_mode = nullptr;
    const char *arg_memsize = nullptr;
    const char *arg_lang = nullptr;
    const char *arg_rootfs = nullptr;
    int mode = -1;
    int width = -1;
    int height = -1;
    int stretch_x = -1;
    int stretch_y = -1;
    int atari_memsize = -1;
    const char *config = DEFAULT_CONFIG;
    const char *editor_command = DEFAULT_EDITOR;    // xdg-open
    const char *file_a2h = nullptr;
    const char *file_h2a = nullptr;
    bool bRunEditor = false;
    bool bWriteConf = false;
    int relativeMouse = -1;     // -1: default

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
            {"geometry",     required_argument, nullptr, 'g' },
            {"stretch",      required_argument, nullptr, 's' },
            {"mouse-mode",   required_argument, nullptr,  0 },      // long_option_index 6
            {"memsize",      required_argument, nullptr, 'm' },
            {"lang",         required_argument, nullptr, 'l' },
            {"rootfs",       required_argument, nullptr, 'r' },
            {"editor",       required_argument, nullptr,  0 },      // long_option_index 10
            {"tconv-a2h",    required_argument, nullptr,  0 },      // long_option_index 11
            {"tconv-h2a",    required_argument, nullptr,  0 },      // long_option_index 12
            {nullptr,        0,                 nullptr,  0 }
        };
        c = getopt_long(argc, argv, "hc:ewg:s:m:l:r:",
                        long_options, &long_option_index);
        //printf("getopt_long() -> %d (c = '%c'), long_option_index = %d\n", c, c, long_option_index);

        if (c == -1)
        {
            // No more options. if optind < argc, then argv[optind ..]
            // are additional parameters
            break;
        }

        switch (c)
        {
            case 0:
                #if 0
                printf("option %d (%s)", long_option_index, long_options[long_option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                #endif
                if (long_option_index == 6)
                {
                    arg_mouse_mode = optarg;
                }
                else
                if (long_option_index == 10)
                {
                    editor_command = optarg;
                }
                else
                if (long_option_index == 11)
                {
                    file_a2h = optarg;
                }
                else
                if (long_option_index == 12)
                {
                    file_h2a = optarg;
                }
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

            case 'g':
                arg_geometry = optarg;
                break;

            case 's':
                arg_stretch = optarg;
                break;

            case 'm':
                arg_memsize = optarg;
                break;

            case 'l':
                arg_lang = optarg;
                break;

            case 'r':
                arg_rootfs = optarg;
                break;

            case 'w':
                bWriteConf = true;
                break;

            case '?':
                // parsing error
                return 1;
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }
    // exit(0);

    if (file_a2h != nullptr)
    {
        const char *outname = "/tmp/host.txt";
        char *buffer = nullptr;
        CConversion::init();
        CConversion::convTextFileAtari2Host(file_a2h, &buffer);
        if (buffer != nullptr)
        {
            unsigned len = strlen(buffer);
            FILE *f = fopen(outname, "wt");
            if (f != nullptr)
            {
                (void) fwrite(buffer, 1, len, f);
                fclose(f);
                printf("\"%s\" written\n", outname);
            }
        }
        return 0;
    }

    //file_h2a = "nothing";
    if (file_h2a != nullptr)
    {
        const char *outname = "/tmp/atari.txt";
        uint8_t *buffer = nullptr;
        CConversion::init();
        CConversion::convTextFileHost2Atari(file_h2a, &buffer);
        if (buffer != nullptr)
        {
            unsigned len = strlen((const char *) buffer);
            FILE *f = fopen(outname, "wt");
            if (f != nullptr)
            {
                (void) fwrite(buffer, 1, len, f);
                fclose(f);
                printf("\"%s\" written\n", outname);
            }
        return 0;
        }
    }

    if ((arg_memsize != nullptr) && eval_memsize(arg_memsize, &atari_memsize))
    {
        return 5;
    }

    //arg_geometry = "2ip";   // test code
    if ((arg_geometry != nullptr) && eval_geometry(arg_geometry, &mode, &width, &height))
    {
        return 3;
    }

    //stretch = "2x4";
    if ((arg_stretch != nullptr) && eval_stretch(arg_stretch, &stretch_x, &stretch_y))
    {
        return 4;
    }

    if ((arg_mouse_mode != nullptr) && eval_mouse_mode(arg_mouse_mode, &relativeMouse))
    {
        return 5;
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
        if ((mode != -1) || (width != -1) || (height = -1) ||
            (stretch_x != -1) || (stretch_y != -1) ||
            (atari_memsize != -1) || (arg_rootfs != nullptr))
        {
            printf("Just writing default values, additional options ignored!\n");
        }
        // just write defaults and ignore all other settings
        Preferences::init(config, -1, -1, -1, -1, -1, -1, -1, nullptr, true);
        return 0;
    }

    if (bRunEditor)
    {
        char command[1024];
        sprintf(command, "%s %s", editor_command, config);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-result"
        (void) system(command);
        #pragma GCC diagnostic pop
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
    if (Preferences::init(config, mode, width, height, stretch_x, stretch_y, relativeMouse, atari_memsize, arg_rootfs, false))
    {
        fputs("There were syntax errors in configuration file\n", stderr);
    }

    // We must get the preferences first to know the location of the Atari root file system.
    // Also we can set localisation from config file.
    // command line has precedence
    if (localise(arg_lang))
    {
        return 4;
    }

    CConversion::init();
    CMagiCPrint::init();
    CMagiCSerial::init();
    m68k_init();
    if (EmulationRunner::init())
    {
        return -1;
    }
    if (EmulationRunner::OpenWindow())
    {
        return -1;
    }
    EmulationRunner::StartEmulatorThread();
    EmulationRunner::EventLoop();
    CMagiCPrint::exit();
    CMagiCSerial::exit();

    return 0;
}
