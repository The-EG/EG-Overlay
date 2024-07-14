#include "utils.h"
#include "lamath.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <lauxlib.h>

void mat4f_ortho(mat4f_t *m, float left, float right, float top, float bottom, float near_, float far_) {
    /*
     mat4f_t m = {
        2.f / (right - left), 0.f, 0.f, ((right + left) / (right - left)) * -1.f,
        0.f, 2.f / (top - bottom), 0, ((top + bottom) / (top - bottom)) * -1.f,
        0.f, 0.f, -2 / (far_ - near_), ((far_ + near_) / (far_ - near_)) * -1.f,
        0.f, 0.f, 0.f, 1.f 
    };
    */
    m->i1j1 = 2.f / (right - left);
    m->i2j1 = 0.f;
    m->i3j1 = 0.f;
    m->i4j1 = ((right + left) / (right - left)) * -1.f;

    m->i1j2 = 0.f;
    m->i2j2 = 2.f / (top - bottom);
    m->i3j2 = 0.f;
    m->i4j2 = ((top + bottom) / (top - bottom)) * -1.f;

    m->i1j3 = 0.f;
    m->i2j3 = 0.f;
    m->i3j3 = -2 / (far_ - near_);
    m->i4j3 = ((far_ + near_) / (far_ - near_)) * -1.f;
    
    m->i1j4 = 0.f;
    m->i2j4 = 0.f;
    m->i3j4 = 0.f;
    m->i4j4 = 1.f;
}

void mat4f_identity(mat4f_t *m) {
    m->i1j1 = 1.f; m->i2j1 = 0.f; m->i3j1 = 0.f; m->i4j1 = 0.f;
    m->i1j2 = 0.f; m->i2j2 = 1.f; m->i3j2 = 0.f; m->i4j2 = 0.f;
    m->i1j3 = 0.f; m->i2j3 = 0.f; m->i3j3 = 1.f; m->i4j3 = 0.f;
    m->i1j4 = 0.f; m->i2j4 = 0.f; m->i3j4 = 0.f; m->i4j4 = 1.f;
}

void mat4f_translate(mat4f_t *m, float x, float y, float z) {
    mat4f_identity(m);
    m->i4j1 = x;
    m->i4j2 = y;
    m->i4j3 = z;
}

void mat4f_mult_mat4f(mat4f_t *a, mat4f_t *b, mat4f_t *out) {
    out->i1j1 = a->i1j1 * b->i1j1 + a->i1j2 * b->i2j1 + a->i1j3 * b->i3j1 + a->i1j4 * b->i4j1;
    out->i2j1 = a->i2j1 * b->i1j1 + a->i2j2 * b->i2j1 + a->i2j3 * b->i3j1 + a->i2j4 * b->i4j1;
    out->i3j1 = a->i3j1 * b->i1j1 + a->i3j2 * b->i2j1 + a->i3j3 * b->i3j1 + a->i3j4 * b->i4j1;
    out->i4j1 = a->i4j1 * b->i1j1 + a->i4j2 * b->i2j1 + a->i4j3 * b->i3j1 + a->i4j4 * b->i4j1;

    out->i1j2 = a->i1j1 * b->i1j2 + a->i1j2 * b->i2j2 + a->i1j3 * b->i3j2 + a->i1j4 * b->i4j2;
    out->i2j2 = a->i2j1 * b->i1j2 + a->i2j2 * b->i2j2 + a->i2j3 * b->i3j2 + a->i2j4 * b->i4j2;
    out->i3j2 = a->i3j1 * b->i1j2 + a->i3j2 * b->i2j2 + a->i3j3 * b->i3j2 + a->i3j4 * b->i4j2;
    out->i4j2 = a->i4j1 * b->i1j2 + a->i4j2 * b->i2j2 + a->i4j3 * b->i3j2 + a->i4j4 * b->i4j2;

    out->i1j3 = a->i1j1 * b->i1j3 + a->i1j2 * b->i2j3 + a->i1j3 * b->i3j3 + a->i1j4 * b->i4j3;
    out->i2j3 = a->i2j1 * b->i1j3 + a->i2j2 * b->i2j3 + a->i2j3 * b->i3j3 + a->i2j4 * b->i4j3;
    out->i3j3 = a->i3j1 * b->i1j3 + a->i3j2 * b->i2j3 + a->i3j3 * b->i3j3 + a->i3j4 * b->i4j3;
    out->i4j3 = a->i4j1 * b->i1j3 + a->i4j2 * b->i2j3 + a->i4j3 * b->i3j3 + a->i4j4 * b->i4j3;

    out->i1j4 = a->i1j1 * b->i1j4 + a->i1j2 * b->i2j4 + a->i1j3 * b->i3j4 + a->i1j4 * b->i4j4;
    out->i2j4 = a->i2j1 * b->i1j4 + a->i2j2 * b->i2j4 + a->i2j3 * b->i3j4 + a->i2j4 * b->i4j4;
    out->i3j4 = a->i3j1 * b->i1j4 + a->i3j2 * b->i2j4 + a->i3j3 * b->i3j4 + a->i3j4 * b->i4j4;
    out->i4j4 = a->i4j1 * b->i1j4 + a->i4j2 * b->i2j4 + a->i4j3 * b->i3j4 + a->i4j4 * b->i4j4;
}

static void mat4f_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "LAMathMat4FMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }
}

void mat4f_push_to_lua(mat4f_t *m, lua_State *L) {
    mat4f_t **pm = lua_newuserdata(L, sizeof(mat4f_t *));
    *pm = m;

    mat4f_register_metatable(L);
    lua_setmetatable(L, -2);
}

mat4f_t *mat4f_from_lua(lua_State *L, int i) {
    mat4f_t *m = *(mat4f_t**)luaL_checkudata(L, i, "LAMathMat4FMetaTable");

    return m;
}

void mat2f_rotate(mat2f_t *m, float radians) {
    m->i1j1 = cosf(radians);
    m->i1j2 = sinf(radians);
    m->i2j1 = sinf(radians) * -1.f;
    m->i2j2 = cosf(radians);
}

float deg2rad(float degrees) {
    return degrees * (float)M_PI / 180.0f;
}

float rad2degrees(float radians) {
    return radians * 180.0f / (float)M_PI;
}

void mat2f_mult_vec2f(mat2f_t *m, vec2f_t *v, vec2f_t *out) {
    out->x = (v->x * m->i1j1) + (v->y * m->i2j1);
    out->y = (v->x * m->i1j2) + (v->y * m->i2j2);
}

void vec2f_mult_vec2f(vec2f_t *a, vec2f_t *b, vec2f_t *out) {
    out->x = a->x * b->x;
    out->y = a->y * b->y;
}

void vec2f_mult_f(vec2f_t *a, float b, vec2f_t *out) {
    out->x = a->x * b;
    out->y = a->y * b;
}

void vec2f_translate(vec2f_t *in, vec2f_t *translate, vec2f_t *out) {
    out->x = in->x + translate->x;
    out->y = in->y + translate->y;
}

void vec2f_rotate(vec2f_t *in, vec2f_t *origin, float radians, vec2f_t *out) {
    mat2f_t rot = {0};
    mat2f_rotate(&rot, radians);

    vec2f_t neg = {-1.f, -1.f};
    vec2f_t origin_neg;
    vec2f_t translated;
    vec2f_t rotated;

    vec2f_mult_vec2f(origin, &neg, &origin_neg);
    vec2f_translate(in, &origin_neg, &translated);
    mat2f_mult_vec2f(&rot, &translated, &rotated);
    vec2f_translate(&rotated, origin, out);
}

float angle_of_segment(vec2f_t *p1, vec2f_t *p2) {
    vec2f_t translated_p2;
    vec2f_t neg_p1;

    vec2f_mult_f(p1, -1.f, &neg_p1);
    vec2f_translate(p2, &neg_p1, &translated_p2);

    return atan2f(translated_p2.y, translated_p2.x);
}