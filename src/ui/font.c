#include <math.h>
#include "font.h"
#include "../utils.h"
#include "../logging/helpers.h"
#include "../gl.h"
#include "../utils.h"

#include <ft2build.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ttnameid.h>
#include FT_FREETYPE_H

static logger_t *logger = NULL;

static gl_shader_program_t *shader_program = NULL;

static GLuint vao = 0;

static FT_Library ftlib = NULL;

// up to CACHE_MAX_SIZE fonts/size combinations can be stored in the cache
// 50 seems like a very reasonable number?
// The cache is a hash map, with the keys being the path to the font file and 
// size concatonated together, ie. 'fonts/Roboto-Regular.ttf20'
#define CACHE_MAX_SIZE 50
static size_t cache_size = 0;
static uint32_t *cache_keys = NULL;
static ui_font_t **cache_fonts = NULL;

// Cached metrics for each rendered glyph
typedef struct glyph_metrics_t {
    double bearing_x;
    double bearing_y;
    double advance_x;
    int bitmap_width;
    int bitmap_rows;
} glyph_metrics_t;

// Glyphs are prerendered into OpenGL textures and then each glyph is drawn as
// a textured quad each frame. Unlike other rendered GUIs, the font textures are
// not baked up front. Each glyph is lazy loaded (rendered) as needed, with a
// predfined list of common glyphs loaded at initialization (preload_chars)
//
// The textures that hold the glyphs are GLYPH_TEX_SIZE (512x512). This means
// that larger font sizes may need multiple textures to hold glyphs, especially
// if additional glyphs outside the preload_chars are needed.
// This does mean that storage for particularly huge fonts becomes pretty
// inefficient (ie. 1 texture per glyph for anything over 256 pixels), but I
// expect that users won't be using fonts with a size over 100 pixels, much less
// 256.
//
// Each texture is stored in a 'glyph page.' This page has the GL texture, the
// raw pixel data (8bit alpha mask), a hash map of the glyphs on the page,
// and an array of the cached glyph metrics defined above, which are stored as
// each glyph is rendered.
// The index of the the glyph within the hash map also indicates the glyph's
// position within the texture with 0 being the top left corner, 1 to the right
// of that, etc.
//
// Once a page fills up a new page will be started the next time a new glyph
// needs to be rendered.
typedef struct glyph_page_t {
    GLuint texture;
    uint8_t *pixels;
    size_t page_glyph_count;

    // glyph hash map
    uint32_t *glyphs;

    // table of glyph metrics, indexed by the hash map above
    glyph_metrics_t *metrics;

    struct glyph_page_t *next;
} glyph_page_t;

#define GLYPH_TEX_SIZE 512

struct ui_font_t {
    int size;
    FT_Face face;

    size_t page_max_glyphs;
    size_t page_glyph_x;

    glyph_page_t *glyph_pages;
};

ui_font_t *ui_font_new(const char *path, int size, int weight, int slant, int width);
void ui_font_free(ui_font_t *font);

static glyph_page_t *ui_font_init_page(ui_font_t *font);
static void ui_font_render_glyph(ui_font_t *font, uint32_t codepoint);
static void ui_font_update_textures(ui_font_t *font);

static const char preload_chars[] = 
    " ~!@#$%^&*()_+`1234567890,.;:'\"-=\\|/?><[]{}"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void ui_font_init() {
    logger = logger_get("ui-font");
    logger_debug(logger, "init");

    FT_Error err = FT_Init_FreeType(&ftlib);

    if (err) {
        logger_error(logger, "Couldn't initialize FreeType2.");
        error_and_exit("EG-Overlay: UI-Font", "Couldn't initialize FreeType2.");
    }

    shader_program = gl_shader_program_new();
    gl_shader_program_attach_shader_file(shader_program, "shaders/text-quad.vert", GL_VERTEX_SHADER);
    gl_shader_program_attach_shader_file(shader_program, "shaders/text-quad.frag", GL_FRAGMENT_SHADER);
    gl_shader_program_link(shader_program);

    glGenVertexArrays(1, &vao);

    cache_fonts = egoverlay_calloc(CACHE_MAX_SIZE, sizeof(ui_font_t*));
    cache_keys = egoverlay_calloc(CACHE_MAX_SIZE, sizeof(uint32_t));
}

void ui_font_cleanup() {
    logger_debug(logger, "cleanup");
    for (size_t i=0;i<CACHE_MAX_SIZE;i++) {
        if (cache_keys[i]) ui_font_free(cache_fonts[i]);
    }
    egoverlay_free(cache_fonts);
    egoverlay_free(cache_keys);

    gl_shader_program_free(shader_program);

    glDeleteVertexArrays(1, &vao);

    FT_Done_FreeType(ftlib);
}

ui_font_t *ui_font_new(const char *path, int size, int weight, int slant, int width) {
    ui_font_t *font = egoverlay_calloc(1, sizeof(ui_font_t));

    FT_Error err = FT_New_Face(ftlib, path, 0, &font->face);
    if (err) {
        logger_error(logger, "Couldn't load %s", path);
        //error_and_exit("EG-Overlay: UI-Font", "Couldn't load font %s", path);
        return NULL;
    }

    FT_MM_Var *mm_var;
    if (!FT_Get_MM_Var(font->face, &mm_var)) {
        FT_Fixed *coords = egoverlay_calloc(mm_var->num_axis, sizeof(FT_Fixed));
        FT_Get_Var_Design_Coordinates(font->face, mm_var->num_axis, coords);

        for (FT_UInt a=0;a<mm_var->num_axis;a++) {
            if (weight!=INT_MIN && strcmp(mm_var->axis[a].name,"Weight")==0) {
                if (weight < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(logger, " %s : specified weight (%d) below minimum %d", path, weight, mm_var->axis[a].minimum / 0x10000);
                    weight = mm_var->axis[a].minimum / 0x10000;
                }
                if (weight > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(logger, " %s : specified weight (%d) above minimum %d", path, weight, mm_var->axis[a].maximum / 0x10000);
                    weight = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = weight * 0x10000;
            } else if (slant!=INT_MIN && strcmp(mm_var->axis[a].name,"Slant")==0) {
                if (slant < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(logger, " %s : specified slant (%d) below minimum %d", path, slant, mm_var->axis[a].minimum / 0x10000);
                    slant = mm_var->axis[a].minimum / 0x10000;
                }
                if (slant > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(logger, " %s : specified weight (%d) above minimum %d", path, slant, mm_var->axis[a].maximum / 0x10000);
                    slant = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = slant * 0x10000;
            } else if (width!=INT_MIN && strcmp(mm_var->axis[a].name,"Width")==0) {
                if (width < mm_var->axis[a].minimum / 0x10000) {
                    logger_warn(logger, " %s : specified width (%d) below minimum %d", path, width, mm_var->axis[a].minimum / 0x10000);
                    width = mm_var->axis[a].minimum / 0x10000;
                }
                if (width > mm_var->axis[a].maximum / 0x10000) {
                    logger_warn(logger, " %s : specified weight (%d) above minimum %d", path, width, mm_var->axis[a].maximum / 0x10000);
                    width = mm_var->axis[a].maximum / 0x10000;
                }
                coords[a] = width * 0x10000;
            }
        }

        FT_Set_Var_Design_Coordinates(font->face, mm_var->num_axis, coords);
        FT_Done_MM_Var(ftlib, mm_var);
        egoverlay_free(coords);
    } else {
        logger_warn(logger, "%s is not a variable font; weight, slant, and width will be ignored.", path);
    }

    err = FT_Set_Pixel_Sizes(font->face, 0, size);
    if (err) {
        logger_error(logger, "Couldn't set size %d for %s.", size, path);
        error_and_exit("EG-OVerlay: UI-Font", "Couldn't set size %d for %s.", size, path);
    }
    
    int max_w = 0;
    int max_h = 0;

    max_w = FT_MulFix(font->face->bbox.xMax - font->face->bbox.xMin, font->face->size->metrics.x_scale);
    max_h = FT_MulFix(font->face->bbox.yMax - font->face->bbox.yMin, font->face->size->metrics.y_scale);

    font->size = ((max_w > max_h ? max_w : max_h) / 64);
    font->page_glyph_x = (uint32_t)GLYPH_TEX_SIZE / font->size;
    font->page_max_glyphs = font->page_glyph_x * font->page_glyph_x;

    logger_debug(logger, "new font, %s size %d (%d), %d glyphs per page.", path, size, font->size, font->page_max_glyphs);

    font->glyph_pages = ui_font_init_page(font);

    size_t c = 0;
    while (preload_chars[c]) {
        ui_font_render_glyph(font, preload_chars[c++]);
    }

    ui_font_update_textures(font);

    return font;
}

void ui_font_free(ui_font_t *font) {
    glyph_page_t *p = font->glyph_pages;
    while (p) {
        glDeleteTextures(1, &p->texture);
        egoverlay_free(p->glyphs);
        egoverlay_free(p->pixels);
        egoverlay_free(p->metrics);
        glyph_page_t *z = p;
        p = z->next;
        egoverlay_free(z);
    }

    FT_Done_Face(font->face);

    egoverlay_free(font);
}

static glyph_page_t *ui_font_init_page(ui_font_t *font) {
    glyph_page_t *page = egoverlay_calloc(1, sizeof(glyph_page_t));
    page->glyphs = egoverlay_calloc(font->page_max_glyphs, sizeof(uint32_t));
    page->pixels = egoverlay_calloc((GLYPH_TEX_SIZE * GLYPH_TEX_SIZE), sizeof(uint8_t));
    page->metrics = egoverlay_calloc(font->page_max_glyphs, sizeof(glyph_metrics_t));

    glGenTextures(1, &page->texture);

    glBindTexture(GL_TEXTURE_RECTANGLE, page->texture);

    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_RECTANGLE, 0);

    return page;
}

static void ui_font_render_glyph(ui_font_t *font, uint32_t codepoint) {
    FT_UInt glyph = FT_Get_Char_Index(font->face, codepoint);

    FT_Error err = FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);
    if (err) {
        logger_error(logger, "Couldn't load glyph for %c", codepoint);
        return;
    }

    FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL);

    size_t page_num = 1;
    glyph_page_t *last_page = font->glyph_pages;
    while(last_page->next) {
        last_page = last_page->next;
        page_num++;
    }

    if (last_page->page_glyph_count==font->page_max_glyphs) {
        glyph_page_t *new_page = ui_font_init_page(font);
        last_page->next = new_page;
        last_page = new_page;
        page_num++;
    }

    // hash index of the codepoint
    size_t new_ind = codepoint % font->page_max_glyphs;

    // in case of hash colission use a linear probe to find the next empty slot, increasing. wrap around if we reach the end
    while (last_page->glyphs[new_ind]) {
        new_ind++;
        if (new_ind>=font->page_max_glyphs) new_ind = 0;
        if (new_ind==codepoint % font->page_max_glyphs) {
            logger_error(logger, "couldn't resolve hash collision!");
            error_and_exit("EG-Overlay: UI-Font", "Couldn't resolve hash collision!");
        }
    }

    last_page->glyphs[new_ind] = codepoint;

    // cache glyph metrics
    last_page->metrics[new_ind].bearing_x = (double)font->face->glyph->metrics.horiBearingX / 64.0;
    last_page->metrics[new_ind].bearing_y = (double)font->face->glyph->metrics.horiBearingY / 64.0;
    last_page->metrics[new_ind].advance_x = (double)font->face->glyph->metrics.horiAdvance / 64.0;
    last_page->metrics[new_ind].bitmap_width = font->face->glyph->bitmap.width;
    last_page->metrics[new_ind].bitmap_rows = font->face->glyph->bitmap.rows;
    last_page->page_glyph_count++;

    //logger_debug(logger, "Rendering %c, page %d, hashed_id %d.", codepoint, page_num, new_ind);

    FT_Bitmap bm = font->face->glyph->bitmap;

    // copy the glyph bitmap into the pixels on this page. the texture can be updated from the pixels later
    for (size_t gy = 0; gy < bm.rows; gy++) {
        size_t ty = ((new_ind / font->page_glyph_x) * font->size) + gy;
        for (size_t gx = 0; gx < bm.width; gx++) {
            size_t tx = ((new_ind % font->page_glyph_x) * font->size) + gx;

            size_t goffset = (gy * bm.width) + gx;
            size_t toffset = (ty * GLYPH_TEX_SIZE) + tx;

            // gamma correct the alpha mask. first it needs to be scaled to 0..1
            double a = bm.buffer[goffset] / 255.0;
            // gamma = 2.0
            double ca = pow(a, 1/2.0);
            // then scale it back to 0..255
            uint8_t corrected_a = (uint8_t)ceil(ca * 255);

            last_page->pixels[toffset] = corrected_a;
        }
    }
}

static void ui_font_update_textures(ui_font_t *font) {
    glyph_page_t *p = font->glyph_pages;
    while (p) {
        glBindTexture(GL_TEXTURE_RECTANGLE, p->texture);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RED, GLYPH_TEX_SIZE, GLYPH_TEX_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, p->pixels);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        p = p->next;
    }
}

void ui_font_render_text(ui_font_t *font, mat4f_t *proj, int x, int y, const char *text, size_t count, ui_color_t color) {
    gl_shader_program_use(shader_program);
    glBindVertexArray(vao);

    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    float penx = (float)x;

    // we get text as individual 8 bit characters, but we assume it's UTF-8.
    // each codepoint could be up to 4 bytes long.
    uint32_t codepoint = 0;
    int bytes_remaining = 0;

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
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        size_t char_hash_ind = codepoint % font->page_max_glyphs;
        size_t char_ind = char_hash_ind;

        glyph_page_t *char_page = font->glyph_pages;
        
        // work out which page the glyph is on and what its index is on that page
        // this might be a tad slow, but the most commonly used glyphs *should*
        // be on the first page anyway
        while (char_page) {
            if (char_page->glyphs[char_ind]==codepoint) break;

            char_ind++;
            if (char_ind>=font->page_max_glyphs) char_ind = 0;
            if (char_ind==char_hash_ind) char_page = char_page->next;
        }

        if (char_page==NULL) {
            // this glyph isn't in a texture yet, so render it
            ui_font_render_glyph(font, codepoint);
            ui_font_update_textures(font);
            // go back and restart this part of the for loop for this glyph
            c--;
            continue;
        }

        if (char_page->metrics[char_ind].bitmap_width==0) {
            // this is an empty glyph, just move the pen position forward without rendering
            penx += (float)char_page->metrics[char_ind].advance_x;
            continue;
        }
        

        float gx = penx + (float)char_page->metrics[char_ind].bearing_x;
        float gy = y + (font->face->size->metrics.ascender / 64.f) - (float)char_page->metrics[char_ind].bearing_y;

        float tex_left = (float)(char_ind % font->page_glyph_x) * font->size;
        float tex_top = (float)(char_ind / font->page_glyph_x) * font->size;

        glUniform1f(0, gx);                                                                         // left
        glUniform1f(1, gy);                                                                         // top 
        glUniform1f(2, gx + char_page->metrics[char_ind].bitmap_width);                             // right
        glUniform1f(3, gy + char_page->metrics[char_ind].bitmap_rows);                              // bottom
        glUniform4f(4, UI_COLOR_R(color), UI_COLOR_G(color), UI_COLOR_B(color), UI_COLOR_A(color)); // color
        glUniformMatrix4fv(5, 1, GL_FALSE, (const GLfloat*)proj);                                   // projection
        glUniform1f(6, tex_left);                                                                   // texture left
        glUniform1f(7, tex_top);                                                                    // texture top
        glUniform1f(8, tex_left + char_page->metrics[char_ind].bitmap_width);                       // texture right
        glUniform1f(9, tex_top + char_page->metrics[char_ind].bitmap_rows);                         // texture bottom
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, char_page->texture);        

        glDrawArrays(GL_TRIANGLES, 0, 6);

        penx += (float)char_page->metrics[char_ind].advance_x;
        prev_glyph = glyph;
    }


    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glUseProgram(0);
}

ui_font_t *ui_font_get(const char *path, int size, int weight, int slant, int width) {
    if (cache_size == CACHE_MAX_SIZE) {
        logger_error(logger, "Reached maximum cached fonts.");
        error_and_exit("EG-Overlay: UI-Font", "Reached maximum cached fonts.");
    }

    char buf[1024] = {0};
    sprintf_s(buf, 1024, "%s%d%d%d%d", path, size, weight, slant, width);
    uint32_t hash = djb2_hash_string(buf);
    size_t key_ind = hash % CACHE_MAX_SIZE;

    size_t n = cache_size;   
    
    while (n--) {
        if (cache_keys[key_ind]==hash) return cache_fonts[key_ind];
        key_ind++;
        if (key_ind==hash % CACHE_MAX_SIZE) break;
        if (key_ind>= CACHE_MAX_SIZE) key_ind = 0;
    }

    logger_debug(logger, "Didn't find %s, creating.", buf);
    ui_font_t *f = ui_font_new(path, size, weight, slant, width);

    key_ind = hash % CACHE_MAX_SIZE;
    while (cache_keys[key_ind]!=hash){
        if (cache_keys[key_ind]) {
            key_ind++;
            if (key_ind>=CACHE_MAX_SIZE) key_ind = 0;
        } else {
            cache_keys[key_ind] = hash;
            cache_fonts[key_ind] = f;
        }
    }

    cache_size++;

    return f;
}

uint32_t ui_font_get_text_width(ui_font_t *font, const char *text, int count) {
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    int penx = 0;

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
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);
        //FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        size_t char_hash_ind = codepoint % font->page_max_glyphs;
        size_t char_ind = char_hash_ind;

        glyph_page_t *char_page = font->glyph_pages;
        
        // work out which page the glyph is on and what its index is on that page
        // this might be a tad slow, but the most commonly used glyphs *should*
        // be on the first page anyway
        while (char_page) {
            if (char_page->glyphs[char_ind]==codepoint) break;

            char_ind++;
            if (char_ind>=font->page_max_glyphs) char_ind = 0;
            if (char_ind==char_hash_ind) char_page = char_page->next;
        }

        if (char_page==NULL) {
            // this glyph isn't in a texture yet, so render it
            ui_font_render_glyph(font, codepoint);
            ui_font_update_textures(font);
            // go back and restart this part of the for loop for this glyph
            c--;
            continue;
        }

        prev_glyph = glyph;

        penx += (int)char_page->metrics[char_ind].advance_x;
    }

    return penx;
}

uint32_t ui_font_get_index_of_width(ui_font_t *font, const char *text, int width) {
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    int penx = 0;

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
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);
        //FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        size_t char_hash_ind = codepoint % font->page_max_glyphs;
        size_t char_ind = char_hash_ind;

        glyph_page_t *char_page = font->glyph_pages;
        
        // work out which page the glyph is on and what its index is on that page
        // this might be a tad slow, but the most commonly used glyphs *should*
        // be on the first page anyway
        while (char_page) {
            if (char_page->glyphs[char_ind]==codepoint) break;

            char_ind++;
            if (char_ind>=font->page_max_glyphs) char_ind = 0;
            if (char_ind==char_hash_ind) char_page = char_page->next;
        }

        if (char_page==NULL) {
            // this glyph isn't in a texture yet, so render it
            ui_font_render_glyph(font, codepoint);
            ui_font_update_textures(font);
            // go back and restart this part of the for loop for this glyph
            c--;
            continue;
        }

        prev_glyph = glyph;

        penx += (int)char_page->metrics[char_ind].advance_x;

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

int ui_font_get_text_wrap_indices(ui_font_t *font, const char *text, int width, int **wrap_indices) {
    FT_UInt glyph;
    FT_UInt prev_glyph = 0;

    int penx = 0;
    int *breaks = NULL;
    int break_count = 0;
    int last_break_ind = 0;

    uint32_t codepoint = 0;
    int bytes_remaining = 0;

    for (size_t c=0;c<strlen(text);c++) {
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
        if (bytes_remaining) continue;

        glyph = FT_Get_Char_Index(font->face, codepoint);
        //FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT);

        // kerning!
        if (c>0 && FT_HAS_KERNING(font->face)) {
            FT_Vector delta;
            FT_Get_Kerning(font->face, prev_glyph, glyph, FT_KERNING_DEFAULT, &delta);
            penx += delta.x / 64;
        }

        size_t char_hash_ind = codepoint % font->page_max_glyphs;
        size_t char_ind = char_hash_ind;

        glyph_page_t *char_page = font->glyph_pages;
        
        // work out which page the glyph is on and what its index is on that page
        // this might be a tad slow, but the most commonly used glyphs *should*
        // be on the first page anyway
        while (char_page) {
            if (char_page->glyphs[char_ind]==codepoint) break;

            char_ind++;
            if (char_ind>=font->page_max_glyphs) char_ind = 0;
            if (char_ind==char_hash_ind) char_page = char_page->next;
        }

        if (char_page==NULL) {
            // this glyph isn't in a texture yet, so render it
            ui_font_render_glyph(font, codepoint);
            ui_font_update_textures(font);
            // go back and restart this part of the for loop for this glyph
            c--;
            continue;
        }

        prev_glyph = glyph;

        penx += (int)char_page->metrics[char_ind].advance_x;

        if (codepoint==' ' || codepoint=='\t') last_break_ind = (int)c;

        if (penx > width) {
            breaks = egoverlay_realloc(breaks, (break_count + 1) * sizeof(int));
            if (last_break_ind > 0) {
                breaks[break_count++] = last_break_ind;
                c = last_break_ind + 1;
            } else {
                breaks[break_count++] = (int)c;
            }
            penx = 0;
            last_break_ind = 0;
        }
    }

    *wrap_indices = breaks;

    return break_count;
}
