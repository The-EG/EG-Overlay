#define COBJMACROS
#include <stdint.h>
#include "dx.h"

#include "logging/logger.h"

#include <stdarg.h>
#include <stdio.h>

#include <windows.h>
#include <objbase.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#include "dcompwrap.h"

#include "utils.h"

#define DX_FRAMES 2

#define DX_SRV_DESCRIPTORS 2048

// a list of scissors used with dx_push_scissor and dx_pop_scissor
typedef struct dx_scissor_t {
    D3D12_RECT rect;
    
    struct dx_scissor_t *prev;
} dx_scissor_t;

// a list of viewports used with dx_push_viewport and dx_pop_viewport
typedef struct dx_viewport_t {
    D3D12_VIEWPORT viewport;

    struct dx_viewport_t *prev;
} dx_viewport_t;

typedef struct {
    IDXGIAdapter4 *adapter;
    ID3D12Device9 *device;

    ID3D12CommandQueue *cmdqueue;

    IDXGISwapChain4 *swapchain;
    HANDLE           swapchain_frame_handle;
    
    ID3D12Resource       *backbuffers[DX_FRAMES];
    ID3D12DescriptorHeap *rtvdescriptorheap;
    uint32_t              rtvdescriptorsize;

    ID3D12DescriptorHeap *srvdescriptorheap;
    uint32_t              srvdescriptorsize;

    // depth/stencil
    ID3D12Resource       *dsbuffer;
    ID3D12DescriptorHeap *dsdescriptorheap;

    size_t srvdescriptorheap_next;

    size_t *srvdescriptors_reuse;
    size_t  srvdescriptors_reuse_size;

    HANDLE rtv_mutex;

    ID3D12GraphicsCommandList *cmdlist;
    ID3D12CommandAllocator    *cmdallocs[DX_FRAMES];

    ID3D12CommandQueue        *copycmdqueue;
    ID3D12GraphicsCommandList *copycmdlist;
    ID3D12CommandAllocator    *copycmdalloc;
    ID3D12Fence               *copyfence;
    uint64_t                   copyfence_value;

    ID3D12Fence *fence;
    uint64_t     fence_values[DX_FRAMES];

    // Direct Composition
    IUnknown *comp_dev;
    IUnknown *comp_target;
    IUnknown *comp_visual;

    uint32_t rtv_width;
    uint32_t rtv_height;

    uint32_t frameind;

    logger_t *log;

    ID3D12RootSignature *rootsig;

    D3D12_RECT     base_scissor;
    D3D12_VIEWPORT base_viewport;

    mat4f_t ortho_proj;

    // the last dx_push_scissor
    dx_scissor_t *scissors;

    dx_viewport_t *viewports;

    #ifdef _DEBUG
    ID3D12Debug     *d3ddebug;
    IDXGIInfoQueue  *dxgiinfoqueue;
    IDXGIDebug      *dxgidebug;
    #endif
} dx_t;

struct dx_texture_t {
    uint32_t width;
    uint32_t height;
    uint16_t depth;
    uint16_t levels;
    ID3D12Resource *texture;
    size_t srvheap_loc;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle;
};

dx_t *dx = NULL;

typedef HRESULT (*fnDXGIGetDebugInterface)(REFIID, void**);

fnDXGIGetDebugInterface _DXGIGetDebugInterface = NULL;

void dx_debug_message(
    D3D12_MESSAGE_CATEGORY category,
    D3D12_MESSAGE_SEVERITY severity,
    D3D12_MESSAGE_ID       id,
    const char            *description,
    void                  *context
);

void dx_find_adapter();
void dx_create_device();
ID3D12CommandQueue *dx_create_command_queue(D3D12_COMMAND_LIST_TYPE type);
void dx_create_swap_chain(HWND hwnd);
void dx_create_dsbuffer();
void dx_update_rtvs();
void dx_execute_command_list(ID3D12GraphicsCommandList *list);
ID3D12CommandAllocator *dx_create_command_allocator(D3D12_COMMAND_LIST_TYPE type);
ID3D12GraphicsCommandList *dx_create_command_list(ID3D12CommandAllocator *alloc, D3D12_COMMAND_LIST_TYPE type);

size_t dx_get_srv_descriptor_loc();
void   dx_release_srv_descriptor_loc(size_t loc);

void dx_flush_copy_commands();

void dx_object_set_name_va(void *obj, const char *fmt, va_list args);

void dx_init(HWND hwnd) {
    dx = egoverlay_calloc(1, sizeof(dx_t));
    dx->log = logger_get("dx");

    logger_info(dx->log, "------------------------------------------------------------");
    logger_info(dx->log, "Initializing Direct3D 12...");

    #ifdef _DEBUG
    LoadLibrary("dxgidebug.dll");
    HMODULE dxgidebug = GetModuleHandle("dxgidebug.dll");
    _DXGIGetDebugInterface = (fnDXGIGetDebugInterface)GetProcAddress(dxgidebug, "DXGIGetDebugInterface");

    if (D3D12GetDebugInterface(&IID_ID3D12Debug, &dx->d3ddebug)!=S_OK) {
        logger_error(dx->log, "Couldn't get D3DDebug.");
        exit(-1);
    }

    if (_DXGIGetDebugInterface(&IID_IDXGIInfoQueue, &dx->dxgiinfoqueue)!=S_OK) {
        logger_error(dx->log, "Couldn't get DXGIInfoQueue.");
        exit(-1);
    }
    if (_DXGIGetDebugInterface(&IID_IDXGIDebug, &dx->dxgidebug)!=S_OK) {
        logger_error(dx->log, "Couldn't get DXGIDebug.");
        exit(-1);
    }
    logger_warn(dx->log, "D3D12 debug validation layer enabled.");
    ID3D12Debug_EnableDebugLayer(dx->d3ddebug);
    #endif
    
    logger_info(dx->log, "Backbuffers      : %d", DX_FRAMES);
    dx_find_adapter(); 
    dx_create_device();
    dx->cmdqueue = dx_create_command_queue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    dx_object_set_name(dx->cmdqueue, "EG-Overlay D3D12 Core Command Queue");

    if (ID3D12Device9_CreateFence(dx->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &dx->fence)!=S_OK) {
        logger_error(dx->log, "Failed to create fence.");
        exit(-1);
    }
    dx_object_set_name(dx->fence, "EG-Overlay D3D12 Command Queue Fence");

    dx_create_swap_chain(hwnd);
    dx->frameind = IDXGISwapChain4_GetCurrentBackBufferIndex(dx->swapchain);

    dx->rtvdescriptorheap = dx_create_descriptor_heap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DX_FRAMES,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );
    dx_object_set_name(dx->rtvdescriptorheap, "EG-Overlay D3D12 RTV Descriptor Heap");
    dx->rtvdescriptorsize = ID3D12Device9_GetDescriptorHandleIncrementSize(dx->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    dx->dsdescriptorheap = dx_create_descriptor_heap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );
    dx_object_set_name(dx->dsdescriptorheap, "EG-Overaly D3D12 Depth/Stencil Descriptor Heap");
    dx_create_dsbuffer();

    dx->srvdescriptorheap = dx_create_descriptor_heap(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DX_SRV_DESCRIPTORS,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    );
    dx_object_set_name(dx->srvdescriptorheap, "EG-Overlay D3D12 SRV Descriptor Heap");
    dx->srvdescriptorsize = ID3D12Device9_GetDescriptorHandleIncrementSize(dx->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    float srvdescsize = (float)dx->srvdescriptorsize * DX_SRV_DESCRIPTORS / 1024.f;
    logger_debug(dx->log, "Allocated %0.2f KiB for CBV/SRV/UAV descriptors.", srvdescsize);

    dx_update_rtvs();

    dx->rtv_mutex = CreateMutex(NULL, 0, "EG-Overlay D3D RTVs Mutex");

    for (int i=0;i<DX_FRAMES;i++) {
        dx->cmdallocs[i] = dx_create_command_allocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
        dx_object_set_name(dx->cmdallocs[i], "EG-Overlay D3D12 Command Allocator #%d", i);
    }

    dx->cmdlist = dx_create_command_list(dx->cmdallocs[0], D3D12_COMMAND_LIST_TYPE_DIRECT);
    dx_object_set_name(dx->cmdlist, "EG-Overlay D3D12 Core Command List");
 
    dx->copycmdqueue = dx_create_command_queue(D3D12_COMMAND_LIST_TYPE_COPY);
    dx_object_set_name(dx->copycmdqueue, "EG-Overlay D3D12 Copy Command Queue");

    dx->copycmdalloc = dx_create_command_allocator(D3D12_COMMAND_LIST_TYPE_COPY);
    dx_object_set_name(dx->copycmdalloc, "EG-Overlay D3D12 Copy Command Allocator");

    dx->copycmdlist = dx_create_command_list(dx->copycmdalloc, D3D12_COMMAND_LIST_TYPE_COPY);
    dx_object_set_name(dx->copycmdlist, "EG-Overlay D3D12 Copy Command list");

    if (ID3D12Device9_CreateFence(dx->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &dx->copyfence)!=S_OK) {
        logger_error(dx->log, "Failed to create copy fence.");
        exit(-1);
    }
    dx_object_set_name(dx->fence, "EG-Overlay D3D12 Copy Command Queue Fence");

    size_t rootlen = 0;
    char *rootbytes = load_file("shaders/root-sig.cso", &rootlen);

    if (!rootbytes) {
        logger_error(dx->log, "Couldn't load root signature.");
        exit(-1);
    }

    dx->rootsig = dx_create_root_signature(rootbytes, rootlen);
    if (!dx->rootsig) {
        logger_error(dx->log, "Couldn't create root signature.");
        exit(-1);
    }
    dx_object_set_name(dx->rootsig, "EG-Overlay D3D12 Root Signature");
    egoverlay_free(rootbytes);

    logger_info(dx->log, "------------------------------------------------------------");

    #ifdef _DEBUG
    // don't print out messages to OutpuDebugString - this will be duplicated in CDB
    IDXGIInfoQueue_SetMuteDebugOutput(dx->dxgiinfoqueue, DXGI_DEBUG_ALL, TRUE);
    // since the above is disabled, process debug messages whenever exit() is called
    atexit((void(__cdecl*)(void))&dx_process_debug_messages);
    #endif
}

void dx_cleanup() {
    logger_debug(dx->log, "cleanup");

    if (dx->srvdescriptors_reuse) egoverlay_free(dx->srvdescriptors_reuse);

    ID3D12RootSignature_Release(dx->rootsig);

    ID3D12GraphicsCommandList_Release(dx->copycmdlist);
    ID3D12CommandAllocator_Release(dx->copycmdalloc);
    ID3D12CommandQueue_Release(dx->copycmdqueue);
    ID3D12Fence_Release(dx->copyfence);

    ID3D12GraphicsCommandList_Release(dx->cmdlist);

    for (int i=0;i<DX_FRAMES;i++) {
        ID3D12CommandAllocator_Release(dx->cmdallocs[i]);
        ID3D12Resource_Release(dx->backbuffers[i]);
    }

    CloseHandle(dx->rtv_mutex);

    ID3D12Resource_Release(dx->dsbuffer);
    ID3D12DescriptorHeap_Release(dx->dsdescriptorheap);

    ID3D12DescriptorHeap_Release(dx->srvdescriptorheap);
    ID3D12DescriptorHeap_Release(dx->rtvdescriptorheap);

    IUnknown_Release(dx->comp_visual);
    IUnknown_Release(dx->comp_target);
    IUnknown_Release(dx->comp_dev);
    IDXGISwapChain4_Release(dx->swapchain);
    ID3D12Fence_Release(dx->fence);
    ID3D12CommandQueue_Release(dx->cmdqueue);
    ID3D12Device9_Release(dx->device);
    IDXGIAdapter4_Release(dx->adapter);

    #ifdef _DEBUG
    logger_debug(dx->log, "D3D12 Live Objects:");
    logger_debug(dx->log, "------------------------------------------------------------");
    IDXGIDebug_ReportLiveObjects(dx->dxgidebug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL | DXGI_DEBUG_RLO_DETAIL);
    dx_process_debug_messages();
    logger_debug(dx->log, "------------------------------------------------------------");
    IDXGIInfoQueue_Release(dx->dxgiinfoqueue);
    IDXGIDebug_Release(dx->dxgidebug);
    ID3D12Debug_Release(dx->d3ddebug);
    #endif    

    egoverlay_free(dx);

    dx = NULL;
}

void dx_create_dsbuffer() {
    D3D12_HEAP_PROPERTIES dsvprops = {0};
    dsvprops.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    dsvprops.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    dsvprops.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc = {0};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment        = 0;
    desc.Width            = dx->rtv_width;
    desc.Height           = dx->rtv_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    D3D12_CLEAR_VALUE clear = {0};
    clear.Format               = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth   = 1.f;
    clear.DepthStencil.Stencil = 0;

    if (ID3D12Device9_CreateCommittedResource(
            dx->device,
            &dsvprops,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear,
            &IID_ID3D12Resource,
            &dx->dsbuffer)!=S_OK
    ) {
        logger_error(dx->log, "Failed to create Depth/Stencil buffer.");
        exit(-1);
    }
    dx_object_set_name(dx->dsbuffer, "EG-Overlay D3D Depth/Stencil Buffer");

    D3D12_CPU_DESCRIPTOR_HANDLE dsvhandle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->dsdescriptorheap, &dsvhandle);

    ID3D12Device9_CreateDepthStencilView(dx->device, dx->dsbuffer, NULL, dsvhandle);
}

int dx_backbuffer_ready() {
    if (WaitForSingleObjectEx(dx->swapchain_frame_handle, 0, 0)==WAIT_TIMEOUT) return 0;

    return 1;
}

size_t dx_get_video_memory_used() {
    DXGI_QUERY_VIDEO_MEMORY_INFO info = {0};

    if (IDXGIAdapter4_QueryVideoMemoryInfo(dx->adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)!=S_OK) {
        logger_error(dx->log, "Couldn't query video memory info.");
    }

    return info.CurrentUsage;
}

ID3D12Resource *dx_create_vertex_buffer(size_t size) {
    D3D12_HEAP_PROPERTIES props = {0};
    props.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc = {0};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment        = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Width            = size;
    desc.Height           = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *buffer = NULL;
    if (ID3D12Device9_CreateCommittedResource(
        dx->device, &props, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,
        NULL,
        &IID_ID3D12Resource, &buffer)!=S_OK) return NULL;

    return buffer;
}

ID3D12Resource *dx_create_upload_buffer(size_t size) {
    D3D12_HEAP_PROPERTIES props = {0};
    props.Type                 = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc = {0};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment        = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Width            = size;
    desc.Height           = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *upload = NULL;
    if (ID3D12Device9_CreateCommittedResource(
        dx->device, &props, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        &IID_ID3D12Resource, &upload)!=S_OK) return NULL;

    return upload;
}

void dx_copy_resource(ID3D12Resource *from, ID3D12Resource *to) {
    ID3D12CommandAllocator_Reset(dx->copycmdalloc);
    ID3D12GraphicsCommandList_Reset(dx->copycmdlist, dx->copycmdalloc, NULL);

    ID3D12GraphicsCommandList_CopyResource(dx->copycmdlist, to, from);

    if (ID3D12GraphicsCommandList_Close(dx->copycmdlist)!=S_OK) {
        logger_error(dx->log, "Couldn't close copy command list.");
        exit(-1);
    }

    ID3D12CommandQueue_ExecuteCommandLists(dx->copycmdqueue, 1, (ID3D12CommandList*const*)&dx->copycmdlist);

    dx_flush_copy_commands();
}

void dx_find_adapter() {
    IDXGIFactory6 *dxgifactory = NULL;
    uint32_t flags = 0;

    #ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
    #endif

    if (CreateDXGIFactory2(flags, &IID_IDXGIFactory6, &dxgifactory)!=S_OK) {
        logger_error(dx->log, "Couldn't get DXGI Factory.");
        exit(-1);
    }

    // Get the first 'high perf.' adapter. This should be the right one in pretty
    // much every case
    if (IDXGIFactory6_EnumAdapterByGpuPreference(
            dxgifactory,
            0,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            &IID_IDXGIAdapter4,
            &dx->adapter
        )!=S_OK
    ) {
        logger_error(dx->log, "Couldn't get adapter.");
        exit(-1);
    }

    IDXGIFactory6_Release(dxgifactory);

    DXGI_ADAPTER_DESC1 desc;
    IDXGIAdapter4_GetDesc1(dx->adapter, &desc);    

    uint16_t ver_prod  = 0;
    uint16_t ver_ver   = 0;
    uint16_t ver_sub   = 0;
    uint16_t ver_build = 0;

    HKEY hklmdx = {0};
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\DirectX", 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hklmdx);

    DWORD subkeystrlen = 0;
    RegQueryInfoKey(hklmdx, NULL, NULL, NULL, NULL, &subkeystrlen, NULL, NULL, NULL, NULL, NULL, NULL);

    char *subkeynm = egoverlay_calloc(subkeystrlen+1, sizeof(char));
    for (DWORD i=0;RegEnumKey(hklmdx, i, subkeynm, subkeystrlen+1)==ERROR_SUCCESS;i++) {
        LUID luid = {0};
        DWORD luidsize = sizeof(LUID);
        RegGetValue(hklmdx, subkeynm, "AdapterLuid", RRF_RT_QWORD, NULL, &luid, &luidsize);
        if (luid.LowPart==0 && luid.HighPart==0) continue;

        if (luid.LowPart==desc.AdapterLuid.LowPart && luid.HighPart==desc.AdapterLuid.HighPart) {
            LARGE_INTEGER driverver;
            DWORD dvsize = sizeof(LARGE_INTEGER);
            RegGetValue(hklmdx, subkeynm, "DriverVersion", RRF_RT_QWORD, NULL, &driverver, &dvsize);
            ver_prod  = HIWORD(driverver.HighPart);
            ver_ver   = LOWORD(driverver.HighPart);
            ver_sub   = HIWORD(driverver.LowPart);
            ver_build = LOWORD(driverver.LowPart);

            break;
        }
    }    

    egoverlay_free(subkeynm);
    RegCloseKey(hklmdx);

    DXGI_QUERY_VIDEO_MEMORY_INFO meminfo = {0};
    if (IDXGIAdapter4_QueryVideoMemoryInfo(dx->adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &meminfo)!=S_OK) {
        logger_error(dx->log, "Couldn't query adapter memory info.");
        exit(-1);
    }

    char *descstr = wchar_to_char(desc.Description);
    logger_info(dx->log, "GPU              : %04X:%04X rev. %X", desc.VendorId, desc.DeviceId, desc.Revision);
    logger_info(dx->log, "                   %s", descstr);
    logger_info(dx->log, "Driver Version   : %d.%d.%d.%d", ver_prod, ver_ver, ver_sub, ver_build);
    logger_info(dx->log, "Memory Budget    : %.2f MiB", (float)meminfo.Budget / 1024.f / 1024.f);
    egoverlay_free(descstr);

}

void dx_create_device() {
    if (D3D12CreateDevice((IUnknown*)dx->adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device9, &dx->device)!=S_OK) {
        logger_error(dx->log, "Couldn't create device.");
        exit(-1);
    }
    dx_object_set_name(dx->device, "EG-Overlay D3D12 Device");
    
    const D3D_FEATURE_LEVEL reqlevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_2
    };

    char *featlevel = "";
    char *hlslmodel = "";

    D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {0};
    levels.NumFeatureLevels = 5;
    levels.pFeatureLevelsRequested = reqlevels;
    if (ID3D12Device9_CheckFeatureSupport(dx->device, D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels))!=S_OK) {
        logger_error(dx->log, "CheckFeaturesupport failed.");
        exit(-1);
    }
    switch(levels.MaxSupportedFeatureLevel) {
        case D3D_FEATURE_LEVEL_11_0: featlevel = "11.0"; break;
        case D3D_FEATURE_LEVEL_11_1: featlevel = "11.1"; break;
        case D3D_FEATURE_LEVEL_12_0: featlevel = "12.0"; break;
        case D3D_FEATURE_LEVEL_12_1: featlevel = "12.1"; break;
        case D3D_FEATURE_LEVEL_12_2: featlevel = "12.2"; break;
        default:                     featlevel = "Unknown"; break;
    }
    logger_info(dx->log, "Max Feature Level: %s", featlevel);

    D3D12_FEATURE_DATA_SHADER_MODEL shadermodel = { D3D_HIGHEST_SHADER_MODEL };
    if (ID3D12Device9_CheckFeatureSupport(dx->device, D3D12_FEATURE_SHADER_MODEL, &shadermodel, sizeof(shadermodel))!=S_OK) {
        logger_error(dx->log, "CheckFeatureSupport failed.");
        exit(-1);
    }
    switch(shadermodel.HighestShaderModel) {
        case D3D_SHADER_MODEL_5_1: hlslmodel = "5.1"; break;
        case D3D_SHADER_MODEL_6_0: hlslmodel = "6.0"; break;
        case D3D_SHADER_MODEL_6_1: hlslmodel = "6.1"; break;
        case D3D_SHADER_MODEL_6_2: hlslmodel = "6.2"; break;
        case D3D_SHADER_MODEL_6_3: hlslmodel = "6.3"; break;
        case D3D_SHADER_MODEL_6_4: hlslmodel = "6.4"; break;
        case D3D_SHADER_MODEL_6_5: hlslmodel = "6.5"; break;
        case D3D_SHADER_MODEL_6_6: hlslmodel = "6.6"; break;
        case D3D_SHADER_MODEL_6_7: hlslmodel = "6.7"; break;
        default:                   hlslmodel = "Unknown"; break;
    }
    logger_info(dx->log, "HLSL Shader Model: %s", hlslmodel);

}

ID3D12CommandQueue *dx_create_command_queue(D3D12_COMMAND_LIST_TYPE type) {
    D3D12_COMMAND_QUEUE_DESC desc = {0};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    
    ID3D12CommandQueue *queue = NULL;

    if (ID3D12Device9_CreateCommandQueue(dx->device, &desc, &IID_ID3D12CommandQueue, &queue)!=S_OK) {
        logger_error(dx->log, "Failed to create command queue.");
        exit(-1);
    }

    return queue;
}

void dx_create_swap_chain(HWND hwnd) {
    IDXGIFactory6 *factory = NULL;
    uint32_t flags = 0;

    #ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
    #endif

    if (CreateDXGIFactory2(flags, &IID_IDXGIFactory6, &factory)!=S_OK) {
        logger_error(dx->log, "Couldn't create DXGIFactory6.");
        exit(-1);
    }

    RECT rect = {0};
    GetClientRect(hwnd, &rect);

    mat4f_ortho(&dx->ortho_proj, 0.f, (float)rect.right, 0.f, (float)rect.bottom, 0.f, 1.f);

    dx->rtv_width  = rect.right;
    dx->rtv_height = rect.bottom;

    dx->base_scissor.left   = 0;
    dx->base_scissor.right  = rect.right;
    dx->base_scissor.top    = 0;
    dx->base_scissor.bottom = rect.bottom;

    dx->base_viewport.TopLeftX = 0.f;
    dx->base_viewport.TopLeftY = 0.f;
    dx->base_viewport.Width    = (float)rect.right;
    dx->base_viewport.Height   = (float)rect.bottom;
    dx->base_viewport.MinDepth = 0.f;
    dx->base_viewport.MaxDepth = 1.f;

    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    desc.Width            = rect.right;
    desc.Height           = rect.bottom;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = DX_FRAMES;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags            = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain1 *sc1 = NULL;

    if (IDXGIFactory6_CreateSwapChainForComposition(factory, (IUnknown*)dx->cmdqueue, &desc, NULL, &sc1)!=S_OK) {
        logger_error(dx->log, "Failed to create swap chain for Direct Composition.");
        exit(-1);
    }
    IDXGIFactory6_Release(factory);

    dx_setup_dcomp(hwnd, (IUnknown*)sc1, &dx->comp_dev, &dx->comp_target, &dx->comp_visual);

    if (IDXGISwapChain1_QueryInterface(sc1, &IID_IDXGISwapChain4, &dx->swapchain)!=S_OK) {
        logger_error(dx->log, "Couldn't get IDXGISwapChain4.");
        exit(-1);
    }
    IDXGISwapChain1_Release(sc1);

    dx->swapchain_frame_handle = IDXGISwapChain4_GetFrameLatencyWaitableObject(dx->swapchain);
}

void dx_update_rtvs() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvhandle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->rtvdescriptorheap, &rtvhandle);

    for (int i=0;i<DX_FRAMES;i++) {
        ID3D12Resource *backbuffer;
        if (IDXGISwapChain4_GetBuffer(dx->swapchain, i, &IID_ID3D12Resource, &backbuffer)!=S_OK) {
            logger_error(dx->log, "Failed to create back buffer resource.");
            exit(-1);
        }
        dx_object_set_name(backbuffer, "EG-Overlay D3D12 Backbuffer RTV Resource #%d", i);

        ID3D12Device9_CreateRenderTargetView(dx->device, backbuffer, NULL, rtvhandle);
        dx->backbuffers[i] = backbuffer;
        rtvhandle.ptr += dx->rtvdescriptorsize;
    }
}

ID3D12DescriptorHeap *dx_create_descriptor_heap(
    D3D12_DESCRIPTOR_HEAP_TYPE  type,
    uint32_t                    num_descriptors,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags
) {
    ID3D12DescriptorHeap *heap = NULL;
    
    D3D12_DESCRIPTOR_HEAP_DESC desc = {0};
    desc.NumDescriptors = num_descriptors;
    desc.Type           = type;
    desc.Flags          = flags;

    if (ID3D12Device9_CreateDescriptorHeap(dx->device, &desc, &IID_ID3D12DescriptorHeap, &heap)!=S_OK) {
        logger_error(dx->log, "Failed to create descriptor heap.");
        exit(-1);
    }

    return heap;
}

ID3D12CommandAllocator *dx_create_command_allocator(D3D12_COMMAND_LIST_TYPE type) {
    ID3D12CommandAllocator *alloc = NULL;
    if (ID3D12Device9_CreateCommandAllocator(dx->device, type, &IID_ID3D12CommandAllocator, &alloc)!=S_OK) {
        logger_error(dx->log, "Couldn't create command allocator.");
        exit(-1);
    }

    return alloc;
}

ID3D12GraphicsCommandList *dx_create_command_list(ID3D12CommandAllocator *alloc, D3D12_COMMAND_LIST_TYPE type) {
    ID3D12GraphicsCommandList *list = NULL;
    if (
        ID3D12Device9_CreateCommandList(
            dx->device,
            0,
            type,
            alloc,
            NULL,
            &IID_ID3D12GraphicsCommandList,
            &list
        )!=S_OK
    ) {
        logger_error(dx->log, "Failed to create Graphics Command List.");
        exit(-1);
    }

    if (ID3D12GraphicsCommandList_Close(list)!=S_OK) {
        logger_error(dx->log, "Failed to close command list.");
        exit(-1);
    }

    return list;
}

uint32_t dx_get_backbuffer_count() {
    return DX_FRAMES;
}

uint32_t dx_get_backbuffer_index() {
    return dx->frameind;
}

void dx_resize(HWND hwnd) {
    if (dx==NULL) return;

    RECT rect = {0};
    GetClientRect(hwnd, &rect);

    uint32_t newwidth = rect.right;
    uint32_t newheight = rect.bottom;

    if (newwidth < 1) newwidth = 1;
    if (newheight < 1) newheight = 1;

    mat4f_ortho(&dx->ortho_proj, 0.f, (float)newwidth, 0.f, (float)newheight, 0.f, 1.f);

    if (dx->rtv_width == newwidth && dx->rtv_height == newheight) return;

    WaitForSingleObject(dx->rtv_mutex, INFINITE);

    dx_flush_commands();

    dx->rtv_width = newwidth;
    dx->rtv_height = newheight;

    dx->base_scissor.left   = 0;
    dx->base_scissor.right  = newwidth;
    dx->base_scissor.top    = 0;
    dx->base_scissor.bottom = newheight;

    dx->base_viewport.TopLeftX = 0.f;
    dx->base_viewport.TopLeftY = 0.f;
    dx->base_viewport.Width    = (float)newwidth;
    dx->base_viewport.Height   = (float)newheight;
    dx->base_viewport.MinDepth = 0.f;
    dx->base_viewport.MaxDepth = 1.f;

    for (int i=0;i<DX_FRAMES;i++) {
        ID3D12Resource_Release(dx->backbuffers[i]);
        dx->fence_values[i] = dx->fence_values[dx->frameind];
    }

    DXGI_SWAP_CHAIN_DESC desc = {0};
    if (IDXGISwapChain4_GetDesc(dx->swapchain, &desc)!=S_OK) {
        logger_error(dx->log, "Couldn't get swapchain description.");
        exit(-1);
    }

    if (IDXGISwapChain4_ResizeBuffers(dx->swapchain, DX_FRAMES, dx->rtv_width, dx->rtv_height, desc.BufferDesc.Format, desc.Flags)!=S_OK) {
        logger_error(dx->log, "Couldn't resize swapchain buffers.");
        exit(-1);
    }

    dx->frameind = IDXGISwapChain4_GetCurrentBackBufferIndex(dx->swapchain);

    dx_update_rtvs();

    ID3D12Resource_Release(dx->dsbuffer);
    dx_create_dsbuffer();
    
    ReleaseMutex(dx->rtv_mutex);
}

void dx_flush_commands() {
    uint64_t curval = dx->fence_values[dx->frameind];
    if (ID3D12CommandQueue_Signal(dx->cmdqueue, dx->fence, curval)!=S_OK) {
        logger_error(dx->log, "Couldn't signal command queue.");
        exit(-1);
    }

    dx->fence_values[dx->frameind]++;

    if (ID3D12Fence_SetEventOnCompletion(dx->fence, curval, NULL)!=S_OK) {
        logger_error(dx->log, "SetEventOnCompletion failed.");
        exit(-1);
    }
}

void dx_flush_copy_commands() {
    uint64_t curval = dx->copyfence_value;
    if (ID3D12CommandQueue_Signal(dx->copycmdqueue, dx->copyfence, curval)!=S_OK) {
        logger_error(dx->log, "Couldn't signal copy command queue.");
        exit(-1);
    }

    dx->copyfence_value++;

    if (ID3D12Fence_SetEventOnCompletion(dx->copyfence, curval, NULL)!=S_OK) {
        logger_error(dx->log, "SetEventOnCompletion failed.");
        exit(-1);
    }
}

mat4f_t *dx_get_ortho_proj() {
    return &dx->ortho_proj;
}

void dx_start_frame() {
    float clearcolor[] = {0.f, 0.f, 0.0f, 0.0f};

    while (!dx_backbuffer_ready()) Sleep(0);

    WaitForSingleObject(dx->rtv_mutex, INFINITE);

    ID3D12CommandAllocator *alloc = dx->cmdallocs[dx->frameind];
    ID3D12Resource *backbuffer = dx->backbuffers[dx->frameind];

    ID3D12CommandAllocator_Reset(alloc);
    ID3D12GraphicsCommandList_Reset(dx->cmdlist, alloc, NULL);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->rtvdescriptorheap, &rtv);
    rtv.ptr += dx->frameind * dx->rtvdescriptorsize;

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->dsdescriptorheap, &dsv);

    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backbuffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    ID3D12GraphicsCommandList_ResourceBarrier(dx->cmdlist, 1, &barrier);

    ID3D12GraphicsCommandList_SetDescriptorHeaps(dx->cmdlist, 1, &dx->srvdescriptorheap);
    
    ID3D12GraphicsCommandList_OMSetRenderTargets(dx->cmdlist, 1, &rtv, 0, &dsv);
    ID3D12GraphicsCommandList_ClearRenderTargetView(dx->cmdlist, rtv, clearcolor, 0, NULL);
    ID3D12GraphicsCommandList_ClearDepthStencilView(dx->cmdlist, dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, NULL);

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(dx->cmdlist, dx->rootsig);

    ID3D12GraphicsCommandList_RSSetViewports(dx->cmdlist, 1, &dx->base_viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(dx->cmdlist, 1, &dx->base_scissor);
}

void dx_set_texture(uint32_t index, dx_texture_t *tex) {
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(dx->cmdlist, 1 + index, tex->gpu_descriptor_handle);
}

void dx_set_pipeline_state(ID3D12PipelineState *pso) {
    ID3D12GraphicsCommandList_SetPipelineState(dx->cmdlist, pso);
}

inline void dx_set_root_constants(uint32_t index, uint32_t num, const void *data, uint32_t offset) {
    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(dx->cmdlist, index, num, data, offset);
}

void dx_set_root_constant_mat4f(uint32_t index, mat4f_t *val, uint32_t offset) {
    dx_set_root_constants(index, 16, val, offset);
}

void dx_set_root_constant_float4(uint32_t index, float *val, uint32_t offset) {
    dx_set_root_constants(index, 4, val, offset);
}

void dx_set_root_constant_float3(uint32_t index, float *val, uint32_t offset) {
    dx_set_root_constants(index, 3, val, offset);
}

void dx_set_root_constant_float(uint32_t index, float val, uint32_t offset) {
    dx_set_root_constants(index, 1, &val, offset);
}

void dx_set_root_constant_uint(uint32_t index, uint32_t val, uint32_t offset) {
    dx_set_root_constants(index, 1, &val, offset);
}

void dx_set_primitive_topology(D3D_PRIMITIVE_TOPOLOGY type) {
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(dx->cmdlist, type);
}

void dx_draw_instanced(uint32_t vertexes, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) {
    ID3D12GraphicsCommandList_DrawInstanced(dx->cmdlist, vertexes, instances, first_vertex, first_instance);
}

void dx_set_vertex_buffers(uint32_t slot, uint32_t numviews, const D3D12_VERTEX_BUFFER_VIEW *views) {
    ID3D12GraphicsCommandList_IASetVertexBuffers(dx->cmdlist, slot, numviews, views);
}

void dx_execute_command_list(ID3D12GraphicsCommandList *list) {
    ID3D12CommandQueue_ExecuteCommandLists(dx->cmdqueue, 1, (ID3D12CommandList **)&list);
}

int dx_push_scissor(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    int32_t scleft   = left;
    int32_t sctop    = top;
    int32_t scright  = right;
    int32_t scbottom = bottom;

    D3D12_RECT *r = (dx->scissors!=NULL ? &dx->scissors->rect : &dx->base_scissor);

    if (scleft   < r->left  ) scleft   = r->left;
    if (sctop    < r->top   ) sctop    = r->top;
    if (scright  > r->right ) scright  = r->right;
    if (scbottom > r->bottom) scbottom = r->bottom;

    if (scright  - scleft <= 0 ||
        scbottom - sctop  <= 0   ) return 0;

    dx_scissor_t *sc = egoverlay_calloc(1, sizeof(dx_scissor_t));
    sc->rect.left   = scleft;
    sc->rect.right  = scright;
    sc->rect.top    = sctop;
    sc->rect.bottom = scbottom;

    sc->prev = dx->scissors;
    dx->scissors = sc;

    ID3D12GraphicsCommandList_RSSetScissorRects(dx->cmdlist, 1, &sc->rect);

    return 1;
}

void dx_pop_scissor() {
    if (dx->scissors==NULL) return;

    dx_scissor_t *old = dx->scissors;
    dx->scissors = old->prev;
    egoverlay_free(old);

    if (dx->scissors) {
        ID3D12GraphicsCommandList_RSSetScissorRects(dx->cmdlist, 1, &dx->scissors->rect);
    } else {
        ID3D12GraphicsCommandList_RSSetScissorRects(dx->cmdlist, 1, &dx->base_scissor);
    }
}

int dx_push_viewport(float left, float top, float width, float height) {
    dx_viewport_t *vp = egoverlay_calloc(1, sizeof(dx_viewport_t));
    vp->viewport.TopLeftX = left;
    vp->viewport.TopLeftY = top;
    vp->viewport.Width    = width;
    vp->viewport.Height   = height;
    vp->viewport.MinDepth = 0.f;
    vp->viewport.MaxDepth = 1.f;

    vp->prev = dx->viewports;
    dx->viewports = vp;

    ID3D12GraphicsCommandList_RSSetViewports(dx->cmdlist, 1, &vp->viewport);

    return 1;
}

void dx_pop_viewport() {
    if (dx->viewports==NULL) return;

    dx_viewport_t *old = dx->viewports;
    dx->viewports = old->prev;
    egoverlay_free(old);

    if (dx->viewports) {
        ID3D12GraphicsCommandList_RSSetViewports(dx->cmdlist, 1, &dx->viewports->viewport);
    } else {
        ID3D12GraphicsCommandList_RSSetViewports(dx->cmdlist, 1, &dx->base_viewport);
    }
}

void dx_end_frame() {
    ID3D12Resource *backbuffer = dx->backbuffers[dx->frameind];

    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backbuffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;

    ID3D12GraphicsCommandList_ResourceBarrier(dx->cmdlist, 1, &barrier);
    if (ID3D12GraphicsCommandList_Close(dx->cmdlist)!=S_OK) {
        logger_error(dx->log, "Failed to close command list.");
        exit(-1);
    }

    dx_execute_command_list(dx->cmdlist);

    uint32_t r = 0;
    if ((r=IDXGISwapChain4_Present(dx->swapchain, 0, DXGI_PRESENT_ALLOW_TEARING))!=S_OK) {
        switch (r) {
        case DXGI_ERROR_DEVICE_RESET:
            logger_error(dx->log, "Present failed: device reset.");
            exit(-1);
        case DXGI_ERROR_DEVICE_REMOVED:
            logger_error(dx->log, "Present failed: device removed.");
            exit(-1);
        case DXGI_STATUS_OCCLUDED:
            logger_error(dx->log, "Present failed: occluded.");
            exit(-1);
        default:
            logger_error(dx->log, "Present failed: other error 0x%X", r);
            exit(-1);
        }
    }

    uint64_t fenceval = dx->fence_values[dx->frameind];
    if (ID3D12CommandQueue_Signal(dx->cmdqueue, dx->fence, fenceval)!=S_OK) {
        logger_error(dx->log, "Signal failed.");
        exit(-1);
    }
    dx->frameind = IDXGISwapChain4_GetCurrentBackBufferIndex(dx->swapchain);

    
    if (ID3D12Fence_GetCompletedValue(dx->fence) < dx->fence_values[dx->frameind]) {
        if (ID3D12Fence_SetEventOnCompletion(dx->fence, fenceval, NULL)!=S_OK) {
            logger_error(dx->log, "SetEventOnCompletion failed.");
            exit(-1);
        }
    }

    while (dx->scissors) {
        dx_scissor_t *p = dx->scissors->prev;
        egoverlay_free(dx->scissors);
        dx->scissors = p;
    }

    while (dx->viewports) {
        dx_viewport_t *p = dx->viewports->prev;
        egoverlay_free(dx->viewports);
        dx->viewports = p;
    }
    
    dx->fence_values[dx->frameind] = fenceval + 1;
    ReleaseMutex(dx->rtv_mutex);
}

ID3D12RootSignature *dx_create_root_signature(const char *bytes, size_t len) {
    ID3D12RootSignature *rootsig = NULL;

    if (ID3D12Device9_CreateRootSignature(dx->device, 0, bytes, len, &IID_ID3D12RootSignature, &rootsig)!=S_OK) {
        return NULL;
    }

    return rootsig;
}

ID3D12PipelineState *dx_create_pipeline_state(D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc) {
    ID3D12PipelineState *pso = NULL;

    desc->pRootSignature = dx->rootsig;

    if (ID3D12Device9_CreateGraphicsPipelineState(dx->device, desc, &IID_ID3D12PipelineState, &pso)!=S_OK) {
        return NULL;
    }

    return pso;
}

void dx_get_render_target_size(uint32_t *width, uint32_t *height) {
    *width = dx->rtv_width;
    *height = dx->rtv_height;
}

size_t dx_get_srv_descriptor_loc() {
    if (dx->srvdescriptors_reuse_size>0) {
        size_t loc = dx->srvdescriptors_reuse[dx->srvdescriptors_reuse_size-1];

        dx->srvdescriptors_reuse_size--;
        if (dx->srvdescriptors_reuse_size==0) {
            egoverlay_free(dx->srvdescriptors_reuse);
            dx->srvdescriptors_reuse = NULL;
        } else {
            egoverlay_realloc(dx->srvdescriptors_reuse, sizeof(size_t) * dx->srvdescriptors_reuse_size);
        }

        return loc;
    } else {
        size_t loc  = dx->srvdescriptorheap_next;
        dx->srvdescriptorheap_next += dx->srvdescriptorsize;
        return loc;
    }
}

void dx_release_srv_descriptor_loc(size_t loc) {
    dx->srvdescriptors_reuse = egoverlay_realloc(dx->srvdescriptors_reuse, sizeof(size_t) * (dx->srvdescriptors_reuse_size+1));

    dx->srvdescriptors_reuse[dx->srvdescriptors_reuse_size++] = loc;
}

dx_texture_t *dx_texture_new_3d(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t depth, uint16_t levels) {
    dx_texture_t *tex = egoverlay_calloc(1, sizeof(dx_texture_t));

    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->levels = levels;
    
    D3D12_HEAP_PROPERTIES heapprops = {0};
    heapprops.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapprops.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprops.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_HEAP_FLAGS heapflag = D3D12_HEAP_FLAG_NONE;
    
    D3D12_RESOURCE_DESC resdesc = {0};
    resdesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    resdesc.Alignment        = 0;
    resdesc.Width            = tex->width;
    resdesc.Height           = tex->height;
    resdesc.DepthOrArraySize = tex->depth;
    resdesc.MipLevels        = tex->levels;
    resdesc.Format           = format;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resdesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    if (
        ID3D12Device9_CreateCommittedResource(
            dx->device,
            &heapprops,
            heapflag,
            &resdesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            NULL,
            &IID_ID3D12Resource,
            &tex->texture
        )!=S_OK
    ) {
        logger_error(dx->log, "Failed to create texture resource.");
        exit(-1);
    }

    tex->srvheap_loc = dx_get_srv_descriptor_loc();

    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &tex->gpu_descriptor_handle);
    tex->gpu_descriptor_handle.ptr += tex->srvheap_loc;

    D3D12_CPU_DESCRIPTOR_HANDLE texsrvhandle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &texsrvhandle);
    texsrvhandle.ptr += tex->srvheap_loc;

    ID3D12Device9_CreateShaderResourceView(dx->device, tex->texture, NULL, texsrvhandle);

    return tex;
}

dx_texture_t *dx_texture_new_2d_array(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t size, uint16_t levels) {
    dx_texture_t *tex = egoverlay_calloc(1, sizeof(dx_texture_t));

    tex->width = width;
    tex->height = height;
    tex->depth = size;
    tex->levels = levels;
    
    D3D12_HEAP_PROPERTIES heapprops = {0};
    heapprops.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapprops.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprops.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_HEAP_FLAGS heapflag = D3D12_HEAP_FLAG_NONE;
    
    D3D12_RESOURCE_DESC resdesc = {0};
    resdesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resdesc.Alignment        = 0;
    resdesc.Width            = tex->width;
    resdesc.Height           = tex->height;
    resdesc.DepthOrArraySize = tex->depth;
    resdesc.MipLevels        = tex->levels;
    resdesc.Format           = format;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resdesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    if (
        ID3D12Device9_CreateCommittedResource(
            dx->device,
            &heapprops,
            heapflag,
            &resdesc,
            D3D12_RESOURCE_STATE_COMMON,
            NULL,
            &IID_ID3D12Resource,
            &tex->texture
        )!=S_OK
    ) {
        logger_error(dx->log, "Failed to create texture resource.");
        exit(-1);
    }

    tex->srvheap_loc = dx_get_srv_descriptor_loc();

    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &tex->gpu_descriptor_handle);
    tex->gpu_descriptor_handle.ptr += tex->srvheap_loc;

    D3D12_CPU_DESCRIPTOR_HANDLE texsrvhandle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &texsrvhandle);
    texsrvhandle.ptr += tex->srvheap_loc;

    ID3D12Device9_CreateShaderResourceView(dx->device, tex->texture, NULL, texsrvhandle);

    return tex;
}

dx_texture_t *dx_texture_new_2d(DXGI_FORMAT format, uint32_t width, uint32_t height, uint16_t levels) {
    dx_texture_t *tex = egoverlay_calloc(1, sizeof(dx_texture_t));

    tex->width = width;
    tex->height = height;
    tex->depth = 1;
    tex->levels = levels;
    
    D3D12_HEAP_PROPERTIES heapprops = {0};
    heapprops.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heapprops.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprops.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_HEAP_FLAGS heapflag = D3D12_HEAP_FLAG_NONE;
    
    D3D12_RESOURCE_DESC resdesc = {0};
    resdesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resdesc.Alignment        = 0;
    resdesc.Width            = tex->width;
    resdesc.Height           = tex->height;
    resdesc.DepthOrArraySize = tex->depth;
    resdesc.MipLevels        = tex->levels;
    resdesc.Format           = format;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resdesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    if (
        ID3D12Device9_CreateCommittedResource(
            dx->device,
            &heapprops,
            heapflag,
            &resdesc,
            D3D12_RESOURCE_STATE_COMMON,
            NULL,
            &IID_ID3D12Resource,
            &tex->texture
        )!=S_OK
    ) {
        logger_error(dx->log, "Failed to create texture resource.");
        exit(-1);
    }

    tex->srvheap_loc = dx_get_srv_descriptor_loc();

    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &tex->gpu_descriptor_handle);
    tex->gpu_descriptor_handle.ptr += tex->srvheap_loc;

    D3D12_CPU_DESCRIPTOR_HANDLE texsrvhandle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dx->srvdescriptorheap, &texsrvhandle);
    texsrvhandle.ptr += tex->srvheap_loc;

    ID3D12Device9_CreateShaderResourceView(dx->device, tex->texture, NULL, texsrvhandle);

    return tex;
}

void dx_texture_free(dx_texture_t *texture) {
    ID3D12Resource_Release(texture->texture);

    dx_release_srv_descriptor_loc(texture->srvheap_loc);

    egoverlay_free(texture);
}

void dx_texture_write_pixels(
    dx_texture_t *tex,
    uint32_t x,
    uint32_t y,
    uint32_t array_level,
    uint32_t w,
    uint32_t h,
    DXGI_FORMAT format,
    uint8_t *data
) {
    uint8_t bpp = 4;
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        bpp = 4;
        break;
    case DXGI_FORMAT_R8_UNORM:
        bpp = 1;
        break;
    default:
        logger_error(dx->log, "Format not implemented.");
        exit(-1);
    }

    uint32_t rowwidth = w * bpp;
    uint32_t rowpitch = rowwidth + (rowwidth % 256);

    D3D12_HEAP_PROPERTIES uploadprops = {0};
    uploadprops.Type                 = D3D12_HEAP_TYPE_UPLOAD;
    uploadprops.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadprops.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC uploaddesc = {0};
    uploaddesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploaddesc.Alignment        = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    uploaddesc.Width            = rowpitch * h;
    uploaddesc.Height           = 1;
    uploaddesc.DepthOrArraySize = 1;
    uploaddesc.MipLevels        = 1;
    uploaddesc.Format           = DXGI_FORMAT_UNKNOWN;
    uploaddesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploaddesc.SampleDesc.Count = 1;
    uploaddesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *upload = NULL;

    if (ID3D12Device9_CreateCommittedResource(
            dx->device, &uploadprops, D3D12_HEAP_FLAG_NONE, &uploaddesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource, &upload)!=S_OK
    ) {
        logger_error(dx->log, "Couldn't create upload resource.");
        exit(1);
    }

    D3D12_RANGE rr = {0,0};

    uint8_t *uploaddata = NULL;
    if (ID3D12Resource_Map(upload, 0, &rr, &uploaddata)!=S_OK) {
        logger_error(dx->log, "Couldn't map upload data.");
        exit(-1);
    }

    for (uint32_t yi=0;yi<h;yi++) {
        uint8_t *line = uploaddata + (yi * rowpitch);
        memcpy(line, &data[yi * w * sizeof(uint8_t) * bpp], sizeof(uint8_t) * bpp * w);
    }

    ID3D12Resource_Unmap(upload, 0, NULL);

    D3D12_TEXTURE_COPY_LOCATION srcloc = {0};
    srcloc.pResource = upload;
    srcloc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    srcloc.PlacedFootprint.Offset = 0;

    srcloc.PlacedFootprint.Footprint.Format   = format;
    srcloc.PlacedFootprint.Footprint.Width    = w;
    srcloc.PlacedFootprint.Footprint.Height   = h;
    srcloc.PlacedFootprint.Footprint.Depth    = 1;
    srcloc.PlacedFootprint.Footprint.RowPitch = rowpitch;

    D3D12_TEXTURE_COPY_LOCATION dstloc = {0};
    dstloc.pResource        = tex->texture;
    dstloc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstloc.SubresourceIndex = array_level;

    dx_flush_commands();

    ID3D12CommandAllocator_Reset(dx->copycmdalloc);
    ID3D12GraphicsCommandList_Reset(dx->copycmdlist, dx->copycmdalloc, NULL);

    ID3D12GraphicsCommandList_CopyTextureRegion(dx->copycmdlist, &dstloc, x, y, 0, &srcloc, NULL);

    if (ID3D12GraphicsCommandList_Close(dx->copycmdlist)!=S_OK) {
        logger_error(dx->log, "Couldn't close copy command list.");
        exit(-1);
    }

    ID3D12CommandQueue_ExecuteCommandLists(dx->copycmdqueue, 1, (ID3D12CommandList*const*)&dx->copycmdlist);

    dx_flush_copy_commands();

    ID3D12Resource_Release(upload);
}

void dx_texture_copy_subresources(dx_texture_t *from, dx_texture_t *to, uint32_t subresources) {
    ID3D12CommandAllocator_Reset(dx->copycmdalloc);
    ID3D12GraphicsCommandList_Reset(dx->copycmdlist, dx->copycmdalloc, NULL);

    for (uint32_t r=0;r<subresources;r++) {
        D3D12_TEXTURE_COPY_LOCATION srcloc = {0};
        D3D12_TEXTURE_COPY_LOCATION dstloc = {0};
        
        srcloc.pResource        = from->texture;
        srcloc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcloc.SubresourceIndex = r;

        dstloc.pResource        = to->texture;
        dstloc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstloc.SubresourceIndex = r;

        ID3D12GraphicsCommandList_CopyTextureRegion(dx->copycmdlist, &dstloc, 0, 0, 0, &srcloc, NULL);
    }

    if (ID3D12GraphicsCommandList_Close(dx->copycmdlist)!=S_OK) {
        logger_error(dx->log, "Couldn't close copy command list.");
        exit(-1);
    }

    ID3D12CommandQueue_ExecuteCommandLists(dx->copycmdqueue, 1, (ID3D12CommandList*const*)&dx->copycmdlist);

    dx_flush_copy_commands();
}

void dx_texture_set_name(dx_texture_t *tex, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    dx_object_set_name_va(tex->texture, fmt, args);
    va_end(args);
}

void dx_texture_copy_name(dx_texture_t *from, dx_texture_t *to) {
    dx_object_copy_name(from->texture, to->texture);
}

#ifdef _DEBUG
void dx_log_dxgi_message(DXGI_INFO_QUEUE_MESSAGE *msg) {
    char *catstr = "";
    char *sevstr = "";

    enum LOGGER_LEVEL loglevel = 0;

    switch (msg->Category) {
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_UNKNOWN              : catstr = "Unknown"              ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_MISCELLANEOUS        : catstr = "Miscellaneous"        ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_INITIALIZATION       : catstr = "Initialization"       ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_CLEANUP              : catstr = "Cleanup"              ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_COMPILATION          : catstr = "Compilation"          ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_STATE_CREATION       : catstr = "State Creation"       ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_STATE_SETTING        : catstr = "State Setting"        ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_STATE_GETTING        : catstr = "State Getting"        ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_RESOURCE_MANIPULATION: catstr = "Resource Manipulation"; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_EXECUTION            : catstr = "Execution"            ; break;
    case DXGI_INFO_QUEUE_MESSAGE_CATEGORY_SHADER               : catstr = "Shader"               ; break;
    }

    switch (msg->Severity) {
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION: sevstr = "CORRUPTION"; loglevel = LOGGER_LEVEL_ERROR  ; break;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR     : sevstr = "ERROR"     ; loglevel = LOGGER_LEVEL_ERROR  ; break;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING   : sevstr = "WARNING"   ; loglevel = LOGGER_LEVEL_WARNING; break;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO      : sevstr = "INFO"      ; loglevel = LOGGER_LEVEL_INFO   ; break;
    case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE   : sevstr = "MESSAGE"   ; loglevel = LOGGER_LEVEL_DEBUG  ; break;
    }
 
    logger_log(dx->log, loglevel, "%s %s (%d): %s", catstr, sevstr, msg->ID, msg->pDescription);
}

void dx_process_debug_messages() {
    if (!dx) return;

    size_t dxgimessages = IDXGIInfoQueue_GetNumStoredMessagesAllowedByRetrievalFilters(dx->dxgiinfoqueue, DXGI_DEBUG_ALL);

    for (size_t m=0;m<dxgimessages;m++) {
        size_t msglen = 0;
        IDXGIInfoQueue_GetMessage(dx->dxgiinfoqueue, DXGI_DEBUG_ALL, m, NULL, &msglen);
        DXGI_INFO_QUEUE_MESSAGE *msg = egoverlay_malloc(msglen);
        if (IDXGIInfoQueue_GetMessage(dx->dxgiinfoqueue, DXGI_DEBUG_ALL, m, msg, &msglen)!=S_OK) {
            logger_error(dx->log, "Couldn't get message.");
            exit(-1);
        }

        dx_log_dxgi_message(msg);
        egoverlay_free(msg);
    }
    IDXGIInfoQueue_ClearStoredMessages(dx->dxgiinfoqueue, DXGI_DEBUG_ALL);
}
#endif

void dx_object_set_name(IUnknown *object, const char *fmt, ...) {
    ID3D12Object *obj = NULL;

    if (IUnknown_QueryInterface(object, &IID_ID3D12Object, &obj)!=S_OK) {
        logger_error(dx->log, "object is not an ID3D12Object.");
        exit(-1);
    }

    char *namebuf = NULL;
    size_t namelen = 0;

    va_list args;
    va_start(args, fmt);
    namelen = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    namebuf = egoverlay_calloc(namelen, sizeof(char));

    va_start(args, fmt);
    vsnprintf(namebuf, namelen, fmt, args);
    va_end(args);

    wchar_t *wname = char_to_wchar(namebuf);
    egoverlay_free(namebuf);

    ID3D12Object_SetName(obj, wname);
    egoverlay_free(wname);
    ID3D12Object_Release(obj);
}

void dx_object_set_name_va(IUnknown *object, const char *fmt, va_list args) {
    ID3D12Object *obj = NULL;

    if (IUnknown_QueryInterface(object, &IID_ID3D12Object, &obj)!=S_OK) {
        logger_error(dx->log, "object is not an ID3D12Object.");
        exit(-1);
    }

    char *namebuf = NULL;
    size_t namelen = 0;

    namelen = vsnprintf(NULL, 0, fmt, args) + 1;

    namebuf = egoverlay_calloc(namelen, sizeof(char));

    vsnprintf(namebuf, namelen, fmt, args);

    ID3D12Object_SetPrivateData(obj, &WKPDID_D3DDebugObjectName, (uint32_t)namelen, namebuf);
    egoverlay_free(namebuf);

    ID3D12Object_Release(obj);
}

void dx_object_copy_name(IUnknown *from, IUnknown *to) {
    uint32_t namelen = 0;

    ID3D12Object *fromobj = NULL;

    if (IUnknown_QueryInterface(from, &IID_ID3D12Object, &fromobj)!=S_OK) {
        logger_error(dx->log, "from is not an ID3D12Object.");
        exit(-1);
    }

    ID3D12Object *toobj = NULL;

    if (IUnknown_QueryInterface(to, &IID_ID3D12Object, &toobj)!=S_OK) {
        logger_error(dx->log, "to is not an ID3D12Object.");
        exit(-1);
    }

    ID3D12Object_GetPrivateData(fromobj, &WKPDID_D3DDebugObjectName, &namelen, NULL);

    char *name = calloc(namelen+1, sizeof(char));
    ID3D12Object_GetPrivateData(fromobj, &WKPDID_D3DDebugObjectName, &namelen, name);
    egoverlay_free(name);

    ID3D12Object_Release(toobj);
    ID3D12Object_Release(fromobj);
}
