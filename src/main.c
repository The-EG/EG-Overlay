#include <crtdbg.h>
#include <windows.h>
#include "utils.h"

#include "app.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    UNUSED_PARAM(nShowCmd);
    UNUSED_PARAM(hPrevInstance);
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
