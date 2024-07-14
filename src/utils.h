#pragma once
#include "lamath.h"
#include <stdint.h>
#include <stdlib.h>

char *load_file(const char *path, size_t *length);

uint32_t djb2_hash_string(const char *string);

void push_child_viewport(int x, int y, int w, int h, int *old_vp, mat4f_t *vp_proj);
void pop_child_viewport(int *old_vp);

int push_scissor(int x, int y, int width, int height, int *old_scissor);
void pop_scissor(int *old_scissor);

#define UNUSED_PARAM(p) (void)p

void error_and_exit(const char *title, const char *msg_format, ...);