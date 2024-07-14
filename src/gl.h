#pragma once
#include <glad/gl.h>

typedef struct gl_shader_program_t gl_shader_program_t;

gl_shader_program_t *gl_shader_program_new();
void                 gl_shader_program_free(gl_shader_program_t *program);

GLuint gl_load_shader(const char *path, GLenum shader_type);

void gl_shader_program_attach_shader_file(gl_shader_program_t *program, const char *path, GLenum type);
void gl_shader_program_link(gl_shader_program_t *program);

void gl_shader_program_use(gl_shader_program_t *program);