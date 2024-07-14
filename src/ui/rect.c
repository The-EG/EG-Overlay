#include "../gl.h"
#include "ui.h"
#include "../logging/helpers.h"
#include "rect.h"
#include "../utils.h"
#include "../lamath.h"
#include <lauxlib.h>

static logger_t *logger = NULL;
static gl_shader_program_t *shader_program = NULL;
static GLuint vao = 0;

void ui_rect_init() {
    logger = logger_get("ui-rect");

    logger_debug(logger, "init");

    shader_program = gl_shader_program_new();
    gl_shader_program_attach_shader_file(shader_program, "shaders/rect.vert", GL_VERTEX_SHADER);
    gl_shader_program_attach_shader_file(shader_program, "shaders/rect.frag", GL_FRAGMENT_SHADER);
    gl_shader_program_link(shader_program);

    glGenVertexArrays(1, &vao);
}

void ui_rect_cleanup() {
    logger_debug(logger, "cleanup");

    gl_shader_program_free(shader_program);

    glDeleteVertexArrays(1, &vao);
}

void ui_rect_draw(int x, int y, int width, int height, ui_color_t color, mat4f_t *proj) {    
    gl_shader_program_use(shader_program);

    float left = (float)x;
    float top = (float)y;
    float right = (float)x + width;
    float bottom = (float)y + height;

    glUniform1f(0, left);
    glUniform1f(1, top);
    glUniform1f(2, right);
    glUniform1f(3, bottom);
    
    glUniform4f(4, UI_COLOR_R(color), UI_COLOR_G(color), UI_COLOR_B(color), UI_COLOR_A(color));
    glUniformMatrix4fv(5, 1, GL_FALSE, (const GLfloat*)proj);

    glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
}

static int ui_rect_lua_draw(lua_State *L);

void ui_rect_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_rect_lua_draw);
    lua_setfield(L, -2, "rect");   
}

static int ui_rect_lua_draw(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    ui_color_t c = (int)luaL_checkinteger(L, 5);
    mat4f_t *proj = mat4f_from_lua(L, 6);

    ui_rect_draw(x, y, w, h, c, proj);

    return 0;
}
