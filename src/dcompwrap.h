#pragma once

#include <windows.h>
#include "unknwn.h"

#ifdef __cplusplus
extern "C"
#endif
void dx_setup_dcomp(
    HWND hwnd,
    IUnknown *swap_chain,
    IUnknown **dcomp_dev,
    IUnknown **dcomp_target,
    IUnknown **dcomp_visual
);
