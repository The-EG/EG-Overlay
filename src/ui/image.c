#define COBJMACROS
#include <wincodec.h>
#include "image.h"
#include "../logging/logger.h"
#include "../dx.h"
#include "ui.h"
#include "../lamath.h"
#include "../utils.h"
#include <lauxlib.h>

typedef struct {
    ui_element_t element;

    struct {
        float r;
        float g;
        float b;
        float a;
    } color;

    uint32_t imgwidth;
    uint32_t imgheight;

    uint32_t prefwidth;
    uint32_t prefheight;

    dx_texture_t *texture;
    float max_u;
    float max_v;
    float xy_ratio;
} ui_image_t;

void ui_image_free(ui_image_t *img);
void ui_image_draw(ui_image_t *img, int offset_x, int offset_y, mat4f_t *proj);
int ui_image_get_preferred_size(ui_image_t *img, int *width, int *height);

typedef struct {
    logger_t *log;

    ID3D12PipelineState *pso;
} ui_image_static_t;

ui_image_static_t *imgstatic = NULL;

void ui_image_init() {
    imgstatic = egoverlay_calloc(1, sizeof(ui_image_static_t));
    imgstatic->log = logger_get("ui-image");

    logger_debug(imgstatic->log, "init");

    size_t vertlen = 0;
    char *vertcso = load_file("shaders/image.vs.cso", &vertlen);

    size_t pixellen = 0;
    char *pixelcso = load_file("shaders/image.ps.cso", &pixellen);

    if (!vertcso || !pixelcso) {
        logger_error(imgstatic->log, "Couldn't load shaders.");
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

    imgstatic->pso = dx_create_pipeline_state(&psodesc);
    if (!imgstatic->pso) {
        logger_error(imgstatic->log, "Couldn't create pipeline state.");
        exit(-1);
    }
    dx_object_set_name(imgstatic->pso, "EG-Overlay D3D12 UI-Image Pipeline State");

    egoverlay_free(vertcso);
    egoverlay_free(pixelcso);
}

void ui_image_cleanup() {
    logger_debug(imgstatic->log, "cleanup");

    ID3D12PipelineState_Release(imgstatic->pso);

    egoverlay_free(imgstatic);
}

void ui_image_free(ui_image_t *img) {
    dx_texture_free(img->texture);

    egoverlay_free(img);
}

void ui_image_draw(ui_image_t *img, int offset_x, int offset_y, mat4f_t *proj) {
    dx_set_pipeline_state(imgstatic->pso);
    dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    float left   = (float)img->element.x + offset_x;
    float top    = (float)img->element.y + offset_y;
    float right  = left + img->element.width;
    float bottom = top  + img->element.height;

    dx_set_root_constant_float (0, left  , 0);
    dx_set_root_constant_float (0, top   , 1);
    dx_set_root_constant_float (0, right , 2);
    dx_set_root_constant_float (0, bottom, 3);

    dx_set_root_constant_float4(0, (float*)&img->color, 4);

    dx_set_root_constant_mat4f (0, proj, 8);

    dx_set_root_constant_float (0, img->max_u, 24);
    dx_set_root_constant_float (0, img->max_v, 25);

    dx_set_texture(0, img->texture);

    dx_draw_instanced(4, 1, 0, 0);
}

int ui_image_get_preferred_size(ui_image_t *img, int *width, int *height) {
    *width = img->prefwidth;
    *height = img->prefheight;

    return 1;
}

int ui_image_lua_new(lua_State *L);
int ui_image_lua_del(lua_State *L);
int ui_image_lua_size(lua_State *L);

#define lua_checkuiimage(L, ind) *(ui_image_t**)luaL_checkudata(L, ind, "UIImage")

luaL_Reg imagefuncs[] = {
    "__gc", &ui_image_lua_del,
    "size", &ui_image_lua_size,
    NULL  ,  NULL
};

/*** RST
Images
======

.. lua:currentmodule:: eg-overlay-ui

Functions
---------
*/

void ui_image_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_image_lua_new);
    lua_setfield(L, -2, "image");
}

/*** RST
.. lua:function:: image(imagedata)

    Create a new :lua:class:`uiimage`.

    .. note::
        
        ``imagedata`` should be data loaded from an image file and can be any
        format supported by the 
        `Windows Imaging Component <https://learn.microsoft.com/en-us/windows/win32/wic/-wic-lh>`_.
*/
int ui_image_lua_new(lua_State *L) {
    size_t datalen = 0;
    const uint8_t *data = (const uint8_t*)luaL_checklstring(L, 1, &datalen);

    IWICImagingFactory *wicfactory = NULL;

    if (CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &wicfactory)!=S_OK) {
        return luaL_error(L, "Couldn't create WIC factory.");
    }

    IWICStream            *memstream = NULL;
    IWICBitmapDecoder     *decoder   = NULL;
    IWICBitmapFrameDecode *frame     = NULL;
    IWICFormatConverter   *converter = NULL;
    IWICBitmap            *bitmap    = NULL;
    IWICBitmapLock        *bmplock   = NULL;
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t pixels_size = 0;
    uint8_t *pixels = NULL;

    if (IWICImagingFactory_CreateStream(wicfactory, &memstream)!=S_OK) {
        return luaL_error(L, "Couldn't create a WIC stream.");
    }

    if (IWICStream_InitializeFromMemory(memstream, (uint8_t*)data, (DWORD)datalen)!=S_OK) {
        return luaL_error(L, "Couldn't initialize texture stream.");
    }

    if (IWICImagingFactory_CreateDecoderFromStream(wicfactory, (IStream*)memstream, NULL, WICDecodeMetadataCacheOnDemand, &decoder)!=S_OK) {
        return luaL_error(L, "Couldn't get image decoder.");
    }

    if (IWICBitmapDecoder_GetFrame(decoder, 0, &frame)!=S_OK) {
        return luaL_error(L, "Couldn't get image frame.");
    }

    if (IWICImagingFactory_CreateFormatConverter(wicfactory, &converter)!=S_OK) {
        return luaL_error(L, "Couldn't create image format converter.");
    }

    if (IWICFormatConverter_Initialize(converter, (IWICBitmapSource*)frame, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom)!=S_OK) {
        return luaL_error(L, "Couldn't initialize image converter.");
    }

    if (IWICImagingFactory_CreateBitmapFromSource(wicfactory, (IWICBitmapSource*)converter, WICBitmapCacheOnDemand, &bitmap)!=S_OK) {
        return luaL_error(L, "Couldn't create WIC bitmap.");
    }

    if (IWICBitmap_GetSize(bitmap, &width, &height)!=S_OK) {
        return luaL_error(L, "Couldn't get bitmap size.");
    }

    WICRect lockrect = {0, 0, width, height};

    if (IWICBitmap_Lock(bitmap, &lockrect, WICBitmapLockRead, &bmplock)!=S_OK) {
        return luaL_error(L, "Couldn't lock bitmap.");
    }

    if (IWICBitmapLock_GetDataPointer(bmplock, &pixels_size, &pixels)!=S_OK) {
        return luaL_error(L, "Couldnt get bitmap data pointer.");
    }

    uint32_t req_size = 1;
    while (req_size < width || req_size < height) req_size <<= 1;

    ui_image_t *img = egoverlay_calloc(1, sizeof(ui_image_t));

    img->element.draw               = &ui_image_draw;
    img->element.get_preferred_size = &ui_image_get_preferred_size;
    img->element.free               = &ui_image_free;

    img->imgwidth   = width;
    img->imgheight  = height;
    img->prefwidth  = width;
    img->prefheight = height;

    img->color.r = 1.f;
    img->color.g = 1.f;
    img->color.b = 1.f;
    img->color.a = 1.f;
    
    img->xy_ratio = (float)width / (float)height;

    img->max_u  = (float)width  / (float)req_size;
    img->max_v  = (float)height / (float)req_size;

    img->texture = dx_texture_new_2d(DXGI_FORMAT_B8G8R8A8_UNORM, req_size, req_size, 1);
    dx_texture_set_name(img->texture, "EG-Overlay D3D12 UI-Image Texture");

    dx_texture_write_pixels(
        img->texture,
        0, 0, 0,
        width, height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        pixels
    );
    IWICBitmapLock_Release(bmplock);
    IWICBitmap_Release(bitmap);
    IWICFormatConverter_Release(converter);
    IWICBitmapFrameDecode_Release(frame);
    IWICStream_Release(memstream);
    IWICImagingFactory_Release(wicfactory);

    ui_element_ref(img);

    ui_image_t **pimg = lua_newuserdata(L, sizeof(ui_image_t*));
    *pimg = img;
    
    if (luaL_newmetatable(L, "UIImage")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__is_uielement");

        luaL_setfuncs(L, imagefuncs, 0);
    }

    lua_setmetatable(L, -2);

    return 1;
}

int ui_image_lua_del(lua_State *L) {
    ui_image_t *img = lua_checkuiimage(L, 1);
    ui_element_unref(img);

    return 0;
}

int ui_image_lua_size(lua_State *L) {
    return luaL_error(L, "Not implemented.");
}
