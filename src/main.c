#include <crtdbg.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <windows.h>
#include "app.h"
#include "utils.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevIstance, LPSTR lpCmdLine, int nShowCmd) {
    UNUSED_PARAM(nShowCmd);
    UNUSED_PARAM(hPrevIstance);
    UNUSED_PARAM(lpCmdLine);

    // check for memory leaks on program exit when debugging
    int dbgflag = 0;
    dbgflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

    dbgflag |= _CRTDBG_LEAK_CHECK_DF;

    _CrtSetDbgFlag(dbgflag);

    app_init(hInstance, __argc, __argv);
    app_run();
    app_cleanup();

    return 0;
}
