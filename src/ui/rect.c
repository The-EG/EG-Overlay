//#include "../gl.h"
#define COBJMACROS
#include "../dx.h"
#include "ui.h"
#include "../logging/logger.h"
#include "rect.h"
#include "../utils.h"
#include "../lamath.h"
#include <lauxlib.h>

typedef struct {
    logger_t *log;

    ID3D12PipelineState *pso;
} ui_rect_t;
ui_rect_t *rect = NULL;

void ui_rect_init() {
    rect = egoverlay_calloc(1, sizeof(ui_rect_t));
    rect->log = logger_get("ui-rect");

    logger_debug(rect->log, "init");
    
    size_t vertlen = 0;
    char *vertcso = load_file("shaders/rect.vs.cso", &vertlen);

    size_t pixellen = 0;
    char *pixelcso = load_file("shaders/rect.ps.cso", &pixellen);

    if (!vertcso || !pixelcso) {
        logger_error(rect->log, "Couldn't load shaders.");
        exit(-1);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psodesc = {0};

    psodesc.VS.pShaderBytecode = vertcso;
    psodesc.VS.BytecodeLength  = vertlen;
    psodesc.PS.pShaderBytecode = pixelcso;
    psodesc.PS.BytecodeLength  = pixellen;

    psodesc.RasterizerState.FillMode             = D3D12_FILL_MODE_SOLID;
    psodesc.RasterizerState.CullMode             = D3D12_CULL_MODE_NONE;
    psodesc.RasterizerState.DepthBias            = D3D12_DEFAULT_DEPTH_BIAS;
    psodesc.RasterizerState.DepthBiasClamp       = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psodesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psodesc.RasterizerState.DepthClipEnable      = 1;
    psodesc.RasterizerState.ConservativeRaster   = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    psodesc.BlendState.RenderTarget[0].BlendEnable           = 1;
    psodesc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
    psodesc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    psodesc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
    psodesc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
    psodesc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    psodesc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    psodesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psodesc.DepthStencilState.DepthEnable   = 0;
    psodesc.DepthStencilState.StencilEnable = 0;

    psodesc.SampleMask = UINT_MAX;
    psodesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psodesc.NumRenderTargets = 1;
    psodesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psodesc.SampleDesc.Count = 1;

    rect->pso = dx_create_pipeline_state(&psodesc);
    if (!rect->pso) {
        logger_error(rect->log, "Couldn't create pipeline state.");
        exit(-1);
    }
    dx_object_set_name(rect->pso, "EG-Overlay D3D12 UI-Rect Pipeline State");

    egoverlay_free(vertcso);
    egoverlay_free(pixelcso);
}

void ui_rect_cleanup() {
    logger_debug(rect->log, "cleanup");

    ID3D12PipelineState_Release(rect->pso);
    
    egoverlay_free(rect);    
}

void ui_rect_draw(int x, int y, int width, int height, ui_color_t color, mat4f_t *proj) {
    float left = (float)x;
    float top = (float)y;
    float right = (float)x + width;
    float bottom = (float)y + height;

    float colorv[] = {
        UI_COLOR_R(color),
        UI_COLOR_G(color),
        UI_COLOR_B(color),
        UI_COLOR_A(color)
    };

    dx_set_pipeline_state(rect->pso);

    dx_set_root_constant_float (0, left  , 0);
    dx_set_root_constant_float (0, top   , 1);
    dx_set_root_constant_float (0, right , 2);
    dx_set_root_constant_float (0, bottom, 3);
    dx_set_root_constant_float4(0, colorv, 4);
    dx_set_root_constant_mat4f (0, proj  , 8);

    dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    dx_draw_instanced(4, 1, 0, 0);
}

void ui_rect_draw_multi(size_t count, ui_rect_multi_t *rects, mat4f_t *proj) {
    dx_set_pipeline_state(rect->pso);

    dx_set_root_constant_mat4f(0, proj, 8);

    dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    for (size_t r=0;r<count;r++) {
        float left = (float)rects[r].x;
        float top = (float)rects[r].y;
        float right = (float)rects[r].x + rects[r].width;
        float bottom = (float)rects[r].y + rects[r].height;

        float colorv[] = {
            UI_COLOR_R(rects[r].color),
            UI_COLOR_G(rects[r].color),
            UI_COLOR_B(rects[r].color),
            UI_COLOR_A(rects[r].color)
        };

        dx_set_root_constant_float (0, left  , 0);
        dx_set_root_constant_float (0, top   , 1);
        dx_set_root_constant_float (0, right , 2);
        dx_set_root_constant_float (0, bottom, 3);
        dx_set_root_constant_float4(0, colorv, 4);
     
        dx_draw_instanced(4, 1, 0, 0);
    }
}
