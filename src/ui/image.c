#include <stb_image.h>
#include <lauxlib.h>
#include <string.h>
#include "image.h"
#include "../logging/helpers.h"
#include "../utils.h"
#include "../gl.h"

struct ui_image_t {
    int tex_size;
    int img_width;
    int img_height;

    float tex_max_u;
    float tex_max_v;

    GLuint texture;
};

static logger_t *logger = NULL;

static gl_shader_program_t *shader_program = NULL;

static GLuint vao = 0;

void ui_image_init() {
    logger = logger_get("ui_image");
    logger_debug(logger, "init");

    shader_program = gl_shader_program_new();
    gl_shader_program_attach_shader_file(shader_program, "shaders/image.vert", GL_VERTEX_SHADER);
    gl_shader_program_attach_shader_file(shader_program, "shaders/image.frag", GL_FRAGMENT_SHADER);
    gl_shader_program_link(shader_program);

    glGenVertexArrays(1, &vao);
}

void ui_image_cleanup() {
    logger_debug(logger, "cleanup");
    gl_shader_program_free(shader_program);

    glDeleteVertexArrays(1, &vao);
}

ui_image_t *ui_image_new_from_file(const char *path) {
    logger_debug(logger, "Loading image from %s", path);

    ui_image_t *img = egoverlay_calloc(1, sizeof(ui_image_t));

    int n;

    uint8_t *pixels = stbi_load(path, &img->img_width, &img->img_height, &n, 4);

    if (pixels==NULL) {
        logger_error(logger, "Couldn't load image from %s", path);
        abort();
    }

    // work out how large the texture needs to be
    int32_t texsize = 16;
    while (texsize < img->img_width || texsize < img->img_height) texsize <<= 1;

    // dont' allow massive textures
    if (texsize > 4096) {
        logger_error(logger, "Texture for %s would be too large.", path);
        abort();
    }
    img->tex_size = texsize;
    img->tex_max_u = (float)img->img_width / (float)texsize;
    img->tex_max_v = (float)img->img_height / (float)texsize;

    uint8_t *tex_pixels = egoverlay_calloc((texsize * texsize * 4), sizeof(uint8_t));
    for (size_t r=0;r<img->img_height;r++) {
        size_t tex_row_offset = r * texsize * 4;
        size_t img_row_offset = r * img->img_width * 4;
        memcpy(tex_pixels + tex_row_offset, pixels + img_row_offset, img->img_width * 4);
    }

    glGenTextures(1, &img->texture);
    glBindTexture(GL_TEXTURE_2D, img->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texsize, texsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    egoverlay_free(tex_pixels);
    stbi_image_free(pixels);

    return img;
}

void ui_image_free(ui_image_t *image) {

    glDeleteTextures(1, &image->texture);
    egoverlay_free(image);
}

void ui_image_draw(ui_image_t *image, mat4f_t *proj, int x, int y, int width, int height, float saturation_f, float value_f) {
    gl_shader_program_use(shader_program);
    glBindVertexArray(vao);

    glUniform1f(0, (float)x);
    glUniform1f(1, (float)y);
    glUniform1f(2, (float)x + width);
    glUniform1f(3, (float)y + height);
    glUniformMatrix4fv(4, 1, GL_FALSE, (const GLfloat*)proj);
    glUniform1f(5, image->tex_max_u);
    glUniform1f(6, image->tex_max_v);
    glUniform1f(7, value_f);
    glUniform1f(8, saturation_f);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image->texture);        

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

int ui_image_height_for_width(ui_image_t *image, int width) {
    return (int)(image->img_height * ((float)width / (float)image->img_width));
}

int ui_image_width_for_height(ui_image_t *image, int height) {
    return (int)(image->img_width * ((float)height / (float)image->img_height));
}

void ui_image_size(ui_image_t *image, int *width, int *height) {
    *width = image->img_width;
    *height = image->img_height;
}

static int ui_image_lua_new_from_file(lua_State *L);
static int ui_image_lua_del(lua_State *L);

static luaL_Reg image_funcs[] = {
    "__gc",              &ui_image_lua_del,
    NULL,    NULL
};

void ui_image_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_image_lua_new_from_file);
    lua_setfield(L, -2, "image_from_file");
}

static void ui_image_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UIImageMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, image_funcs, 0);
    }
}

void ui_image_push_to_lua(ui_image_t *image, lua_State *L, int lua_managed) {
    ui_image_t **i = lua_newuserdata(L, sizeof(ui_image_t*));
    *i = image;

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    ui_image_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

static int ui_image_lua_new_from_file(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    ui_image_t *img = ui_image_new_from_file(path);
    ui_image_push_to_lua(img, L, 1);

    return 1;
}

static int ui_image_lua_del(lua_State *L) {
    ui_image_t *img = *(ui_image_t**)luaL_checkudata(L, 1, "UIImageMetaTable");

    lua_getiuservalue(L, -1, 1);
    int lua_managed = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (lua_managed) ui_image_free(img);

    return 0;
}

ui_image_t *ui_image_from_lua(lua_State *L, int arg) {
    ui_image_t *img = *(ui_image_t**)luaL_checkudata(L, arg, "UIImageMetaTable");

    return img;
}
