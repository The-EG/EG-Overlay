#pragma once

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

    float i2j1;
    float i2j2;
    float i2j3;
    
    float i3j1;
    float i3j2;
    float i3j3;
} mat3f_t;

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

typedef struct {
    float x;
    float y;
    float z;
} vec3f_t;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4f_t;

/**
 * Convert degrees to radians.
 * 
 * @param degrees
 */
float deg2rad(float degrees);

/**
 * Convert radians to degrees.
 * 
 * @param radians
 */
float rad2deg(float radians);

/**
 * Initialize a 4x4 matrix to identity.
 * 
 * @param m The matrix to initialize. It must be valid/allocated already.
 */
void mat4f_identity(mat4f_t *m);

/**
 * Initialize a 4x4 matrix to a translation matrix, given by x, y, and z.
 * 
 * @param m
 * @param x
 * @param y
 * @param z
 */
void mat4f_translate(mat4f_t *m, float x, float y, float z);

void mat4f_minors(mat4f_t *m, mat4f_t *out);
void mat4f_cofactors(mat4f_t *m, mat4f_t *out);
void mat4f_adjugate(mat4f_t *m, mat4f_t *out);
void mat4f_inverse(mat4f_t *m, mat4f_t *out);

float mat4f_determinate(mat4f_t *m);

void mat4f_ortho(mat4f_t *m, float left, float right, float top, float bottom, float near_, float far_);

void mat4f_perpsective_rh(mat4f_t *m, float fovy, float aspect, float near_, float far_);
void mat4f_lookat_rh(mat4f_t *m, vec3f_t *camera, vec3f_t *center, vec3f_t *up);

void mat4f_perpsective_lh(mat4f_t *m, float fovy, float aspect, float near_, float far_);
void mat4f_lookat_lh(mat4f_t *m, vec3f_t *camera, vec3f_t *center, vec3f_t *up);
void mat4f_camera_facing(mat4f_t *m, vec3f_t *camera, vec3f_t *forward, vec3f_t *up);

void mat4f_mult_mat4f(mat4f_t *a, mat4f_t *b, mat4f_t *out);
void mat4f_mult_vec4f(mat4f_t *a, vec4f_t *b, vec4f_t *out);

void mat4f_rotatex(mat4f_t *m, float radians);
void mat4f_rotatey(mat4f_t *m, float radians);
void mat4f_rotatez(mat4f_t *m, float radians);

float mat3f_determinate_f(float a, float b, float c, float d, float e, float f, float g, float h, float i);

void mat2f_rotate(mat2f_t *m, float radians);

// +-----+
// | A B |
// | C D |
// +-----+
float mat2f_determinate_f(float a, float b, float c, float d);

void mat2f_mult_vec2f(mat2f_t *m, vec2f_t *v, vec2f_t *out);

void vec2f_translate(vec2f_t *in, vec2f_t *translate, vec2f_t *out);
void vec2f_rotate(vec2f_t *in, vec2f_t *origin, float radians, vec2f_t *out);
void vec2f_mult_vec2f(vec2f_t *a, vec2f_t *b, vec2f_t *out);
void vec2f_mult_f(vec2f_t *a, float b, vec2f_t *out);

// angle of a line given by two points in relation to the x axis
float angle_of_segment(vec2f_t *p1, vec2f_t *p2);

void vec3f_from_vec3f(vec3f_t *in, vec3f_t *out);

void vec3f_normalize(vec3f_t *in, vec3f_t *out);
void vec3f_crossproduct(vec3f_t *a, vec3f_t *b, vec3f_t *out);

float vec3f_length(vec3f_t *a);
float vec3f_dot_vec3f(vec3f_t *a, vec3f_t *b);
void vec3f_mult_f(vec3f_t *a, float b,vec3f_t *out);
void vec3f_add_vec3f(vec3f_t *a, vec3f_t *b, vec3f_t *out);
void vec3f_sub_vec3f(vec3f_t *a, vec3f_t *b, vec3f_t *out);
