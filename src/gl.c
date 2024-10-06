#include "gl.h"
#include "utils.h"
#include "logging/logger.h"
#include <string.h>

struct gl_shader_program_t {
    GLuint *shaders;
    size_t shader_count;

    GLuint program;
};

GLuint gl_load_shader(const char *path, GLenum shader_type) {
    logger_t *log = logger_get("gl");

    size_t src_len = 0;
    char *src = load_file(path, &src_len);

    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &src, (const GLint*)&src_len);
    egoverlay_free(src);

    glCompileShader(shader);
   
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint log_len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        char *log_msg = egoverlay_calloc(log_len + 1, sizeof(char));
        glGetShaderInfoLog(shader, log_len, NULL, log_msg);
        logger_error(log, "Error while compiling %s:\n%s", path, log_msg);
        error_and_exit("EG-Overlay: GL", "Error while compiling %s:\n%s", path, log_msg);
        egoverlay_free(log_msg); // never happens
    } else {
        logger_debug(log, "Shader compiled: %s", path);
    }

    return shader;
}

void gl_add_shader_include(const char *path, const char *name) {
    logger_t *log = logger_get("gl");

    size_t src_len = 0;
    char *src = load_file(path, &src_len);
    
    glNamedStringARB(GL_SHADER_INCLUDE_ARB, -1, name, (GLint)src_len, src);

    egoverlay_free(src);

    logger_debug(log, "Shader include %s loaded to %s", path, name);
}

void gl_del_shader_include(const char *name) {
    glDeleteNamedStringARB(-1, name);
}

gl_shader_program_t *gl_shader_program_new() {
    gl_shader_program_t *p = egoverlay_calloc(1, sizeof(gl_shader_program_t));

    p->program = glCreateProgram();

    return p;
}

gl_shader_program_t *gl_shader_program_new_with_sources(gl_shader_source_list_t *sources) {
    gl_shader_program_t *p = gl_shader_program_new();

    while (sources->path) {
        gl_shader_program_attach_shader_file(p, sources->path, sources->type);
        sources++;
    }

    gl_shader_program_link(p);

    return p;
}

void gl_shader_program_free(gl_shader_program_t *program) {

    for (size_t s=0;s<program->shader_count;s++) {
        glDetachShader(program->program, program->shaders[s]);
        glDeleteShader(program->shaders[s]);
    }
    egoverlay_free(program->shaders);

    glDeleteProgram(program->program);
    logger_t *logger = logger_get("gl");
    logger_debug(logger, "Shader program %d deleted.", program->program);
    egoverlay_free(program);
}

void gl_shader_program_attach_shader_file(gl_shader_program_t *program, const char *path, GLenum type) {
    logger_t *logger = logger_get("gl");

    GLuint shader = gl_load_shader(path, type);
    glAttachShader(program->program, shader);

    program->shaders = egoverlay_realloc(program->shaders, sizeof(GLuint) * (program->shader_count + 1));
    program->shaders[program->shader_count] = shader;
    program->shader_count++;

    logger_debug(
        logger,
        "Shader %d attached to program %d at stage %d from %s.",
        shader,
        program->program,
        type,
        path
    );
}

void gl_shader_program_link(gl_shader_program_t *program) {
    logger_t *logger = logger_get("gl");
    glLinkProgram(program->program);

    GLint linked;
    glGetProgramiv(program->program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len;
        glGetProgramiv(program->program, GL_INFO_LOG_LENGTH, &len);
        char *log_msg = egoverlay_calloc(len+1, sizeof(char));
        glGetProgramInfoLog(program->program, len, NULL, log_msg);

        logger_error(logger, "Error while linking shader program %d:\n%s", program->program, log_msg);
        error_and_exit("EG-Overlay: GL", "Error while linking shader program %d:\n%s", program->program, log_msg);
        egoverlay_free(log_msg); // never happens
    } else {
        logger_debug(logger, "Shader program %d linked successfully.", program->program);
    }
}

void gl_shader_program_use(gl_shader_program_t *program) {
    glUseProgram(program->program);
}
