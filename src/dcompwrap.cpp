#include "dcompwrap.h"

extern "C" {
    #include "logging/logger.h"
}

#include <dcomp.h>

extern "C" {

void dx_setup_dcomp(
    HWND hwnd,
    IUnknown *swap_chain,
    IUnknown **dcomp_dev,
    IUnknown **dcomp_target,
    IUnknown **dcomp_visual
) {
    logger_t *log = logger_get("dx");

    IDCompositionDevice *dev    = NULL;
    IDCompositionTarget *target = NULL;
    IDCompositionVisual *visual = NULL;

    logger_debug(log, "Setting up DirectComposition...");

    if (DCompositionCreateDevice(NULL, IID_PPV_ARGS(&dev))!=S_OK) {
        logger_error(log, "Couldn't create DirectComposition device.");
        exit(-1);
    }
    *dcomp_dev = dev;

    if (dev->CreateTargetForHwnd(hwnd, true, &target)!=S_OK) {
        logger_error(log, "Couldn't create DirectComposition target.");
        exit(-1);
    }
    *dcomp_target = dev;

    if (dev->CreateVisual(&visual)!=S_OK) {
        logger_error(log, "Couldn't create DirectComposition visual.");
        exit(-1);
    }
    *dcomp_visual = visual;

    if (visual->SetContent(swap_chain)!=S_OK) {
        logger_error(log, "Couldn't set DirectComposition visual content.");
        exit(-1);
    }

    if (target->SetRoot(visual)!=S_OK) {
        logger_error(log, "Couldn't set DirectComposition target root.");
        exit(-1);
    }

    if (dev->Commit()!=S_OK) {
        logger_error(log, "Couldn't commit DirectComposition device.");
        exit(-1);
    }
}

}
