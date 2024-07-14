#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <windows.h>
#include "app.h"
#include "utils.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevIstance, LPSTR lpCmdLine, int nShowCmd) {
    UNUSED_PARAM(nShowCmd);
    UNUSED_PARAM(hPrevIstance);
    UNUSED_PARAM(lpCmdLine);

    app_init(hInstance, __argc, __argv);
    app_run();
    app_cleanup();

    return 0;
}