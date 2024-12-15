#pragma once
#include <lamath.h>
#include <stdint.h>
#include <d3d12.h>
#include <windows.h>

typedef struct dx_texture_t dx_texture_t;

void dx_init(HWND hwnd);
void dx_cleanup();
void dx_resize(HWND hwnd);
void dx_flush_commands();

mat4f_t *dx_get_ortho_proj();

size_t dx_get_video_memory_used();

uint32_t dx_get_backbuffer_count();
uint32_t dx_get_backbuffer_index();

dx_texture_t   *dx_texture_new_3d(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t depth, uint16_t levels);
dx_texture_t   *dx_texture_new_2d_array(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t size, uint16_t levels);
dx_texture_t   *dx_texture_new_2d(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t levels);
void            dx_texture_free(dx_texture_t *tex);
ID3D12Resource *dx_texture_get_upload_resource(dx_texture_t *tex);
void            dx_texture_set_name(dx_texture_t *tex, const char *fmt, ...);
void            dx_texture_copy_name(dx_texture_t *from, dx_texture_t *to);

void dx_texture_copy_subresources(dx_texture_t *from, dx_texture_t *to, uint32_t subresources);

ID3D12Resource *dx_create_vertex_buffer(size_t size);
ID3D12Resource *dx_create_upload_buffer(size_t size);

void dx_copy_resource(ID3D12Resource *from, ID3D12Resource *to);

void dx_texture_write_pixels(
    dx_texture_t *tex,
    uint32_t x,
    uint32_t y,
    uint32_t array_level,
    uint32_t w,
    uint32_t h,
    DXGI_FORMAT format,
    uint8_t *data
);

ID3D12DescriptorHeap *dx_create_descriptor_heap(
    D3D12_DESCRIPTOR_HEAP_TYPE  type,
    uint32_t                    num_descriptors,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags
);

void dx_object_set_name(void *object, const char *fmt, ...);
void dx_object_copy_name(void *from, void *to);

ID3D12RootSignature *dx_create_root_signature(const char *bytes, size_t len);
ID3D12PipelineState *dx_create_pipeline_state(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc);

void dx_get_render_target_size(uint32_t *width, uint32_t *height);

int dx_backbuffer_ready();

// All functions defined here between start_frame and end_frame can only be
// called between those two
void dx_start_frame();

void dx_set_pipeline_state(ID3D12PipelineState *pso);

void dx_set_root_constants(uint32_t index, uint32_t num, const void *data, uint32_t offset);

void dx_set_root_constant_mat4f (uint32_t index, mat4f_t *val, uint32_t offset);
void dx_set_root_constant_float4(uint32_t index, float   *val, uint32_t offset);
void dx_set_root_constant_float3(uint32_t index, float   *val, uint32_t offset);
void dx_set_root_constant_float (uint32_t index, float    val, uint32_t offset);
void dx_set_root_constant_uint  (uint32_t index, uint32_t val, uint32_t offset);

void dx_set_vertex_buffers(uint32_t slot, uint32_t numviews, const D3D12_VERTEX_BUFFER_VIEW *views);
void dx_set_descriptor_table(uint32_t index, D3D12_GPU_DESCRIPTOR_HANDLE base);
void dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY type);
void dx_set_texture(uint32_t index, dx_texture_t *tex);

void dx_draw_instanced(uint32_t vertexes, uint32_t instances, uint32_t first_vertex, uint32_t first_instance);

int dx_push_scissor(int32_t left, int32_t top, int32_t right, int32_t bottom);
void dx_pop_scissor();

int dx_push_viewport(float left, float top, float width, float height);
void dx_pop_viewport();

void dx_end_frame();

#ifdef _DEBUG
void dx_process_debug_messages();
#endif

