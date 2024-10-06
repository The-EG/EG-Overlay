#pragma once
#include <glad/gl.h>

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
