#include "../gl.h"
#include "../logging/helpers.h"
#include "../utils.h"
#include "polyline.h"
#include <string.h>
#include "ui.h"
#include <lauxlib.h>

static logger_t *logger = NULL;

static gl_shader_program_t *shader_program = NULL;

struct ui_polyline_t {
    ui_element_t element;

    ui_color_t color;
    uint8_t width;

    GLuint vao;
    GLuint vbo;

    vec2f_t *verts;
    size_t vert_count;

    int update_verts;
};

static void ui_polyline_draw(ui_polyline_t *line, int offset_x, int offset_y, mat4f_t *proj);

void ui_polyline_init() {
    logger = logger_get("ui_polyline");

    logger_debug(logger, "init");

    shader_program = gl_shader_program_new();
    gl_shader_program_attach_shader_file(shader_program, "shaders/polyline.vert", GL_VERTEX_SHADER);
    gl_shader_program_attach_shader_file(shader_program, "shaders/polyline.frag", GL_FRAGMENT_SHADER);
    gl_shader_program_link(shader_program);
}

void ui_polyline_cleanup() {
    logger_debug(logger, "cleanup");
    gl_shader_program_free(shader_program);
}

ui_polyline_t *ui_polyline_new() {
    ui_polyline_t *p = calloc(1, sizeof(ui_polyline_t));

    p->element.draw = &ui_polyline_draw;

    p->color = 0xFFFFFFFF;
    p->width = 1;

    glGenVertexArrays(1, &p->vao);
    glGenBuffers(1, &p->vbo);

    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return p;
}

void ui_polyline_free(ui_polyline_t *line) {
    if (line->verts) free(line->verts);

    glDeleteBuffers(1, &line->vbo);
    glDeleteVertexArrays(1, &line->vao);

    free(line);
}

void ui_polyline_set_color(ui_polyline_t *line, ui_color_t color) {
    line->color = color;
}

void ui_polyline_set_width(ui_polyline_t *line, uint8_t width) {
    line->width = width;
}

void ui_polyline_add_point(ui_polyline_t *line, int x, int y) {
    size_t new_size = line->vert_count+1;

    line->verts = realloc(line->verts, sizeof(vec2f_t) * new_size);
    line->verts [line->vert_count].x = (float)x;
    line->verts [line->vert_count].y = (float)y;
    
    line->vert_count++;    
}

void ui_polyline_clear_points(ui_polyline_t *line) {
    if (line->verts==NULL) return;

    free(line->verts);
    line->verts = NULL;
    line->vert_count = 0;
}

void update_vbo(ui_polyline_t *line) {
    if (line->vert_count<2) return;

    size_t vbo_size = sizeof(vec2f_t) * 6 * (line->vert_count - 1);
    vec2f_t *vbo_verts = calloc(1, vbo_size);
    size_t vv = 0;

    for (size_t v=1;v<line->vert_count;v++) {
        vec2f_t *p1 = line->verts + (v-1);
        vec2f_t *p2 = line->verts + v;

        float a = angle_of_segment(p1, p2);
        vec2f_t p1a = {p1->x + (line->width / 2.f), p1->y};
        vec2f_t p1b = {p1->x + (line->width / 2.f), p1->y};

        vec2f_rotate(&p1a, p1, a - deg2rad(90), &p1a);
        vec2f_rotate(&p1b, p1, a + deg2rad(90), &p1b);

        vec2f_t p2a = {p2->x + (line->width / 2.f), p2->y};
        vec2f_t p2b = {p2->x + (line->width / 2.f), p2->y};

        vec2f_rotate(&p2a, p2, a - deg2rad(90), &p2a);
        vec2f_rotate(&p2b, p2, a + deg2rad(90), &p2b);

        if (v<line->vert_count-1) {
            // for multiline segments, the joints need to have points
            // where the sides intersect
            // so we need to calculate where the sides are for the next segment
            // to figure that out

            vec2f_t *p3 = line->verts + (v + 1);
            float a2 = angle_of_segment(p2, p3);

            // same as p2a and p2b, but rotated for p3
            vec2f_t np2a = {p2->x + (line->width / 2.f), p2->y};
            vec2f_t np2b = {p2->x + (line->width / 2.f), p2->y};
            vec2f_rotate(&np2a, p2, a2 - deg2rad(90), &np2a);
            vec2f_rotate(&np2b, p2, a2 + deg2rad(90), &np2b);

            vec2f_t np3a = {p3->x + (line->width / 2.f), p3->y};
            vec2f_t np3b = {p3->x + (line->width / 2.f), p3->y};
            vec2f_rotate(&np3a, p3, a2 - deg2rad(90), &np3a);
            vec2f_rotate(&np3b, p3, a2 + deg2rad(90), &np3b);

            
            // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
            // calculate the denominator first, if it's 0 the lines are coincident
            float ad = (p1a.x - p2a.x) * (np2a.y - np3a.y) - (p1a.y - p2a.y) * (np2a.x - np3a.x);
            float bd = (p1b.x - p2b.x) * (np2b.y - np3b.y) - (p1b.y - p2b.y) * (np2b.x - np3b.x);
            
            if (ad!=0 && bd!=0) {
                float axn = (p1a.x * p2a.y - p1a.y * p2a.x) * (np2a.x - np3a.x) - (p1a.x - p2a.x) * (np2a.x * np3a.y - np2a.y * np3a.x);
                float bxn = (p1b.x * p2b.y - p1b.y * p2b.x) * (np2b.x - np3b.x) - (p1b.x - p2b.x) * (np2b.x * np3b.y - np2b.y * np3b.x);

                float ayn = (p1a.x * p2a.y - p1a.y * p2a.x) * (np2a.y - np3a.y) - (p1a.y - p2a.y) * (np2a.x * np3a.y - np2a.y * np3a.x);
                float byn = (p1b.x * p2b.y - p1b.y * p2b.x) * (np2b.y - np3b.y) - (p1b.y - p2b.y) * (np2b.x * np3b.y - np2b.y * np3b.x);

                // if not the intersections become p2a and p2b

                // if np2a.x == np3a.x then we know the x component of p2a is the same
                // the math above doesn't cover this case, so just set it manually
                if (np2a.x == np3a.x) p2a.x = np2a.x;
                else p2a.x = axn / ad; // otherwise just calculate it
                p2a.y = ayn / ad;

                if (np2b.x == np3b.x) p2b.x = np2b.x;
                else p2b.x = bxn / bd;
                p2b.y = byn / bd;
            }
        }

        if (v>1) {
            // if we did the above calculation already for the previous segment use the previous points for p1a and p1b
            p1a = vbo_verts[vv-4];
            p1b = vbo_verts[vv-1];
        }

        vbo_verts[vv++] = p1a;
        vbo_verts[vv++] = p2b;
        vbo_verts[vv++] = p2a;

        vbo_verts[vv++] = p1a;
        vbo_verts[vv++] = p1b;
        vbo_verts[vv++] = p2b;
    }

    glBindBuffer(GL_ARRAY_BUFFER, line->vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_size, vbo_verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    line->update_verts = 0;
}

void ui_polyline_update(ui_polyline_t *line) {
    line->update_verts = 1;   
}

void ui_polyline_draw(ui_polyline_t *line, int offset_x, int offset_y, mat4f_t *proj) {
    if (line->update_verts) update_vbo(line);

    if (line->vert_count<2) return;
    gl_shader_program_use(shader_program);
    
    glUniformMatrix4fv(0, 1, GL_FALSE, (const GLfloat*)proj);
    glUniform4f(1, UI_COLOR_R(line->color), UI_COLOR_G(line->color), UI_COLOR_B(line->color), UI_COLOR_A(line->color));

    glBindVertexArray(line->vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(line->vert_count - 1) * 6);
    glBindVertexArray(0);
}

int ui_polyline_lua_new(lua_State *L);
int ui_polyline_lua_del(lua_State *L);
int ui_polyline_lua_set_color(lua_State *L);
int ui_polyline_lua_set_width(lua_State *L);
int ui_polyline_lua_add_point(lua_State *L);
int ui_polyline_lua_clear_points(lua_State *L);
int ui_polyline_lua_update(lua_State *L);
int ui_polyline_lua_draw(lua_State *L);

static struct luaL_Reg polyline_funcs[] = {
    "__gc",         &ui_polyline_lua_del,
    "set_color",    &ui_polyline_lua_set_color,
    "set_width",    &ui_polyline_lua_set_width,
    "add_point",    &ui_polyline_lua_add_point,
    "clear_points", &ui_polyline_lua_clear_points,
    "update",       &ui_polyline_lua_update,
    "draw",         &ui_polyline_lua_draw,
    NULL,            NULL
};

void ui_polyline_lua_register_ui_funcs(lua_State *L) {
    lua_pushcfunction(L, &ui_polyline_lua_new);
    lua_setfield(L, -2, "polyline");
}

static void ui_polyline_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "UIPolylineMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, polyline_funcs, 0);
    }
}

#define CHECK_POLYLINE(L, i) *(ui_polyline_t**)luaL_checkudata(L, i, "UIPolylineMetaTable")

void ui_polyline_push_to_lua(ui_polyline_t *line, lua_State *L, int lua_managed) {
    ui_polyline_t **p = lua_newuserdata(L, sizeof(ui_polyline_t*));
    *p = line;

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    ui_polyline_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

int ui_polyline_lua_new(lua_State *L) {
    ui_polyline_t *line = ui_polyline_new();
    ui_polyline_push_to_lua(line, L, 1);

    return 1;
}

int ui_polyline_lua_del(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);

    lua_getiuservalue(L, -1, 1);
    int lua_managed = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (lua_managed) ui_polyline_free(line);

    return 0;
}

int ui_polyline_lua_set_color(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);

    ui_color_t color = (ui_color_t)luaL_checkinteger(L, 2);

    ui_polyline_set_color(line, color);

    return 0;
}

int ui_polyline_lua_set_width(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);
    uint8_t width = (uint8_t)luaL_checkinteger(L, 2);
    ui_polyline_set_width(line, width);

    return 0;
}

int ui_polyline_lua_add_point(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    ui_polyline_add_point(line, x, y);

    return 0;
}

int ui_polyline_lua_clear_points(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);
    ui_polyline_clear_points(line);

    return 0;
}

int ui_polyline_lua_update(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);
    ui_polyline_update(line);

    return 0;
}

int ui_polyline_lua_draw(lua_State *L) {
    ui_polyline_t *line = CHECK_POLYLINE(L, 1);
    mat4f_t *proj = mat4f_from_lua(L, 2);

    // TODO offsets
    ui_polyline_draw(line, 0, 0, proj);

    return 0;
}