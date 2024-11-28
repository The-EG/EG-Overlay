#include "app.h"
#include "utils.h"
#include "logging/logger.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>
#include <glad/gl.h>

char *load_file(const char *path, size_t *length) {
    logger_t *log = logger_get("load_file");

    FILE *f = fopen(path, "rb");

    if (f==NULL) {
        logger_error(log, "Can't load file %s, does not exist.", path);
        *length = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);

    //logger_debug(log, "Read %d bytes from %s.", *length, path);

    char *data = egoverlay_calloc(*length, sizeof(char));
    fseek(f, 0, SEEK_SET);
    fread(data, sizeof(char), *length, f);

    fclose(f);

    return data;
}

// http://www.cse.yorku.ca/~oz/hash.html
uint32_t djb2_hash_string(const char *string) {
    uint32_t hash = 5381;
    
    int c;
    while ((c = *string++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

void push_child_viewport(int x, int y, int w, int h, int *old_vp, mat4f_t *vp_proj) {
    glGetIntegerv(GL_VIEWPORT, old_vp);

    int fb_w, fb_h;
    app_get_framebuffer_size(&fb_w, &fb_h);

    GLint new_vp_x = old_vp[0] + x;
    GLint new_vp_y = fb_h - (old_vp[1] + y + h);
    GLint new_vp_width = w; // <= (old_vp.w - win->x) ? win->width : (old_vp.w - win->x);
    GLint new_vp_height = h; //<= (old_vp.h - (win->y + win->height)) ? win->height : (old_vp.h - (win->y + win->height));

    glViewport(new_vp_x, new_vp_y, new_vp_width, new_vp_height);
    mat4f_ortho(vp_proj, 0.f, (float)new_vp_width, 0.f, (float)new_vp_height, -1.f, 1.f);
}

void pop_child_viewport(int *old_vp) {
    glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
}

int push_scissor(int x, int y, int width, int height, int *old_scissor) {
    
    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);

    int fb_w, fb_h;
    app_get_framebuffer_size(&fb_w, &fb_h);

    int sx = width > 0 ? x : 0;
    int sy = height > 0 ? fb_h - y - height : 0;
    int sw = width > 0 ? width : fb_w;
    int sh = height > 0 ? height : fb_h;

    if (x < 0) sw += x;
    if (sy < 0) sh += sy;

    if ((sx < old_scissor[0] || sx > (old_scissor[0] + old_scissor[2])) && 
        (sy < old_scissor[1] || sy > (old_scissor[1] + old_scissor[3]))) {
            return 0;
    }

    if (sx < old_scissor[0]) sx = old_scissor[0];
    if (sy < old_scissor[1]) sy = old_scissor[1];
    if (sw > old_scissor[2] - (sx - old_scissor[0])) sw = old_scissor[2] - (sx - old_scissor[0]);
    if (sh > old_scissor[3] - (sy - old_scissor[1])) sh = old_scissor[3] - (sy - old_scissor[1]);

    if (sh <= 0 || sw <=0) return 0;

    glScissor(sx, sy, sw, sh);

    return 1;
}

void pop_scissor(int *old_scissor) {
    glScissor(old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3]);
}

void error_and_exit(const char *title, const char *msg_format, ...) {
    size_t msglen;
    char *msgbuf;

    va_list args;
    va_start(args, msg_format);
    msglen = vsnprintf(NULL, 0, msg_format, args) + 1;
    va_end(args);

    // now allocate it and do the actual format
    msgbuf = egoverlay_calloc(msglen, sizeof(char));
    
    va_start(args, msg_format);
    vsnprintf(msgbuf, msglen, msg_format, args);
    va_end(args);

    MessageBox(NULL, msgbuf, title, MB_OK | MB_ICONERROR);
    egoverlay_free(msgbuf);
    exit(-1);
}

char *wchar_to_char(wchar_t *wstr) {
    int strsize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);

    if (strsize==0) return NULL;

    char *str = egoverlay_calloc(strsize, sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, strsize, NULL, NULL);

    return str;
}
