#include <stdio.h>
#include "Debug.h"
#include "preferences.h"
#include "EmulationMain.h"




int main(int argc, const char *argv[])
{
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
    Preferences::Init(false);
    EmulationInit();
    EmulationOpenWindow();
    EmulationRun();
    EmulationRunSdl();
}
