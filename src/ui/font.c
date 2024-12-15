#define COBJMACROS
#include "../dx.h"
#include <math.h>
#include "font.h"
#include "../utils.h"
#include "../logging/logger.h"
#include "../utils.h"

#include <ft2build.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ttnameid.h>
#include FT_FREETYPE_H

// up to CACHE_MAX_SIZE fonts/size combinations can be stored in the cache
// 50 seems like a very reasonable number?
// The cache is a hash map, with the keys being the path to the font file and 
// size concatenated together, ie. 'fonts/Roboto-Regular.ttf20'
#define CACHE_MAX_SIZE 50

typedef struct {
    logger_t *log;
    FT_Library ftlib;

    size_t cache_size;
    uint32_t *cache_keys;
    ui_font_t **cache_fonts;

    ID3D12PipelineState *pso;
} ui_font_global_t;

static ui_font_global_t *fonts = NULL;

// Cached metrics for each rendered glyph
typedef struct glyph_metrics_t {
    double bearing_x;
    double bearing_y;
    double advance_x;
    int bitmap_width;
    int bitmap_rows;
    FT_UInt char_index;
} glyph_metrics_t;

// Glyphs are pre-rendered into OpenGL textures and then each glyph is drawn as
// a textured quad each frame. Unlike other rendered GUIs, the font textures are
// not baked up front. Each glyph is lazy loaded (rendered) as needed, with a
// predefined list of common glyphs loaded at initialization (preload_chars)
//
// The textures that hold the glyphs are GLYPH_TEX_SIZE (512x512). This means
// that larger font sizes may need multiple textures to hold glyphs, especially
// if additional glyphs outside the preload_chars are needed.
// This does mean that storage for particularly huge fonts becomes pretty
// inefficient (ie. 1 texture per glyph for anything over 256 pixels), but I
// expect that users won't be using fonts with a size over 100 pixels, much less
// 256.
//
// Each texture is stored as a layer in a 2D texture array. All glyphs are for
// a font are stored in a hash map (of the codepoint). The order the glyphs are
// rendered is the order in which they will appear in the texture array.
#define GLYPH_TEX_SIZE 512

struct ui_font_t {
    int size;
    FT_Face face;

    // the number of glyphs that can fit on a single layer of the texture array
    size_t page_max_glyphs;
    // the number of glyphs per row in the texture
    size_t page_glyph_x;

    // glyph codepoint hash map
    size_t glyphmap_capacity;
    size_t glyph_count;
    uint32_t *glyphs;

    // hash map contents
    size_t *glyph_index; // where is the glyph within the texture array?
    glyph_metrics_t *metrics;
    uint16_t *texture_num; // which layer in the array

    uint16_t texture_levels;
    dx_texture_t *texture;
};

ui_font_t *ui_font_new(const char *path, int size, int weight, int slant, int width);
void ui_font_free(ui_font_t *font);

static void ui_font_render_glyph(ui_font_t *font, uint32_t codepoint);

int ui_font_get_codepoint_ind(ui_font_t *font, uint32_t codepoint, size_t *ind);

static const char preload_chars[] = 
    " ~!@#$%^&*()_+`1234567890,.;:'\"-=\\|/?><[]{}"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void ui_font_init() {
    fonts = egoverlay_calloc(1, sizeof(ui_font_global_t));
    
    fonts->log = logger_get("ui-font");
    logger_debug(fonts->log, "init");

    FT_Error err = FT_Init_FreeType(&fonts->ftlib);

    if (err) {
        logger_error(fonts->log, "Couldn't initialize FreeType2.");
        error_and_exit("EG-Overlay: UI-Font", "Couldn't initialize FreeType2.");
    }

    size_t vertlen = 0;
    char *vertcso = load_file("shaders/font-quad.vs.cso", &vertlen);

    size_t pixellen = 0;
    char *pixelcso = load_file("shaders/font-quad.ps.cso", &pixellen);

    if (!vertcso || !pixelcso) {
        logger_error(fonts->log, "Couldn't load shaders.");
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

    fonts->pso = dx_create_pipeline_state(&psodesc);
    if (!fonts->pso) {
        logger_error(fonts->log, "Couldn't create pipeline state.");
        exit(-1);
    }
    dx_object_set_name(fonts->pso, "EG-Overlay D3D12 UI-Font Pipeline State");

    egoverlay_free(vertcso);
    egoverlay_free(pixelcso);

    fonts->cache_fonts = egoverlay_calloc(CACHE_MAX_SIZE, sizeof(ui_font_t*));
    fonts->cache_keys = egoverlay_calloc(CACHE_MAX_SIZE, sizeof(uint32_t)); 
}

void ui_font_cleanup() {
    logger_debug(fonts->log, "cleanup");
    for (size_t i=0;i<CACHE_MAX_SIZE;i++) {
        if (fonts->cache_keys[i]) ui_font_free(fonts->cache_fonts[i]);
    }
    egoverlay_free(fonts->cache_fonts);
    egoverlay_free(fonts->cache_keys);

    ID3D12PipelineState_Release(fonts->pso);

    FT_Done_FreeType(fonts->ftlib);
    egoverlay_free(fonts);
}

ui_font_t *ui_font_new(const char *path, int size, int weight, int slant, int width) {
    ui_font_t *font = egoverlay_calloc(1, sizeof(ui_font_t));

    FT_Error err = FT_New_Face(fonts->ftlib, path, 0, &font->face);
    if (err) {
        logger_error(fonts->log, "Couldn't load %s", path);
        return NULL;
    }

    FT_MM_Var *mm_var;
    if (!FT_Get_MM_Var(font->face, &mm_var)) {
        FT_Fixed *coords = egoverlay_calloc(mm_var->num_axis, sizeof(FT_Fixed));
        FT_Get_Var_Design_Coordinates(font->face, mm_var->num_axis, coords);

        for (FT_UInt a=0;a<mm_var->num_axis;a++) {
            if (weight!=INT_MIN && strcmp(mm_var->axis[a].name,"Weight")==0) {
                if (weight < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified weight (%d) below minimum %d",
                                path, weight, mm_var->axis[a].minimum / 0x10000);
                    weight = mm_var->axis[a].minimum / 0x10000;
                }
                if (weight > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified weight (%d) above minimum %d",
                                path, weight, mm_var->axis[a].maximum / 0x10000);
                    weight = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = weight * 0x10000;
            } else if (slant!=INT_MIN && strcmp(mm_var->axis[a].name,"Slant")==0) {
                if (slant < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified slant (%d) below minimum %d",
                                path, slant, mm_var->axis[a].minimum / 0x10000);
                    slant = mm_var->axis[a].minimum / 0x10000;
                }
                if (slant > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified weight (%d) above minimum %d",
                                path, slant, mm_var->axis[a].maximum / 0x10000);
                    slant = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = slant * 0x10000;
            } else if (width!=INT_MIN && strcmp(mm_var->axis[a].name,"Width")==0) {
                if (width < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified width (%d) below minimum %d",
                                path, width, mm_var->axis[a].minimum / 0x10000);
                    width = mm_var->axis[a].minimum / 0x10000;
                }
                if (width > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(fonts->log, " %s : specified weight (%d) above minimum %d",
                                path, width, mm_var->axis[a].maximum / 0x10000);
                    width = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = width * 0x10000;
            }
        }

        FT_Set_Var_Design_Coordinates(font->face, mm_var->num_axis, coords);
        FT_Done_MM_Var(fonts->ftlib, mm_var);
        egoverlay_free(coords);
    } else {
        logger_warn(fonts->log, "%s is not a variable font; weight, slant, and width will be ignored.", path);
    }

    err = FT_Set_Pixel_Sizes(font->face, 0, size);
    if (err) {
        logger_error(fonts->log, "Couldn't set size %d for %s.", size, path);
        error_and_exit("EG-OVerlay: UI-Font", "Couldn't set size %d for %s.", size, path);
    }
    
    int max_w = 0;
    int max_h = 0;

    max_w = FT_MulFix(font->face->bbox.xMax - font->face->bbox.xMin, font->face->size->metrics.x_scale);
    max_h = FT_MulFix(font->face->bbox.yMax - font->face->bbox.yMin, font->face->size->metrics.y_scale);

    font->size = ((max_w > max_h ? max_w : max_h) / 64);
    font->page_glyph_x = (uint32_t)GLYPH_TEX_SIZE / font->size;
    font->page_max_glyphs = font->page_glyph_x * font->page_glyph_x;

    // initial hashmap size of 256, enough for standard ascii and then some
    font->glyphmap_capacity = 256;
    font->glyphs = egoverlay_calloc(font->glyphmap_capacity, sizeof(uint32_t));
    font->metrics = egoverlay_calloc(font->glyphmap_capacity, sizeof(glyph_metrics_t));
    font->texture_num = egoverlay_calloc(font->glyphmap_capacity, sizeof(uint16_t));
    font->glyph_index = egoverlay_calloc(font->glyphmap_capacity, sizeof(size_t));

    logger_debug(fonts->log, "new font, %s size %d (%d), %d glyphs per page.",
                 path, size, font->size, font->page_max_glyphs);

    font->texture_levels = 1;

    font->texture = dx_texture_new_2d_array(DXGI_FORMAT_R8_UNORM, GLYPH_TEX_SIZE, GLYPH_TEX_SIZE, 1, 1);
    dx_texture_set_name(
        font->texture,
        "EG-Overlay D3D12 Font Texture: %s|%d|%d|%d|%d", path, size, weight, slant, width
    );

    size_t c = 0;
    while (preload_chars[c]) {
        ui_font_render_glyph(font, preload_chars[c++]);
    }

    return font;
}

void ui_font_free(ui_font_t *font) {
    dx_texture_free(font->texture);

    egoverlay_free(font->glyphs);
    egoverlay_free(font->metrics);
    egoverlay_free(font->texture_num);
    egoverlay_free(font->glyph_index);

    FT_Done_Face(font->face);

    egoverlay_free(font);
}

int ui_font_get_codepoint_ind(ui_font_t *font, uint32_t codepoint, size_t *ind) {
    size_t gind = codepoint % font->glyphmap_capacity;

    while (font->glyphs[gind]==0 || font->glyphs[gind]!=codepoint) {
        gind++;
        if (gind>=font->glyphmap_capacity) gind = 0;
        if (gind==codepoint % font->glyphmap_capacity) return 0;
    }

    *ind = gind;
    return 1;
}

static void ui_font_render_glyph(ui_font_t *font, uint32_t codepoint) {
    FT_UInt glyph = FT_Get_Char_Index(font->face, codepoint);

    FT_Error err = FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);
    if (err) {
        logger_error(fonts->log, "Couldn't load glyph for %c", codepoint);
        return;
    }

    FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL);

    if (font->glyph_count==font->glyphmap_capacity) {
        // hash map is full, make a bigger one
        size_t newcap = font->glyphmap_capacity + 128;
        uint32_t *newglyphs = calloc(newcap, sizeof(uint32_t));

        glyph_metrics_t *newmetrics = calloc(newcap, sizeof(glyph_metrics_t));
        uint16_t *newtexnums = calloc(newcap, sizeof(uint16_t));

        // move the existing glyphs into the new map
        for (size_t g=0;g<font->glyphmap_capacity;g++) {
            if (font->glyphs[g]==0) continue;

            size_t newind = font->glyphs[g] % newcap;

            while (newglyphs[newind]!=0) {
                newind++;
                if (newind>=newcap) newind = 0;
            }

            newglyphs[newind] = font->glyphs[g];
            memcpy(&newmetrics[newind], &font->metrics[g], sizeof(glyph_metrics_t));
            newtexnums[newind] = font->texture_num[g];
        }

        egoverlay_free(font->glyphs);
        egoverlay_free(font->metrics);
        egoverlay_free(font->texture_num);

        font->glyphmap_capacity = newcap;
        font->glyphs = newglyphs;
        font->metrics = newmetrics;
        font->texture_num = newtexnums;
    }
    
    size_t glyphind = codepoint % font->glyphmap_capacity;

    while (font->glyphs[glyphind]!=0) {
        glyphind++;
        if (glyphind>=font->glyphmap_capacity) glyphind = 0;
    }

    font->glyphs[glyphind] = codepoint;

    // cache glyph metrics
    font->metrics[glyphind].bearing_x = (double)font->face->glyph->metrics.horiBearingX / 64.0;
    font->metrics[glyphind].bearing_y = (double)font->face->glyph->metrics.horiBearingY / 64.0;
    font->metrics[glyphind].advance_x = (double)font->face->glyph->metrics.horiAdvance / 64.0;
    font->metrics[glyphind].bitmap_width = font->face->glyph->bitmap.width;
    font->metrics[glyphind].bitmap_rows = font->face->glyph->bitmap.rows;
    font->metrics[glyphind].char_index = FT_Get_Char_Index(font->face, codepoint);
    font->glyph_index[glyphind] = font->glyph_count;
    font->glyph_count++;

    if (font->glyph_count > font->page_max_glyphs * font->texture_levels) {
        // this glyph will spill over onto a new layer in the texture

        dx_texture_t *newtex = dx_texture_new_2d_array(
            DXGI_FORMAT_R8_UNORM,
            GLYPH_TEX_SIZE, GLYPH_TEX_SIZE, font->texture_levels+1, 1
        );
        dx_texture_copy_subresources(font->texture, newtex, font->texture_levels);

        font->texture_levels++;
        dx_texture_copy_name(font->texture, newtex);
        dx_texture_free(font->texture);
        font->texture = newtex;
    }

    font->texture_num[glyphind] = font->texture_levels - 1;

    FT_Bitmap bm = font->face->glyph->bitmap;

    if (bm.width==0 || bm.rows==0) return;
 
    // fist we need to gamma correct it
    uint8_t *pixels = egoverlay_calloc(bm.rows * bm.width, sizeof(uint8_t));
    for (size_t gy=0;gy<bm.rows;gy++) {
        for (size_t gx=0;gx<bm.width;gx++) {
            size_t goffset = (gy * bm.width) + gx;
            // gamma correction; first scale to 0..1
            double a = bm.buffer[goffset] / 255.0;
            // gamma = 2.2
            double ca = pow(a, 1/2.2);
            // the scale it back to 0..255 and store it
            pixels[goffset] = (uint8_t)ceil(ca * 255);
        }
    }

    // then copy the corrected bitmap into the texture
    uint32_t tex_x = (uint32_t)((font->glyph_index[glyphind] % font->page_glyph_x) * font->size);
    uint32_t tex_y = (uint32_t)(((font->glyph_index[glyphind] % font->page_max_glyphs) / font->page_glyph_x) * font->size);

    
    dx_texture_write_pixels(
        font->texture,
        tex_x, tex_y, font->texture_num[glyphind],
        bm.width, bm.rows,
        DXGI_FORMAT_R8_UNORM,
        pixels
    );
    
    egoverlay_free(pixels);
}

void ui_font_render_text(
    ui_font_t *font,
    mat4f_t *proj,
    int x,
    int y,
    const char *text,
    size_t count,
    ui_color_t color
) {
    dx_set_pipeline_state(fonts->pso);
    dx_set_texture(0, font->texture);

    float colorv[] = {
        UI_COLOR_R(color),
        UI_COLOR_G(color),
        UI_COLOR_B(color),
        UI_COLOR_A(color)
    };

    dx_set_root_constant_float4(0, colorv, 12);
    dx_set_root_constant_mat4f (0, proj  , 16);

    dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    // we get text as individual 8 bit characters, but we assume it's UTF-8.
    // each codepoint could be up to 4 bytes long.
    uint32_t codepoint = 0;
    int glyph_bytes = 0;
    int bytes_remaining = 0;

    float penx = (float)x;
    for (size_t c=0;c<count;c++) {
        // handle multi-byte UTF-8 characters
        if ((text[c] & 0x80)==0) {
            // single byte utf-8 character
            codepoint = text[c];
            bytes_remaining = 0;
        } else if ((text[c] & 0xF0) == 0xF0) {
            // 4 byte utf-8 character
            // first 3 bits of 21 total
            codepoint = (text[c] & 0x07) << 18;
            bytes_remaining = 3;
        } else if ((text[c] & 0xE0) == 0xE0) {
            // 3 byte utf-8 character
            // first 4 bits of 16
            codepoint = (text[c] & 0x0F) << 12;
            bytes_remaining = 2;
        } else if ((text[c] & 0xC0) == 0xC0) {
            // 2 byte utf-8 character
            // first 5 bits of 11
            codepoint = (text[c] & 0x1F) << 6;
            bytes_remaining = 1;
        } else if ((text[c] & 0x80) == 0x80) {
            // a byte in a multi-byte character
            int shift_bits = 6 * (bytes_remaining - 1);
            codepoint |= (text[c] & 0x3F) << shift_bits;
            bytes_remaining--;
        }
        glyph_bytes++;
        if (bytes_remaining) continue;

        size_t char_ind = 0;
        if (!ui_font_get_codepoint_ind(font, codepoint, &char_ind)) {
            ui_font_render_glyph(font, codepoint);
            c -= glyph_bytes;
            codepoint = 0;
            continue;
        }

        glyph = font->metrics[char_ind].char_index;

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        if (font->metrics[char_ind].bitmap_width == 0) {
            // this is an empty glyph, just move the pen position forward without rendering
            penx += (float)font->metrics[char_ind].advance_x;
            continue;
        }
        
        size_t glyph_ind = font->glyph_index[char_ind];

        float left = penx + (float)font->metrics[char_ind].bearing_x;
        float right = left + font->metrics[char_ind].bitmap_width;
        float top  = y + (font->face->size->metrics.ascender / 64.f) - (float)font->metrics[char_ind].bearing_y;
        float bottom = top + font->metrics[char_ind].bitmap_rows;
        
        float tex_left = ((float)(glyph_ind % font->page_glyph_x) * font->size);
        float tex_top  = ((float)((glyph_ind % font->page_max_glyphs)/ font->page_glyph_x) * font->size);
        float tex_right  = (tex_left + font->metrics[char_ind].bitmap_width);
        float tex_bottom = (tex_top + font->metrics[char_ind].bitmap_rows);
        float tex_layer = (float)font->texture_num[char_ind];

        dx_set_root_constant_float(0, left, 0);
        dx_set_root_constant_float(0, top , 1);
        dx_set_root_constant_float(0, right, 2);
        dx_set_root_constant_float(0, bottom, 3);
        dx_set_root_constant_float(0, tex_left, 4);
        dx_set_root_constant_float(0, tex_top , 5);
        dx_set_root_constant_float(0, tex_right, 6);
        dx_set_root_constant_float(0, tex_bottom, 7);
        dx_set_root_constant_float(0, tex_layer, 8);

        dx_draw_instanced(4, 1, 0, 0);

        penx += (float)font->metrics[char_ind].advance_x;
        prev_glyph = glyph;
        glyph_bytes = 0;
    }

}

ui_font_t *ui_font_get(const char *path, int size, int weight, int slant, int width) {
    if (fonts->cache_size == CACHE_MAX_SIZE) {
        logger_error(fonts->log, "Reached maximum cached fonts.");
        error_and_exit("EG-Overlay: UI-Font", "Reached maximum cached fonts.");
    }

    char buf[1024] = {0};
    sprintf_s(buf, 1024, "%s%d%d%d%d", path, size, weight, slant, width);
    uint32_t hash = djb2_hash_string(buf);
    size_t key_ind = hash % CACHE_MAX_SIZE;

    size_t n = fonts->cache_size;   
    
    while (n--) {
        if (fonts->cache_keys[key_ind]==hash) return fonts->cache_fonts[key_ind];
        key_ind++;
        if (key_ind==hash % CACHE_MAX_SIZE) break;
        if (key_ind>= CACHE_MAX_SIZE) key_ind = 0;
    }

    logger_debug(fonts->log, "Didn't find %s, creating.", buf);
    ui_font_t *f = ui_font_new(path, size, weight, slant, width);

    key_ind = hash % CACHE_MAX_SIZE;
    while (fonts->cache_keys[key_ind]!=hash){
        if (fonts->cache_keys[key_ind]) {
            key_ind++;
            if (key_ind>=CACHE_MAX_SIZE) key_ind = 0;
        } else {
            fonts->cache_keys[key_ind] = hash;
            fonts->cache_fonts[key_ind] = f;
        }
    }

    fonts->cache_size++;

    return f;
}

uint32_t ui_font_get_text_width(ui_font_t *font, const char *text, int count) {
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    int penx = 0;
    
    int glyph_bytes = 0;
    uint32_t codepoint = 0;
    int bytes_remaining = 0;

    for (size_t c=0;c<count;c++) {
        if ((text[c] & 0x80)==0) {
            // single byte utf-8 character
            codepoint = text[c];
            bytes_remaining = 0;
        } else if ((text[c] & 0xF0) == 0xF0) {
            // 4 byte utf-8 character
            // first 3 bits of 21 total
            codepoint = (text[c] & 0x07) << 18;
            bytes_remaining = 3;
        } else if ((text[c] & 0xE0) == 0xE0) {
            // 3 byte utf-8 character
            // first 4 bits of 16
            codepoint = (text[c] & 0x0F) << 12;
            bytes_remaining = 2;
        } else if ((text[c] & 0xC0) == 0xC0) {
            // 2 byte utf-8 character
            // first 5 bits of 11
            codepoint = (text[c] & 0x1F) << 6;
            bytes_remaining = 1;
        } else if ((text[c] & 0x80) == 0x80) {
            // a byte in a multi-byte character
            int shift_bits = 6 * (bytes_remaining - 1);
            codepoint |= (text[c] & 0x3F) << shift_bits;
            bytes_remaining--;
        }
        // track the number of bytes in each glyph
        // so that we can back that out of c below if the glyph needs to be
        // rendered first. otherwise a goto is needed, and no one likes those
        glyph_bytes++;
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);
        //FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);

        size_t char_ind = 0;
        if (!ui_font_get_codepoint_ind(font, codepoint, &char_ind)) {
            ui_font_render_glyph(font, codepoint);
            c -= glyph_bytes;
            codepoint = 0;
            continue;
        }

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        prev_glyph = glyph;
        glyph_bytes = 0;

        penx += (int)font->metrics[char_ind].advance_x;
    }

    return penx;
}

uint32_t ui_font_get_index_of_width(ui_font_t *font, const char *text, int width) {
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    int penx = 0;
    
    int glyph_bytes = 0;
    uint32_t codepoint = 0;
    int bytes_remaining = 0;

    uint32_t c;

    for (c=0;c<strlen(text);c++) {
        if ((text[c] & 0x80)==0) {
            // single byte utf-8 character
            codepoint = text[c];
            bytes_remaining = 0;
        } else if ((text[c] & 0xF0) == 0xF0) {
            // 4 byte utf-8 character
            // first 3 bits of 21 total
            codepoint = (text[c] & 0x07) << 18;
            bytes_remaining = 3;
        } else if ((text[c] & 0xE0) == 0xE0) {
            // 3 byte utf-8 character
            // first 4 bits of 16
            codepoint = (text[c] & 0x0F) << 12;
            bytes_remaining = 2;
        } else if ((text[c] & 0xC0) == 0xC0) {
            // 2 byte utf-8 character
            // first 5 bits of 11
            codepoint = (text[c] & 0x1F) << 6;
            bytes_remaining = 1;
        } else if ((text[c] & 0x80) == 0x80) {
            // a byte in a multi-byte character
            int shift_bits = 6 * (bytes_remaining - 1);
            codepoint |= (text[c] & 0x3F) << shift_bits;
            bytes_remaining--;
        }
        glyph_bytes++;
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);
        //FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);

        size_t char_ind = 0;
        if (!ui_font_get_codepoint_ind(font, codepoint, &char_ind)) {
            ui_font_render_glyph(font, codepoint);
            c -= glyph_bytes;
            codepoint = 0;
            continue;
        }
        
        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }
        prev_glyph = glyph;

        penx += (int)font->metrics[char_ind].advance_x;
        glyph_bytes = 0;

        if (penx > width) return c;
    }

    return c;
}

uint32_t ui_font_get_text_height(ui_font_t *font) {
    //int asc = FT_MulFix(font->face->ascender, font->face->size->metrics.y_scale);
    //int desc = FT_MulFix(font->face->descender, font->face->size->metrics.y_scale);

    //return (asc - desc) / 64;
    return (font->face->size->metrics.ascender - font->face->size->metrics.descender)  / 64;
}

uint32_t ui_font_get_line_spacing(ui_font_t *font) {
    return font->face->size->metrics.height / 64;
}


