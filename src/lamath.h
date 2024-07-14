#pragma once
#include <lua.h>

typedef struct {
    float i1j1;
    float i1j2;

    float i2j1;
    float i2j2;
} mat2f_t;

typedef struct {
    float i1j1;
    float i1j2;
    float i1j3;
    float i1j4;

    float i2j1;
    float i2j2;
    float i2j3;
    float i2j4;

    float i3j1;
    float i3j2;
    float i3j3;
    float i3j4;

    float i4j1;
    float i4j2;
    float i4j3;
    float i4j4;
} mat4f_t;

typedef struct {
    float x;
    float y;
} vec2f_t;

float deg2rad(float degrees);
float rad2degrees(float radians);

void mat4f_identity(mat4f_t *m);
void mat4f_translate(mat4f_t *m, float x, float y, float z);
void mat4f_ortho(mat4f_t *m, float left, float right, float top, float bottom, float near_, float far_);

void mat4f_mult_mat4f(mat4f_t *a, mat4f_t *b, mat4f_t *out);

void mat4f_push_to_lua(mat4f_t *mat4f, lua_State *L);
mat4f_t *mat4f_from_lua(lua_State *L, int i);

void mat2f_rotate(mat2f_t *m, float radians);

void mat2f_mult_vec2f(mat2f_t *m, vec2f_t *v, vec2f_t *out);

void vec2f_translate(vec2f_t *in, vec2f_t *translate, vec2f_t *out);
void vec2f_rotate(vec2f_t *in, vec2f_t *origin, float radians, vec2f_t *out);
void vec2f_mult_vec2f(vec2f_t *a, vec2f_t *b, vec2f_t *out);
void vec2f_mult_f(vec2f_t *a, float b, vec2f_t *out);

// angle of a line given by two points in relation to the x axis
float angle_of_segment(vec2f_t *p1, vec2f_t *p2);