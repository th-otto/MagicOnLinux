#include <stdio.h>
#include <stdlib.h>
#include "Debug.h"
#include "conversion.h"
#include "preferences.h"
#include "MagiCPrint.h"
#include "MagiCSerial.h"
#include "EmulationMain.h"




int main(int argc, const char *argv[])
{
    if ((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    {
        fputs("Usage:\n"
              "  -h or --help\n"
              "  -rewrite_conf\n"
              "  -open_conf\n",
            stderr);
        return 0;
    }

    if ((argc > 1) && (!strcmp(argv[1], "-open_conf")))
    {
        system("gnome-text-editor ~/.config/magiclinux.conf &");
        return 0;
    }

    if ((argc > 1) && (!strcmp(argv[1], "-rewrite_conf")))
    {
        Preferences::Init(true);
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

    DebugInit(NULL /* stderr */);
    if (Preferences::Init(false))
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
