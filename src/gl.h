#pragma once
#include <glad/gl.h>

#define VERT_ATTRIB_FLOATS(loc, count, div, type, memb, extra) \
    glEnableVertexAttribArray(loc);\
    glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, sizeof(type), (void*)(offsetof(type, memb) + extra));\
    glVertexAttribDivisor(loc, div)

#define VERT_ATTRIB_FLOAT(loc, div, type, memb) VERT_ATTRIB_FLOATS(loc, 1, div, type, memb, 0)
#define VERT_ATTRIB_VEC2(loc, div, type, memb) VERT_ATTRIB_FLOATS(loc, 2, div, type, memb, 0)
#define VERT_ATTRIB_VEC3(loc, div, type, memb) VERT_ATTRIB_FLOATS(loc, 3, div, type, memb, 0)
#define VERT_ATTRIB_VEC4(loc, div, type, memb) VERT_ATTRIB_FLOATS(loc, 4, div, type, memb, 0)

#define VERT_ATTRIB_UINT(loc, div, type, memb) \
    glEnableVertexAttribArray(loc);\
    glVertexAttribIPointer(loc, 1, GL_UNSIGNED_INT, sizeof(type), (void*)offsetof(type, memb));\
    glVertexAttribDivisor(loc, div)

#define VERT_ATTRIB_MAT4(loc, div, type, memb) \
    VERT_ATTRIB_FLOATS(loc    , 4, div, type, memb, 0                   );\
    VERT_ATTRIB_FLOATS(loc + 1, 4, div, type, memb, ( 4 * sizeof(float)));\
    VERT_ATTRIB_FLOATS(loc + 2, 4, div, type, memb, ( 8 * sizeof(float)));\
    VERT_ATTRIB_FLOATS(loc + 3, 4, div, type, memb, (12 * sizeof(float)))

typedef struct gl_shader_program_t gl_shader_program_t;

typedef struct {
    const char *path;
    GLenum type;
} gl_shader_source_list_t;

gl_shader_program_t *gl_shader_program_new();
gl_shader_program_t *gl_shader_program_new_with_sources(gl_shader_source_list_t *sources);
void                 gl_shader_program_free(gl_shader_program_t *program);

GLuint gl_load_shader(const char *path, GLenum shader_type);

void gl_add_shader_include(const char *path, const char *name);
void gl_del_shader_include(const char *name);

void gl_shader_program_attach_shader_file(gl_shader_program_t *program, const char *path, GLenum type);
void gl_shader_program_link(gl_shader_program_t *program);

void gl_shader_program_use(gl_shader_program_t *program);
