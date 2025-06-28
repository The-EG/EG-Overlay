// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Direct3D12 API

pub mod lua;

use std::sync::Mutex;
use std::sync::Arc;
use std::sync::MutexGuard;

use std::collections::VecDeque;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lamath;
use crate::overlay;

use windows::Win32::Foundation;

use windows::Win32::Graphics::Direct3D;
use windows::Win32::Graphics::Direct3D12;
use windows::Win32::Graphics::Dxgi;
use windows::Win32::Graphics::DirectComposition;

use windows::Win32::System::Registry;

use windows::Win32::UI::WindowsAndMessaging;

// needed to access the IID of interfaces, ::from and others
use windows::core::Interface;

/// The number of framebuffer frames.
///
/// This must be at least 2 since one frame will be in use while it is being
/// displayed and can't be drawn to.
const DX_FRAMES: u32 = 2;

/// The number of SRV descriptors to allocate heap space for.
///
/// These descriptors are needed for textures, VBOs and other dynamic shader data
/// that can't be set directly in the root signature.
const DX_SRV_DESCRIPTORS: u32 = 2048;

/// Report D3D12 objects that are still alive.
///
/// This will output any D3D12 objects that still have active references to them
/// to the debugger, NOT the log.
pub fn report_live_objects() {
    // and then dump any live objects that survived, which should be none
    if cfg!(debug_assertions) {
        unsafe {
            let dxgi_debug = Dxgi::DXGIGetDebugInterface1::<Dxgi::IDXGIDebug>(0).expect("Couldn't get DXGIDebug.");

            debug!("D3D12 Live Objects:");
            debug!("------------------------------------------------------------");
            dxgi_debug.ReportLiveObjects(
                Dxgi::DXGI_DEBUG_ALL,
                Dxgi::DXGI_DEBUG_RLO_IGNORE_INTERNAL | Dxgi::DXGI_DEBUG_RLO_DETAIL
            ).unwrap();
            debug!("------------------------------------------------------------");
        }
    }
}

/// The main Direct3D12 state.
pub struct Dx {
    adapter: Dxgi::IDXGIAdapter4,
    device: Direct3D12::ID3D12Device9,

    swapchain: Mutex<SwapChain>,

    copy_queue: Mutex<CopyQueue>,

    srv_descriptorheap      : Direct3D12::ID3D12DescriptorHeap,
    srv_descriptorsize      : u32,
    srv_descriptorheap_addresses: Mutex<DescriptorHeapAddresses>,
}

/// A record representing the next and resusable addresses in a descriptor heap.
///
/// This is separate so it can be in a [Mutex].
struct DescriptorHeapAddresses {
    next: u64,
    reuse: VecDeque<u64>,
}

impl Dx {

    /// Initializes Direct3D12.
    ///
    /// This should only be called once. Direct3D12 will be cleaned up when the
    /// returned Dx is dropped.
    pub fn new() -> Arc<Dx> {
        info!("------------------------------------------------------------");
        info!("Initializing Direct3D 12...");

        if cfg!(debug_assertions) {
            enable_debug_layer();
        }
        
        let adapter = find_adapter();
        let device  = create_device(&adapter);

        let swapchain = Mutex::new(create_swapchain(&device, overlay::hwnd()));
        let copy_queue = Mutex::new(create_copyqueue(&device));

        let srv_heap = create_descriptor_heap(
            &device,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            DX_SRV_DESCRIPTORS,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        );
        object_set_name(&srv_heap, "EG-Overlay D3D12 SRV Descriptor Heap");
        let srv_descriptorsize: u32 = unsafe {
            device.GetDescriptorHandleIncrementSize(Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        };

        let srvdescsize = ((srv_descriptorsize * DX_SRV_DESCRIPTORS) as f32) / 1024.0;
        debug!("Allocated {:.2} KiB for CBV/SRV/UAV descriptors.", srvdescsize);

        info!("------------------------------------------------------------");

        return Arc::new(Dx {
            adapter: adapter,
            device : device,

            swapchain: swapchain,

            copy_queue: copy_queue,

            srv_descriptorheap: srv_heap,
            srv_descriptorsize: srv_descriptorsize,
            srv_descriptorheap_addresses: Mutex::new( DescriptorHeapAddresses {
                next: 0,
                reuse: VecDeque::new(),
            }),
        });
    }

    /// Starts a frame.
    ///
    /// This locks the swapchain and related resources. If a backbuffer isn't
    /// ready for rendering this will return [None] and the overlay can do other
    /// processing instead.
    pub fn start_frame(&self) -> Option<SwapChainLock<'_>> {
        let swapchain = self.swapchain.lock().unwrap();

        if !swapchain.backbuffer_ready() { return None; }

        let clear_color: [f32;4] = [0.0, 0.0, 0.0, 0.0];
            
        let alloc = &swapchain.cmd_allocs[swapchain.frameind as usize];
        let backbuffer = &swapchain.backbuffers[swapchain.frameind as usize];

        unsafe {
            alloc.Reset().unwrap();
            swapchain.cmd_list.Reset(alloc, None).unwrap();

            let mut rtv = swapchain.rtv_descriptorheap.GetCPUDescriptorHandleForHeapStart();

            rtv.ptr += (swapchain.frameind * swapchain.rtv_descriptorsize) as usize;

            let dsv = swapchain.ds_descriptorheap.GetCPUDescriptorHandleForHeapStart();
            
            let mut barrier = Direct3D12::D3D12_RESOURCE_BARRIER::default();
            barrier.Type = Direct3D12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = Direct3D12::D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Anonymous.Transition = std::mem::ManuallyDrop::new(Direct3D12::D3D12_RESOURCE_TRANSITION_BARRIER {
                pResource: std::mem::transmute_copy(backbuffer),
                Subresource: Direct3D12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                StateBefore: Direct3D12::D3D12_RESOURCE_STATE_PRESENT,
                StateAfter: Direct3D12::D3D12_RESOURCE_STATE_RENDER_TARGET,
            });

            swapchain.cmd_list.ResourceBarrier(&[barrier]);
            swapchain.cmd_list.SetDescriptorHeaps(&[Some(self.srv_descriptorheap.clone())]);
            swapchain.cmd_list.SetGraphicsRootSignature(&swapchain.rootsig);
            swapchain.cmd_list.OMSetRenderTargets(1, Some(&rtv), false,  Some(&dsv));
            swapchain.cmd_list.ClearRenderTargetView(rtv, &clear_color, None);
            swapchain.cmd_list.ClearDepthStencilView(dsv, Direct3D12::D3D12_CLEAR_FLAG_DEPTH, 1.0, 0, None);
            swapchain.cmd_list.RSSetViewports(&[swapchain.base_viewport]);
            swapchain.cmd_list.RSSetScissorRects(&[swapchain.base_scissor]);
        }

        return Some(swapchain);
    }

    /// Resizes the swapchain resources for the given window.
    ///
    /// Note: this locks the swapchain, so this call will block while the swapchain
    /// is already locked, such as by a call to [Dx::start_frame].
    pub fn resize_swapchain(&self, hwnd: Foundation::HWND) {
        self.swapchain.lock().unwrap().resize(hwnd);
    }

    /// Creates a new pipeline state.
    ///
    /// `desc` must be a valid pipeline state description; this function will
    /// set the `pRootSignature` value before creating the pipeline state.
    ///
    /// `name` is used to set the object name, used during debugging.
    pub fn create_pipeline_state(
        &self,
        desc: &mut Direct3D12::D3D12_GRAPHICS_PIPELINE_STATE_DESC,
        name: &str
    ) -> Result<Direct3D12::ID3D12PipelineState,()> {
        unsafe {
            // convince rust to copy the rootsig pointer without incrementing the 
            // counter or releasing it later
            let swapchain = self.swapchain.lock().unwrap();
            desc.pRootSignature = std::mem::transmute_copy(&swapchain.rootsig);
        }

        if let Ok(pso) = unsafe { self.device.CreateGraphicsPipelineState::<Direct3D12::ID3D12PipelineState>(desc) } {
            object_set_name(&pso, name);
            return Ok(pso);
        } else {
            return Err(());
        }
    }

    /// Returns the next available location for a descriptor within the SRV heap.
    fn get_new_srv_descriptor_loc(&self) -> u64 {
        let mut addr = self.srv_descriptorheap_addresses.lock().unwrap();
        if let Some(loc) = addr.reuse.pop_front() {
            return loc;
        } else {
            let loc = addr.next;
            addr.next += self.srv_descriptorsize as u64;
            return loc;
        }
    }

    /// Creates a new 2-dimensional [Texture] array.
    ///
    /// `levels` is the number of mip-map levels and should be at least 1.
    /// `size` is the number of layers.
    pub fn new_texture_2d_array(
        self: &Arc<Self>,
        format: Dxgi::Common::DXGI_FORMAT,
        width: u32,
        height: u32,
        size: u16,
        levels: u16
    ) -> Texture {
        let mut heapprops = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        heapprops.Type                 = Direct3D12::D3D12_HEAP_TYPE_DEFAULT;
        heapprops.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapprops.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let heapflag = Direct3D12::D3D12_HEAP_FLAG_NONE;

        let mut resdesc = Direct3D12::D3D12_RESOURCE_DESC::default();
        resdesc.Dimension        = Direct3D12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resdesc.Alignment        = 0;
        resdesc.Width            = width as u64;
        resdesc.Height           = height;
        resdesc.DepthOrArraySize = size;
        resdesc.MipLevels        = levels;
        resdesc.Format           = format;
        resdesc.SampleDesc.Count = 1;
        resdesc.Layout           = Direct3D12::D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resdesc.Flags            = Direct3D12::D3D12_RESOURCE_FLAG_NONE;

        let mut texptr: Option<Direct3D12::ID3D12Resource> = None;

        let r = unsafe { self.device.CreateCommittedResource(
            &heapprops,
            heapflag,
            &resdesc,
            Direct3D12::D3D12_RESOURCE_STATE_COMMON,
            None,
            &mut texptr
        ) };

        if r.is_err() {
            panic!("Couldn't create texture!");
        }

        let tex = texptr.unwrap();

        let srvheap_loc = self.get_new_srv_descriptor_loc();
        
        let mut gpu_desc_handle = unsafe { self.srv_descriptorheap.GetGPUDescriptorHandleForHeapStart() };
        gpu_desc_handle.ptr += srvheap_loc;

        let mut tex_srvhandle = unsafe { self.srv_descriptorheap.GetCPUDescriptorHandleForHeapStart() };
        tex_srvhandle.ptr += srvheap_loc as usize;

        unsafe { self.device.CreateShaderResourceView(&tex, None, tex_srvhandle) };

        Texture {
            /*
            width: width,
            height: height,
            depth: size,
            */

            srvheap_loc: srvheap_loc,

            texture: tex,
            gpu_descriptor_handle: gpu_desc_handle,

            dx: self.clone(),
        }
    }

    /// Creates a new 2-dimensional [Texture].
    ///
    /// `levels` is the number of mip-map levels and should be at least 1.
    pub fn new_texture_2d(
        self: &Arc<Self>,
        format: Dxgi::Common::DXGI_FORMAT,
        width: u32,
        height: u32,
        levels: u16
    ) -> Texture {
        let mut heapprops = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        heapprops.Type                 = Direct3D12::D3D12_HEAP_TYPE_DEFAULT;
        heapprops.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapprops.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let heapflag = Direct3D12::D3D12_HEAP_FLAG_NONE;

        let mut resdesc = Direct3D12::D3D12_RESOURCE_DESC::default();
        resdesc.Dimension        = Direct3D12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resdesc.Alignment        = 0;
        resdesc.Width            = width as u64;
        resdesc.Height           = height;
        resdesc.DepthOrArraySize = 1;
        resdesc.MipLevels        = levels;
        resdesc.Format           = format;
        resdesc.SampleDesc.Count = 1;
        resdesc.Layout           = Direct3D12::D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resdesc.Flags            = Direct3D12::D3D12_RESOURCE_FLAG_NONE;

        let mut texptr: Option<Direct3D12::ID3D12Resource> = None;

        let r = unsafe { self.device.CreateCommittedResource(
            &heapprops,
            heapflag,
            &resdesc,
            Direct3D12::D3D12_RESOURCE_STATE_COMMON,
            None,
            &mut texptr
        ) };

        if r.is_err() {
            panic!("Couldn't create texture!");
        }

        let tex = texptr.unwrap();

        let srvheap_loc = self.get_new_srv_descriptor_loc();
        
        let mut gpu_desc_handle = unsafe { self.srv_descriptorheap.GetGPUDescriptorHandleForHeapStart() };
        gpu_desc_handle.ptr += srvheap_loc;

        let mut tex_srvhandle = unsafe { self.srv_descriptorheap.GetCPUDescriptorHandleForHeapStart() };
        tex_srvhandle.ptr += srvheap_loc as usize;

        unsafe { self.device.CreateShaderResourceView(&tex, None, tex_srvhandle) };

        Texture {
            /*
            width: width,
            height: height,
            depth: size,
            */

            srvheap_loc: srvheap_loc,

            texture: tex,
            gpu_descriptor_handle: gpu_desc_handle,

            dx: self.clone(),
        }
    }

    pub fn new_vertex_buffer(&self, size: u64) -> Direct3D12::ID3D12Resource {
        let mut props = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        props.Type                 = Direct3D12::D3D12_HEAP_TYPE_DEFAULT;
        props.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let mut desc = Direct3D12::D3D12_RESOURCE_DESC::default();
        desc.Dimension         = Direct3D12::D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment         = Direct3D12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64;
        desc.Width             = size;
        desc.Height            = 1;
        desc.DepthOrArraySize  = 1;
        desc.MipLevels         = 1;
        desc.Format            = Dxgi::Common::DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count  = 1;
        desc.Layout            = Direct3D12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags             = Direct3D12::D3D12_RESOURCE_FLAG_NONE;

        let mut buffer: Option<Direct3D12::ID3D12Resource> = None;
        unsafe { self.device.CreateCommittedResource(
            &props,
            Direct3D12::D3D12_HEAP_FLAG_NONE,
            &desc,
            Direct3D12::D3D12_RESOURCE_STATE_COMMON,
            None,
            &mut buffer
        ).expect("Couldn't create vertex buffer."); }

        buffer.unwrap()
    }
    
    pub fn new_upload_buffer(&self, size: u64) -> Direct3D12::ID3D12Resource {
        let mut props = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        props.Type                 = Direct3D12::D3D12_HEAP_TYPE_UPLOAD;
        props.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let mut desc = Direct3D12::D3D12_RESOURCE_DESC::default();
        desc.Dimension         = Direct3D12::D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment         = Direct3D12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64;
        desc.Width             = size;
        desc.Height            = 1;
        desc.DepthOrArraySize  = 1;
        desc.MipLevels         = 1;
        desc.Format            = Dxgi::Common::DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count  = 1;
        desc.Layout            = Direct3D12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags             = Direct3D12::D3D12_RESOURCE_FLAG_NONE;

        let mut buffer: Option<Direct3D12::ID3D12Resource> = None;
        unsafe { self.device.CreateCommittedResource(
            &props,
            Direct3D12::D3D12_HEAP_FLAG_NONE,
            &desc,
            Direct3D12::D3D12_RESOURCE_STATE_GENERIC_READ,
            None,
            &mut buffer
        ).expect("Couldn't create vertex buffer."); }

        buffer.unwrap()
    }
    /// Locks and returns the copy command queue, which can be used to perform
    /// memory transfers between the CPU and the GPU.
    pub fn copy_queue(&self) -> MutexGuard<'_, CopyQueue> {
        self.copy_queue.lock().unwrap()
    }

    pub fn get_video_mem_used(&self) -> u64 {
        let mut info = Dxgi::DXGI_QUERY_VIDEO_MEMORY_INFO::default();

        if unsafe {
            self.adapter.QueryVideoMemoryInfo(0, Dxgi::DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mut info)
        }.is_err() {
            error!("Couldn't query video memory info.");
        }

        return info.CurrentUsage;
    }
}

/// The copy command queue resources.
pub struct CopyQueue {
    pub cmd_queue: Direct3D12::ID3D12CommandQueue,
    pub cmd_alloc: Direct3D12::ID3D12CommandAllocator,
    pub cmd_list : Direct3D12::ID3D12GraphicsCommandList,

    fence: Direct3D12::ID3D12Fence,
    fence_value: u64,
}

impl CopyQueue {
    /// Waits for all commands within the copy command queue to complete.
    pub fn flush_commands(&mut self) {
        let curval = self.fence_value;
        
        if unsafe { self.cmd_queue.Signal(&self.fence, curval) }.is_err() {
            panic!("Couldn't signal copy command queue.");
        }

        self.fence_value += 1;

        if unsafe { self.fence.SetEventOnCompletion(curval, Foundation::HANDLE(std::ptr::null_mut())) }.is_err() {
            panic!("SetEventOnCompletion failed.");
        }
    }

    /// Resets the copy command queue to an initial state, ready for new commands.
    pub fn reset(&mut self) {
        unsafe {
            self.cmd_alloc.Reset().expect("Couldn't reset copy command allocator.");
            self.cmd_list.Reset(&self.cmd_alloc, None).expect("Couldn't reset copy command list.");
        }
    }

    pub fn copy_resource(&mut self, from: &Direct3D12::ID3D12Resource, to: &Direct3D12::ID3D12Resource) {
        self.reset();

        unsafe { self.cmd_list.CopyResource(to, from) };

        unsafe { self.cmd_list.Close() }.expect("Couldn't close copy command list.");

        unsafe { self.cmd_queue.ExecuteCommandLists(&[Some(self.cmd_list.clone().into())]); }
        self.flush_commands();
    }
}

/// The swapchain resource lock. Typically this represents a frame.
pub type SwapChainLock<'a> = MutexGuard<'a, SwapChain>;

/// Swapchain related resources.
pub struct SwapChain {
    device: Direct3D12::ID3D12Device,

    cmd_queue: Direct3D12::ID3D12CommandQueue,
    cmd_allocs: Vec<Direct3D12::ID3D12CommandAllocator>,
    pub cmd_list  : Direct3D12::ID3D12GraphicsCommandList,

    swapchain: Dxgi::IDXGISwapChain4,

    swapchain_frame_handle_ptr: usize,

    // DirectComposition
    comp_dev_ptr: usize,
    comp_visual_ptr: usize,
    comp_target_ptr: usize,

    frameind: u32,

    fence: Direct3D12::ID3D12Fence,
    fence_values: [u64; DX_FRAMES as usize],

    rtv_width: u32,
    rtv_height: u32,

    rtv_descriptorheap: Direct3D12::ID3D12DescriptorHeap,
    rtv_descriptorsize: u32,

    backbuffers: Vec<Direct3D12::ID3D12Resource>,

    ds_descriptorheap : Direct3D12::ID3D12DescriptorHeap,
    ds_buffer         : Option<Direct3D12::ID3D12Resource>,

    base_scissor: Foundation::RECT,
    base_viewport: Direct3D12::D3D12_VIEWPORT,

    rootsig: Direct3D12::ID3D12RootSignature,

    ortho_proj: lamath::Mat4F,

    scissors: VecDeque<Foundation::RECT>,
    viewports: VecDeque<Direct3D12::D3D12_VIEWPORT>,
}

impl SwapChain {
    pub fn render_target_width(&self) -> u32 {
        self.rtv_width
    }

    pub fn render_target_height(&self) -> u32 {
        self.rtv_height
    }

    /// Updates the render target views and corresponding backbuffer resources.
    fn update_rtvs(&mut self) {
        unsafe {
            let mut rtvhandle = self.rtv_descriptorheap.GetCPUDescriptorHandleForHeapStart();

            for i in 0..DX_FRAMES {
                let backbuffer: Direct3D12::ID3D12Resource;
                backbuffer = self.swapchain.GetBuffer(i).expect("Failed to create back buffer resource.");
                object_set_name(&backbuffer, format!("EG-Overlay D3D12 Backbuffer RTV Resource {}", i).as_str());

                self.device.CreateRenderTargetView(&backbuffer, None, rtvhandle);
                self.backbuffers.push(backbuffer);
                rtvhandle.ptr += self.rtv_descriptorsize as usize;
            }
        }
    }

    /// (Re)Creates the depth/stencil buffer.
    fn create_dsbuffer(&mut self) {
        let mut dsvprops = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        dsvprops.Type                 = Direct3D12::D3D12_HEAP_TYPE_DEFAULT;
        dsvprops.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        dsvprops.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let mut desc = Direct3D12::D3D12_RESOURCE_DESC::default();
        desc.Dimension        = Direct3D12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment        = 0;
        desc.Width            = self.rtv_width as u64;
        desc.Height           = self.rtv_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels        = 1;
        desc.Format           = Dxgi::Common::DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Layout           = Direct3D12::D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags            =
            Direct3D12::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
            Direct3D12::D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

        let mut clear = Direct3D12::D3D12_CLEAR_VALUE::default();
        clear.Format               = Dxgi::Common::DXGI_FORMAT_D32_FLOAT;
        clear.Anonymous.DepthStencil.Depth   = 1.0;
        clear.Anonymous.DepthStencil.Stencil = 0;

        self.ds_buffer = None;

        unsafe {
            self.device.CreateCommittedResource(
                &dsvprops,
                Direct3D12::D3D12_HEAP_FLAG_NONE,
                &desc,
                Direct3D12::D3D12_RESOURCE_STATE_DEPTH_WRITE,
                Some(&clear),
                &mut self.ds_buffer
            ).expect("Failed to create Depth/Stencil buffer.");
        }

        object_set_name(&self.ds_buffer.as_ref().unwrap(), "EG-Overlay D3D12 Depth/Stencil Buffer");

        unsafe {
            let dsvhandle = self.ds_descriptorheap.GetCPUDescriptorHandleForHeapStart();
            self.device.CreateDepthStencilView(self.ds_buffer.as_ref().unwrap(), None, dsvhandle);
        }
    }
    
    /// Returns [true] if a backbuffer is available for rendering, [false] otherwise.
    fn backbuffer_ready(&self) -> bool {
        use windows::Win32::System::Threading::WaitForSingleObjectEx;

        let hndl = Foundation::HANDLE(self.swapchain_frame_handle_ptr as *mut std::ffi::c_void);
        
        if unsafe { WaitForSingleObjectEx(hndl, 0, false)==Foundation::WAIT_TIMEOUT } {
            return false;
        }

        return true;
    }

    /// End the current frame, executing all commands in the current command list.
    pub fn end_frame(&mut self) {
        if !self.scissors.is_empty() {
            panic!("push_scissor/pop_scissor mismatch!");
        }

        if !self.viewports.is_empty() {
            panic!("push_viewport/pop_viewport mismatch!");
        }
        
        let backbuffer = &self.backbuffers[self.frameind as usize];

        let mut barrier = Direct3D12::D3D12_RESOURCE_BARRIER::default();
        barrier.Type = Direct3D12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = Direct3D12::D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Anonymous.Transition = std::mem::ManuallyDrop::new(Direct3D12::D3D12_RESOURCE_TRANSITION_BARRIER {
            pResource: unsafe {std::mem::transmute_copy(backbuffer) },
            Subresource: Direct3D12::D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            StateBefore: Direct3D12::D3D12_RESOURCE_STATE_RENDER_TARGET,
            StateAfter: Direct3D12::D3D12_RESOURCE_STATE_PRESENT,
        });

        let cmd_queue = &self.cmd_queue;
        let cmd_list = &self.cmd_list;
        let swapchain = &self.swapchain;
        let fence = &self.fence;

        unsafe {
            cmd_list.ResourceBarrier(&[barrier]);

            cmd_list.Close().expect("Failed to close command list.");

            cmd_queue.ExecuteCommandLists(&[Some(cmd_list.clone().into())]);

            swapchain.Present(0, Dxgi::DXGI_PRESENT_ALLOW_TEARING).unwrap();

            let fenceval = self.fence_values[self.frameind as usize];
        
            cmd_queue.Signal(fence, fenceval).unwrap();
            self.frameind = self.swapchain.GetCurrentBackBufferIndex();

            if fence.GetCompletedValue() < self.fence_values[self.frameind as usize] {
                fence.SetEventOnCompletion(fenceval, Foundation::HANDLE(std::ptr::null_mut()))
                    .expect("SetEventOnCompletion failed.");
            }

            self.fence_values[self.frameind as usize] = fenceval + 1;
        }
    }

    /// Waits for all commands in the command queue to finish.
    pub fn flush_commands(&self) {
        let cur_val: u64 = self.fence_values[self.frameind as usize];

        unsafe {
            self.cmd_queue.Signal(&self.fence, cur_val).expect("Couldn't signal command queue.");
            self.fence.SetEventOnCompletion(cur_val, Foundation::HANDLE(std::ptr::null_mut()))
                .expect("SetEventOnCompletion failed.");
        }
    }

    /// Resizes the swapchain resources for the given window.
    fn resize(&mut self, hwnd: Foundation::HWND) {
        let mut rect = Foundation::RECT::default();

        unsafe {
            WindowsAndMessaging::GetClientRect(hwnd, &mut rect).unwrap();
        }

        let mut new_width : u32 = rect.right.try_into().unwrap();
        let mut new_height: u32 = rect.bottom.try_into().unwrap();

        if new_width < 1 { new_width = 1; }
        if new_height < 1 { new_height = 1; }

        if self.rtv_width == new_width && self.rtv_height == new_height { return; }

        self.ortho_proj = lamath::Mat4F::ortho(0.0, new_width as f32, 0.0, new_height as f32, 0.0, 1.0);

        self.flush_commands();

        self.rtv_width = new_width;
        self.rtv_height = new_height;

        self.base_scissor.left   = 0;
        self.base_scissor.right  = new_width as i32;
        self.base_scissor.top    = 0;
        self.base_scissor.bottom = new_height as i32;

        self.base_viewport.TopLeftX = 0.0;
        self.base_viewport.TopLeftY = 0.0;
        self.base_viewport.Width    = new_width as f32;
        self.base_viewport.Height   = new_height as f32;
        self.base_viewport.MinDepth = 0.0;
        self.base_viewport.MaxDepth = 1.0;

        
        self.backbuffers.clear();

        unsafe {
            let desc = self.swapchain.GetDesc().expect("Couldn't get swapchain description (during resize).");
            self.swapchain.ResizeBuffers(
                DX_FRAMES,
                self.rtv_width,
                self.rtv_height,
                desc.BufferDesc.Format,
                Dxgi::DXGI_SWAP_CHAIN_FLAG(desc.Flags as i32)
            ).expect("Couldn't resize swapchain buffers.");

            self.frameind = self.swapchain.GetCurrentBackBufferIndex();

            self.update_rtvs();
            self.create_dsbuffer();
        }
    }

    /// Sets the current pipeline state.
    ///
    /// All rendering commands added after this call will use the given state.
    pub fn set_pipeline_state(&self, pso: &Direct3D12::ID3D12PipelineState) {
        unsafe { self.cmd_list.SetPipelineState(pso); }
    }

    /// Sets a float value within the shader root signature.
    ///
    /// `value` is the 32-bit float value to set, `index` is the index of the
    /// root signature which is always `0` currently, and `offset` is offset
    /// into the constant values, measured in 32-bit blocks. I.e. an offset of
    /// `4` means an offset of 4 32-bit values (16 bytes total).
    pub fn set_root_constant_float(&self, value: f32, index: u32, offset: u32) {
        unsafe {
            self.cmd_list.SetGraphicsRoot32BitConstant(
                index,
                value.to_bits(),
                offset
            )
        }
    }

    /// Sets a color represented by 4 float values within the shader root signature.
    ///
    /// See [SwapChain::set_root_constant_float].
    pub fn set_root_constant_color(&self, value: crate::ui::Color, index: u32, offset: u32) {
        let float4: [f32; 4] = [
            value.r_f32(),
            value.g_f32(),
            value.b_f32(),
            value.a_f32()
        ];
        self.set_root_constant_float4(&float4, index, offset);
    }

    /// Sets a float4 value within the shader root signature.
    ///
    /// See [SwapChain::set_root_constant_float].
    pub fn set_root_constant_float4(&self, value: &[f32; 4], index: u32, offset: u32) {
        unsafe {
            self.cmd_list.SetGraphicsRoot32BitConstants(
                index,
                4,
                std::mem::transmute(value),
                offset
            )
        }
    }

    pub fn set_root_constant_vec3f(&self, value: &lamath::Vec3F, index: u32, offset: u32) {
        unsafe {
            self.cmd_list.SetGraphicsRoot32BitConstants(
                index,
                3,
                std::mem::transmute(value),
                offset
            )
        }
    }

    /// Sets a float4x4 within the shader root signature.
    ///
    /// See [SwapChain::set_root_constant_float].
    pub fn set_root_constant_mat4f(&self, value: &lamath::Mat4F, index: u32, offset: u32) {
        unsafe {
            self.cmd_list.SetGraphicsRoot32BitConstants(
                index,
                16,
                std::mem::transmute(value),
                offset
            )
        }
    }

    /// Sets a float4x4 value within the shader root signature to the value of
    /// the current ortho projection.
    pub fn set_root_constant_ortho_proj(&self, index: u32, offset: u32) {
        self.set_root_constant_mat4f(&self.ortho_proj, index, offset);
    }

    pub fn set_root_constant_bool(&self, value: bool, index: u32, offset: u32) {
        let intval: u32 = if value { 1 } else { 0 };
        unsafe {
            self.cmd_list.SetGraphicsRoot32BitConstants(
                index,
                1,
                std::mem::transmute(&intval),
                offset
            )
        }
    }

    /// Sets the current primitive topology.
    ///
    /// See [D3D_PRIMITIVE_TOPOLOGY](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_primitive_topology)
    pub fn set_primitive_topology(&self, topo: Direct3D::D3D_PRIMITIVE_TOPOLOGY) {
        unsafe {
            self.cmd_list.IASetPrimitiveTopology(topo)
        }
    }

    /// Draws non-indexed, instanced primitives.
    ///
    /// See [DrawInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-drawinstanced)
    pub fn draw_instanced(&self, vertexes: u32, instances: u32, first_vertex: u32, first_instance: u32) {
        unsafe {
            self.cmd_list.DrawInstanced(vertexes, instances, first_vertex, first_instance)
        }
    }

    /// Sets the static texture within the root signature.
    pub fn set_texture(&self, index: u32, texture: &Texture) {
        unsafe {
            self.cmd_list.SetGraphicsRootDescriptorTable(index + 1, texture.gpu_descriptor_handle);
        }
    }

    pub fn set_vertex_buffers(&mut self, slot: u32, views: Option<&[Direct3D12::D3D12_VERTEX_BUFFER_VIEW]>) {
        unsafe {
            self.cmd_list.IASetVertexBuffers(slot, views)
        }
    }

    pub fn push_scissor(&mut self, left: i64, top: i64, right: i64, bottom: i64) -> bool {
        let mut scleft   = left as i32;
        let mut sctop    = top as i32;
        let mut scright  = right as i32;
        let mut scbottom = bottom as i32;

        let r = self.scissors.front().unwrap_or(&self.base_scissor);

        if scleft   < r.left   { scleft = r.left;     }
        if sctop    < r.top    { sctop = r.top;       }
        if scright  > r.right  { scright = r.right;   }
        if scbottom > r.bottom { scbottom = r.bottom; }

        if scright - scleft <= 0 || scbottom - sctop <= 0 { return false; }

        let sc = Foundation::RECT {
            left   : scleft,
            right  : scright,
            top    : sctop,
            bottom : scbottom,
        };


        unsafe { self.cmd_list.RSSetScissorRects(&[sc]); }

        self.scissors.push_front(sc);

        true
    }

    pub fn pop_scissor(&mut self) {
        self.scissors.pop_front();

        let r = self.scissors.front().unwrap_or(&self.base_scissor);

        unsafe { self.cmd_list.RSSetScissorRects(&[*r]); }
    }

    pub fn push_viewport(&mut self, left: f32, top: f32, width: f32, height: f32) {
        let vp = Direct3D12::D3D12_VIEWPORT {
            TopLeftX: left,
            TopLeftY: top,
            Width: width,
            Height: height,
            MinDepth: 0.0,
            MaxDepth: 1.0,
        };

        unsafe { self.cmd_list.RSSetViewports(&[vp]); }

        self.viewports.push_front(vp);
    }

    pub fn pop_viewport(&mut self) {
        self.viewports.pop_front();

        let r = self.viewports.front().unwrap_or(&self.base_viewport);

        unsafe { self.cmd_list.RSSetViewports(&[*r]); }
    }
}

impl Drop for SwapChain {
    fn drop(&mut self) {
        // Drop our manually held DirectComposition objects
        unsafe {
            drop(DirectComposition::IDCompositionDevice::from_raw(self.comp_dev_ptr    as *mut std::ffi::c_void));
            drop(DirectComposition::IDCompositionVisual::from_raw(self.comp_visual_ptr as *mut std::ffi::c_void));
            drop(DirectComposition::IDCompositionTarget::from_raw(self.comp_target_ptr as *mut std::ffi::c_void));
        }
    }
}

/// Enables the D3D12 Debugging Layer.
fn enable_debug_layer() {
    unsafe {
        let mut debug_ptr: Option<Direct3D12::ID3D12Debug> = None;
        if Direct3D12::D3D12GetDebugInterface(&mut debug_ptr).is_err() {
            panic!("Couldn't get ID3D12Debug interface.");
        }
        
        let debug = debug_ptr.unwrap();

        debug.EnableDebugLayer();
    }
    
    warn!("D3D12 debug validation layer enabled. This WILL negatively impact performance.");
}

fn find_adapter() -> Dxgi::IDXGIAdapter4 {
    let factory: Dxgi::IDXGIFactory6;

    let mut flags: Dxgi::DXGI_CREATE_FACTORY_FLAGS = Dxgi::DXGI_CREATE_FACTORY_FLAGS(0);

    if cfg!(debug_assertions) {
        flags = Dxgi::DXGI_CREATE_FACTORY_DEBUG;
    }

    let desc: Dxgi::DXGI_ADAPTER_DESC1;
    let mut meminfo = Dxgi::DXGI_QUERY_VIDEO_MEMORY_INFO::default();
    let mut driver_ver = DriverVer::default();

    let adapter: Dxgi::IDXGIAdapter4;

    unsafe {
        factory = Dxgi::CreateDXGIFactory2(flags).expect("Couldn't get DXGI Factory");

        // Get the first 'high performance' GPU. This should be a discrete
        // GPU with dedicated video memory. This should be the correct GPU
        // in pretty much every case.
        adapter = factory.EnumAdapterByGpuPreference::<Dxgi::IDXGIAdapter4>(
            0,
            Dxgi::DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
        ).expect("Couldn't get GPU adapter.");

        desc = adapter.GetDesc1().expect("Couldn't get adapter description.");

        // load the driver info from the registry to display the driver version
        // for debug/troubleshooting purposes
        let mut hklmdx = Registry::HKEY(std::ptr::null_mut());
        if !Registry::RegOpenKeyExA(
            Registry::HKEY_LOCAL_MACHINE,
            windows::core::s!("SOFTWARE\\Microsoft\\DirectX"),
            None, Registry::KEY_READ | Registry::KEY_ENUMERATE_SUB_KEYS | Registry::KEY_QUERY_VALUE,
            &mut hklmdx
        ).is_ok() {
            panic!("Couldn't open HKLM\\Software\\Microsoft\\DirectX");
        }
        
        let mut subkeystrlen = 0;

        if !Registry::RegQueryInfoKeyA(
            hklmdx,
            None,
            None,
            None,
            None,
            Some(&mut subkeystrlen),
            None,
            None,
            None,
            None,
            None,
            None
        ).is_ok() {
            panic!("Couldn't get DX max subkey len.");
        }

        let mut subkeynmbytes: Vec<u8> = vec![0; (subkeystrlen+1).try_into().unwrap()];
        let mut i = 0;
        while Registry::RegEnumKeyA(hklmdx, i, Some(&mut subkeynmbytes)).is_ok() {
            let subkeynm = windows::core::PCSTR(subkeynmbytes.as_ptr());
            let mut luid = windows::Win32::Foundation::LUID {LowPart: 0, HighPart: 0};
            let mut luid_size: u32 = std::mem::size_of::<windows::Win32::Foundation::LUID>().try_into().unwrap();

            if Registry::RegGetValueA(
                hklmdx,
                subkeynm,
                windows::core::s!("AdapterLuid"),
                Registry::RRF_RT_QWORD,
                None,
                Some(&mut luid as *mut _ as *mut std::ffi::c_void),
                Some(&mut luid_size)
            ).is_ok() && luid.LowPart==desc.AdapterLuid.LowPart && luid.HighPart==desc.AdapterLuid.HighPart {
                let mut versize: u32 = std::mem::size_of::<DriverVer>().try_into().unwrap();

                if !Registry::RegGetValueA(
                    hklmdx,
                    subkeynm,
                    windows::core::s!("DriverVersion"),
                    Registry::RRF_RT_QWORD,
                    None,
                    Some(&mut driver_ver as *mut _ as *mut std::ffi::c_void),
                    Some(&mut versize)
                ).is_ok() {
                    panic!("Couldn't get driver version.");
                }
                break;
            }

            i += 1;
        }
        if !Registry::RegCloseKey(hklmdx).is_ok() { panic!("Couldn't close registry key."); };

        adapter.QueryVideoMemoryInfo(
            0,
            Dxgi::DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
            &mut meminfo as &mut _
        ).expect("Couldn't get adapter memory info.");
    }

    // convert from a wide-string
    let descstr = String::from_utf16(&desc.Description).unwrap();
    // and trim trailing nulls
    let descstr = descstr.trim_matches(char::from(0));

    info!("GPU              : {:04X}:{:04X} rev. {:X}", desc.VendorId, desc.DeviceId, desc.Revision);
    info!("                   {}", descstr);
    info!("Driver Version   : {}.{}.{}.{}", driver_ver.prod, driver_ver.ver, driver_ver.sub, driver_ver.build);
    info!("Memory Budget    : {:.2} MiB", (meminfo.Budget as f64) / 1024.0 / 1024.0);

    return adapter;
}

fn create_device(adapter: &Dxgi::IDXGIAdapter4) -> Direct3D12::ID3D12Device9 {
    let mut device_ptr: Option<Direct3D12::ID3D12Device9> = None;

    let r = unsafe { Direct3D12::D3D12CreateDevice::<&Dxgi::IDXGIAdapter4, Direct3D12::ID3D12Device9>(
        adapter,
        Direct3D::D3D_FEATURE_LEVEL_11_0,
        &mut device_ptr as *mut _
    ) };

    if r.is_err() {
        panic!("Couldn't create D3D12 device.");
    }

    let device = device_ptr.unwrap();

    object_set_name(&device, "EG-Overlay D3D12 Device");

    let reqlevels = vec![
        Direct3D::D3D_FEATURE_LEVEL_11_0,
        Direct3D::D3D_FEATURE_LEVEL_11_1,
        Direct3D::D3D_FEATURE_LEVEL_12_0,
        Direct3D::D3D_FEATURE_LEVEL_12_1,
        Direct3D::D3D_FEATURE_LEVEL_12_2,
    ];

    let mut levels = Direct3D12::D3D12_FEATURE_DATA_FEATURE_LEVELS::default();
    levels.NumFeatureLevels = reqlevels.len() as u32;
    levels.pFeatureLevelsRequested = reqlevels.as_ptr();

    let levels_size = std::mem::size_of::<Direct3D12::D3D12_FEATURE_DATA_FEATURE_LEVELS>() as u32;

    if unsafe { device.CheckFeatureSupport(
        Direct3D12::D3D12_FEATURE_FEATURE_LEVELS,
        &mut levels as *mut _ as *mut std::ffi::c_void,
        levels_size
    ) }.is_err() {
        panic!("CheckFeatureSupport failed.");
    }

    let featlevel: &str;

    match levels.MaxSupportedFeatureLevel {
    Direct3D::D3D_FEATURE_LEVEL_11_0 => featlevel = "11_0",
    Direct3D::D3D_FEATURE_LEVEL_11_1 => featlevel = "11_1",
    Direct3D::D3D_FEATURE_LEVEL_12_0 => featlevel = "12_0",
    Direct3D::D3D_FEATURE_LEVEL_12_1 => featlevel = "12_1",
    Direct3D::D3D_FEATURE_LEVEL_12_2 => featlevel = "12_2",
    _                                => featlevel = "Unknown",
    }

    info!("Max Feature Level: {}, 11_0 requested", featlevel);

    let hlslmodel: &str;

    let mut shadermodel = Direct3D12::D3D12_FEATURE_DATA_SHADER_MODEL {
        HighestShaderModel: Direct3D12::D3D_HIGHEST_SHADER_MODEL,
    };
    let shadermodelsize = std::mem::size_of::<Direct3D12::D3D12_FEATURE_DATA_SHADER_MODEL>() as u32;

    unsafe {
        if device.CheckFeatureSupport(
            Direct3D12::D3D12_FEATURE_SHADER_MODEL,
            &mut shadermodel as *mut _ as *mut std::ffi::c_void,
            shadermodelsize
        ).is_err() {
            panic!("CheckFeatureSupport failed.");
        }
    }

    match shadermodel.HighestShaderModel {
    Direct3D12::D3D_SHADER_MODEL_5_1 => hlslmodel = "5.1",
    Direct3D12::D3D_SHADER_MODEL_6_0 => hlslmodel = "6.0",
    Direct3D12::D3D_SHADER_MODEL_6_1 => hlslmodel = "6.1",
    Direct3D12::D3D_SHADER_MODEL_6_2 => hlslmodel = "6.2",
    Direct3D12::D3D_SHADER_MODEL_6_3 => hlslmodel = "6.3",
    Direct3D12::D3D_SHADER_MODEL_6_4 => hlslmodel = "6.4",
    Direct3D12::D3D_SHADER_MODEL_6_5 => hlslmodel = "6.5",
    Direct3D12::D3D_SHADER_MODEL_6_6 => hlslmodel = "6.6",
    Direct3D12::D3D_SHADER_MODEL_6_7 => hlslmodel = "6.7",
    _                                => hlslmodel = "Unknown",
    }

    if shadermodel.HighestShaderModel.0 < Direct3D12::D3D_SHADER_MODEL_6_1.0 {
        panic!("HLSL Shader Model 6.1+ required.");
    }

    info!("HLSL Shader Model: {}, 6.1 required", hlslmodel);

    return device;
}

fn create_swapchain(device: &Direct3D12::ID3D12Device, hwnd: Foundation::HWND) -> SwapChain {
    let factory: Dxgi::IDXGIFactory6;

    let mut flags: Dxgi::DXGI_CREATE_FACTORY_FLAGS = Dxgi::DXGI_CREATE_FACTORY_FLAGS(0);

    if cfg!(debug_assertions) {
        flags = Dxgi::DXGI_CREATE_FACTORY_DEBUG;
    }

    let mut rect = Foundation::RECT::default();

    unsafe {
        factory = Dxgi::CreateDXGIFactory2(flags).expect("Couldn't get DXGI Factory");

        WindowsAndMessaging::GetClientRect(hwnd, &mut rect).unwrap();
    }

    let ortho = lamath::Mat4F::ortho(0.0, rect.right as f32, 0.0, rect.bottom as f32, 0.0, 1.0);

    let rtv_width  = rect.right as u32;
    let rtv_height = rect.bottom as u32;

    let mut base_scissor = Foundation::RECT::default();
    let mut base_viewport = Direct3D12::D3D12_VIEWPORT::default();

    base_scissor.left   = 0;
    base_scissor.right  = rect.right;
    base_scissor.top    = 0;
    base_scissor.bottom = rect.bottom;

    base_viewport.TopLeftX = 0.0;
    base_viewport.TopLeftY = 0.0;
    base_viewport.Width    = rect.right as f32;
    base_viewport.Height   = rect.bottom as f32;
    base_viewport.MinDepth = 0.0;
    base_viewport.MaxDepth = 1.0;

    let flags =
        Dxgi::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
        Dxgi::DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    let mut desc = Dxgi::DXGI_SWAP_CHAIN_DESC1::default();
    desc.Width            = rect.right as u32;
    desc.Height           = rect.bottom as u32;
    desc.Format           = Dxgi::Common::DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = Dxgi::DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = DX_FRAMES;
    desc.Scaling          = Dxgi::DXGI_SCALING_STRETCH;
    desc.SwapEffect       = Dxgi::DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode        = Dxgi::Common::DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags            = flags.0 as u32;

    let cmd_queue = create_command_queue(device, Direct3D12::D3D12_COMMAND_LIST_TYPE_DIRECT);
    object_set_name(&cmd_queue, "EG-Overlay D3D12 Swap Chain Command Queue");

    let comp_dev_ptr: usize;
    let comp_target_ptr: usize;
    let comp_visual_ptr: usize;
    let swapchain: Dxgi::IDXGISwapChain4;
    let swapchain_frame_handle_ptr: usize;
    let frameind: u32;

    let rtv_descriptorheap: Direct3D12::ID3D12DescriptorHeap;
    let rtv_descriptorsize: u32;

    let ds_descriptorheap: Direct3D12::ID3D12DescriptorHeap;

    let fence: Direct3D12::ID3D12Fence;

    let mut cmd_allocs: Vec<Direct3D12::ID3D12CommandAllocator> = Vec::with_capacity(DX_FRAMES as usize);
    let cmd_list: Direct3D12::ID3D12GraphicsCommandList;

    for i in 0..DX_FRAMES {
        let alloc = create_command_allocator(device, Direct3D12::D3D12_COMMAND_LIST_TYPE_DIRECT);
        object_set_name(&alloc, format!("EG-Overlay D3D12 Swap Chain Command Allocator {}", i).as_str());
        cmd_allocs.push(alloc);
    }

    cmd_list = create_command_list(device, &cmd_allocs[0], Direct3D12::D3D12_COMMAND_LIST_TYPE_DIRECT);
    object_set_name(&cmd_list, "EG-Overlay D3D12 Swap Chain Command List");

    debug!("Setting up DirectComposition...");
    unsafe {
        fence = device.CreateFence::<Direct3D12::ID3D12Fence>(
            0,
            Direct3D12::D3D12_FENCE_FLAG_NONE
        ).expect("Failed to create command queue fence.");
        object_set_name(&fence, "EG-Overlay D3D12 Swap Chain Fence");

        let sc1 = factory.CreateSwapChainForComposition(
            &cmd_queue,
            &desc,
            None
        ).expect("Couldn't create swap chain.");

        let comp_dev: DirectComposition::IDCompositionDevice;
        let comp_target: DirectComposition::IDCompositionTarget;
        let comp_visual: DirectComposition::IDCompositionVisual;

        comp_dev = DirectComposition::DCompositionCreateDevice(None)
            .expect("Couldn't create DirectComposition device.");

        comp_target = comp_dev.CreateTargetForHwnd(hwnd, true)
            .expect("Couldn't create DirectComposition target.");

        comp_visual = comp_dev.CreateVisual()
            .expect("Couldn't create DirectComposition visual.");

        comp_visual.SetContent(&sc1)
            .expect("Couldn't set DirectComposition visual content.");
        comp_target.SetRoot(&comp_visual)
            .expect("Couldn't set DirectComposition target root.");

        comp_dev.Commit().expect("Couldn't commit DirectComposition device.");

        // This is UGLY, but the DirectComposition structs don't implement send...
        comp_dev_ptr    = comp_dev.into_raw()    as usize;
        comp_target_ptr = comp_target.into_raw() as usize;
        comp_visual_ptr = comp_visual.into_raw() as usize;

        swapchain = sc1.cast::<Dxgi::IDXGISwapChain4>().expect("Couldn't get IDXGISwapChain4.");

        let hndl = swapchain.GetFrameLatencyWaitableObject();
        swapchain_frame_handle_ptr = hndl.0 as usize;

        frameind = swapchain.GetCurrentBackBufferIndex();

        rtv_descriptorheap = create_descriptor_heap(
            device,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            DX_FRAMES,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        );
        object_set_name(&rtv_descriptorheap, "EG-Overlay D3D12 RTV Descriptor Heap");
        rtv_descriptorsize = device.GetDescriptorHandleIncrementSize(Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        ds_descriptorheap = create_descriptor_heap(
            device,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1,
            Direct3D12::D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        );
        object_set_name(&ds_descriptorheap, "EG-Overlay D3D12 Depth/Stencil Descriptor Heap");
    }

    let rootsig: Direct3D12::ID3D12RootSignature;

    info!("Loading root signature from shaders/root-sig.cso...");

    let rootcso = std::fs::read("shaders/root-sig.cso").expect("Couldn't read shaders/root-sig.cso");
    unsafe {
        rootsig = device.CreateRootSignature(0, rootcso.as_slice()).expect("Couldn't create root signature");
    }
    object_set_name(&rootsig, "EG-Overlay D3D12 Root Signature");

    let mut swapchain = SwapChain {
        device: device.clone(),

        cmd_queue: cmd_queue,
        cmd_allocs: cmd_allocs,
        cmd_list  : cmd_list,

        swapchain: swapchain,

        base_scissor: base_scissor,
        base_viewport: base_viewport,

        comp_dev_ptr: comp_dev_ptr,
        comp_target_ptr: comp_target_ptr,
        comp_visual_ptr: comp_visual_ptr,

        ortho_proj: ortho,

        swapchain_frame_handle_ptr: swapchain_frame_handle_ptr,

        frameind: frameind,

        fence: fence,
        fence_values: [0; DX_FRAMES as usize],

        rtv_width : rtv_width,
        rtv_height: rtv_height,

        rtv_descriptorheap: rtv_descriptorheap,
        rtv_descriptorsize: rtv_descriptorsize,

        backbuffers: Vec::with_capacity(DX_FRAMES as usize),

        ds_descriptorheap: ds_descriptorheap,
        ds_buffer: None,

        rootsig: rootsig,

        scissors: VecDeque::new(),
        viewports: VecDeque::new(),
    };

    swapchain.update_rtvs();
    swapchain.create_dsbuffer();

    return swapchain;
}

fn create_copyqueue(device: &Direct3D12::ID3D12Device) -> CopyQueue {
    let queue = create_command_queue(device    , Direct3D12::D3D12_COMMAND_LIST_TYPE_COPY);
    let alloc = create_command_allocator(device, Direct3D12::D3D12_COMMAND_LIST_TYPE_COPY);
    let list  = create_command_list(device, &alloc, Direct3D12::D3D12_COMMAND_LIST_TYPE_COPY);

    object_set_name(&queue, "EG-Overlay D3D12 Copy Command Queue");
    object_set_name(&alloc, "EG-Overlay D3D12 Copy Command Allocator");
    object_set_name(&list , "EG-Overlay D3D12 Copy Command List");

    let fence = unsafe { device.CreateFence(0, Direct3D12::D3D12_FENCE_FLAG_NONE)
        .expect("Couldn't create copy command queue fence.") };

    CopyQueue {
        cmd_queue: queue,
        cmd_alloc: alloc,
        cmd_list : list,
        fence    : fence,
        fence_value: 0,
    }
}

fn create_command_queue(
    device: &Direct3D12::ID3D12Device,
    queue_type: Direct3D12::D3D12_COMMAND_LIST_TYPE
) -> Direct3D12::ID3D12CommandQueue {
    let mut desc = Direct3D12::D3D12_COMMAND_QUEUE_DESC::default();

    desc.Type     = queue_type;
    desc.Priority = 0; //Direct3D12::D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags    = Direct3D12::D3D12_COMMAND_QUEUE_FLAG_NONE;

    unsafe {
        return device.CreateCommandQueue::<Direct3D12::ID3D12CommandQueue>(
                &desc as *const _
        ).expect("Couldn't create Command Queue.");
    }
}

fn create_descriptor_heap(
    device         : &Direct3D12::ID3D12Device,
    heap_type      : Direct3D12::D3D12_DESCRIPTOR_HEAP_TYPE,
    num_descriptors: u32,
    flags          : Direct3D12::D3D12_DESCRIPTOR_HEAP_FLAGS
) -> Direct3D12::ID3D12DescriptorHeap {
    let mut desc = Direct3D12::D3D12_DESCRIPTOR_HEAP_DESC::default();
    desc.NumDescriptors = num_descriptors;
    desc.Type           = heap_type;
    desc.Flags          = flags;

    unsafe {
        return device.CreateDescriptorHeap(&desc)
            .expect("Failed to create descriptor heap.");
    }
}

fn create_command_allocator(
    device: &Direct3D12::ID3D12Device,
    alloc_type: Direct3D12::D3D12_COMMAND_LIST_TYPE
) -> Direct3D12::ID3D12CommandAllocator {
    let alloc: Direct3D12::ID3D12CommandAllocator;
    unsafe {
        alloc = device.CreateCommandAllocator(alloc_type).expect("Couldn't create command allocator.");
    }

    return alloc;
}

fn create_command_list(
    device: &Direct3D12::ID3D12Device,
    alloc: &Direct3D12::ID3D12CommandAllocator,
    list_type: Direct3D12::D3D12_COMMAND_LIST_TYPE
) -> Direct3D12::ID3D12GraphicsCommandList {
    let list: Direct3D12::ID3D12GraphicsCommandList;

    unsafe {
        list = device.CreateCommandList(0, list_type, alloc, None).expect("Failed to create Graphics Command List.");
        list.Close().expect("Failed to close command list.");
    }

    return list;
}



#[repr(C)]
#[derive(Default)]
struct DriverVer {
    build: u16,
    sub  : u16,
    ver  : u16,
    prod : u16,
}

/// Sets the name for the given object, used during debugging.
pub fn object_set_name(object: &Direct3D12::ID3D12Object, name: &str) {
    let namecstr = std::ffi::CString::new(name).unwrap();
    let namelen: u32 = (namecstr.count_bytes()+1).try_into().unwrap();

    unsafe {
        object.SetPrivateData(
            &Direct3D::WKPDID_D3DDebugObjectName,
            namelen,
            Some(namecstr.as_ptr() as *const _ as *const std::ffi::c_void)
        ).expect("Couldn't set D3D12 Object Debug Name.");
    }
}

/// A D3D12 texture and related resources.
pub struct Texture {
    /*
    width: u32,
    height: u32,
    depth: u16,
    */

    srvheap_loc: u64,

    texture: Direct3D12::ID3D12Resource,
    gpu_descriptor_handle: Direct3D12::D3D12_GPU_DESCRIPTOR_HANDLE,

    dx: Arc<Dx>,
}

impl Texture {
    /// Sets this texture's name.
    pub fn set_name(&self, name: &str) {
        object_set_name(&self.texture, name);
    }

    /// Copies the pixel data from the given array slice to the GPU heap.
    ///
    /// `x`, `y`, `array_level` are the destination location within the texture
    // where `data` will be copied to.
    /// `w` and `h` are the size of the region represented by `data`.
    ///
    /// `data` must be `w` x `h` x bpp long where bpp is the number of bytes
    /// per pixel based on `format`.
    pub fn write_pixels(
        &self,
        x: u32,
        y: u32,
        array_level: u32,
        w: u32,
        h: u32,
        format: Dxgi::Common::DXGI_FORMAT,
        data: &[u8]
    ) {
        let bpp: u32;
        match format {
            Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM |
            Dxgi::Common::DXGI_FORMAT_R8G8B8A8_UNORM => bpp = 4,
            Dxgi::Common::DXGI_FORMAT_R8_UNORM => bpp = 1,
            _ => panic!("format not implemented."),
        }

        let rowwidth: u32 = w * bpp;
        let rowpitch: u32 = rowwidth + (rowwidth % 256);

        let mut uploadprops = Direct3D12::D3D12_HEAP_PROPERTIES::default();
        uploadprops.Type                 = Direct3D12::D3D12_HEAP_TYPE_UPLOAD;
        uploadprops.CPUPageProperty      = Direct3D12::D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadprops.MemoryPoolPreference = Direct3D12::D3D12_MEMORY_POOL_UNKNOWN;

        let mut uploaddesc = Direct3D12::D3D12_RESOURCE_DESC::default();
        uploaddesc.Dimension        = Direct3D12::D3D12_RESOURCE_DIMENSION_BUFFER;
        uploaddesc.Alignment        = Direct3D12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64;
        uploaddesc.Width            = (rowpitch * h) as u64;
        uploaddesc.Height           = 1;
        uploaddesc.DepthOrArraySize = 1;
        uploaddesc.MipLevels        = 1;
        uploaddesc.Format           = Dxgi::Common::DXGI_FORMAT_UNKNOWN;
        uploaddesc.Layout           = Direct3D12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploaddesc.SampleDesc.Count = 1;
        uploaddesc.Flags            = Direct3D12::D3D12_RESOURCE_FLAG_NONE;

        let mut upload_ptr: Option<Direct3D12::ID3D12Resource> = None;

        if unsafe { self.dx.device.CreateCommittedResource(
            &uploadprops,
            Direct3D12::D3D12_HEAP_FLAG_NONE,
            &uploaddesc,
            Direct3D12::D3D12_RESOURCE_STATE_GENERIC_READ,
            None,
            &mut upload_ptr
        ) }.is_err() {
            panic!("Couldn't create upload resource.");
        }

        let upload = upload_ptr.unwrap();

        let rr = Direct3D12::D3D12_RANGE::default();

        let mut uploaddata: *mut std::ffi::c_void = std::ptr::null_mut();

        if unsafe { upload.Map(0, Some(&rr), Some(&mut uploaddata)) }.is_err() {
            panic!("Couldn't map upload data.");
        }

        for yi in 0..h {
            unsafe {
                let line = uploaddata.add((yi * rowpitch) as usize);
                let data_line = data.as_ptr().add((yi * w * bpp) as usize) as *mut std::ffi::c_void;
                std::ptr::copy_nonoverlapping(data_line, line, (bpp * w) as usize)
            }
        }

        unsafe { upload.Unmap(0, None) }

        let mut srcloc = Direct3D12::D3D12_TEXTURE_COPY_LOCATION::default();
        srcloc.pResource = unsafe { std::mem::transmute_copy(&upload) };
        srcloc.Type      = Direct3D12::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

        srcloc.Anonymous.PlacedFootprint.Offset = 0;

        srcloc.Anonymous.PlacedFootprint.Footprint.Format   = format;
        srcloc.Anonymous.PlacedFootprint.Footprint.Width    = w;
        srcloc.Anonymous.PlacedFootprint.Footprint.Height   = h;
        srcloc.Anonymous.PlacedFootprint.Footprint.Depth    = 1;
        srcloc.Anonymous.PlacedFootprint.Footprint.RowPitch = rowpitch;

        let mut dstloc = Direct3D12::D3D12_TEXTURE_COPY_LOCATION::default();
        dstloc.pResource                  = unsafe { std::mem::transmute_copy(&self.texture) };
        dstloc.Type                       = Direct3D12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstloc.Anonymous.SubresourceIndex = array_level;

        let mut copy_queue = self.dx.copy_queue();

        copy_queue.flush_commands();

        copy_queue.reset();
        unsafe { copy_queue.cmd_list.CopyTextureRegion(&dstloc, x, y, 0, &srcloc, None) };

        if unsafe { copy_queue.cmd_list.Close() }.is_err() {
            panic!("Couldn't close copy command list.");
        }

        unsafe { copy_queue.cmd_queue.ExecuteCommandLists(&[Some(copy_queue.cmd_list.clone().into())]); }

        copy_queue.flush_commands(); // make sure the commands are executed before upload is dropped
    }

    /// Copies entire subresources (levels/layers) from another texture to this one.
    pub fn copy_subresources_from(&self, from: &Texture, subresources: u32) {
        let mut copy_queue = self.dx.copy_queue();
        
        copy_queue.flush_commands();
        copy_queue.reset();

        for r in 0..subresources {
            let mut srcloc = Direct3D12::D3D12_TEXTURE_COPY_LOCATION::default();
            let mut dstloc = Direct3D12::D3D12_TEXTURE_COPY_LOCATION::default();

            srcloc.pResource                  = unsafe { std::mem::transmute_copy(&from.texture) };
            srcloc.Type                       = Direct3D12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcloc.Anonymous.SubresourceIndex = r;

            dstloc.pResource                  = unsafe { std::mem::transmute_copy(&self.texture) };
            dstloc.Type                       = Direct3D12::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstloc.Anonymous.SubresourceIndex = r;

            unsafe { copy_queue.cmd_list.CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, None) };
        }

        if unsafe { copy_queue.cmd_list.Close() }.is_err() {
            panic!("Couldn't close copy command list.");
        }

        unsafe { copy_queue.cmd_queue.ExecuteCommandLists(&[Some(copy_queue.cmd_list.clone().into())]); }

        copy_queue.flush_commands();
    }
}

impl Drop for Texture {
    fn drop(&mut self) {
        self.dx.srv_descriptorheap_addresses.lock().unwrap().reuse.push_back(self.srvheap_loc);
    }
}
