// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::ui;
use crate::overlay;
use crate::dx;

use windows::Win32::Graphics::Direct3D12;
use windows::Win32::Graphics::Direct3D;
use windows::Win32::Graphics::Dxgi;

const VERT_CSO : &str = "shaders/rect.vs.cso";
const PIXEL_CSO: &str = "shaders/rect.ps.cso";

pub struct Rect {
    pso: Direct3D12::ID3D12PipelineState,
}

impl Rect {
    pub fn new() -> Rect {
        debug!("init");

        debug!("Loading vertex shader from {}...", VERT_CSO);
        let vertcso = std::fs::read(VERT_CSO).expect(format!("Couldn't read {}",VERT_CSO).as_str());

        debug!("Loading pixel shader from {}...", PIXEL_CSO);
        let pixelcso = std::fs::read(PIXEL_CSO).expect(format!("Couldn't read {}",PIXEL_CSO).as_str());
    
        let mut psodesc = Direct3D12::D3D12_GRAPHICS_PIPELINE_STATE_DESC::default();
        psodesc.VS.pShaderBytecode = vertcso.as_ptr() as *const _;
        psodesc.VS.BytecodeLength  = vertcso.len();
        psodesc.PS.pShaderBytecode = pixelcso.as_ptr() as *const _;
        psodesc.PS.BytecodeLength  = pixelcso.len();

        psodesc.RasterizerState.FillMode             = Direct3D12::D3D12_FILL_MODE_SOLID;
        psodesc.RasterizerState.CullMode             = Direct3D12::D3D12_CULL_MODE_NONE;
        psodesc.RasterizerState.DepthBias            = Direct3D12::D3D12_DEFAULT_DEPTH_BIAS;
        psodesc.RasterizerState.DepthBiasClamp       = Direct3D12::D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psodesc.RasterizerState.SlopeScaledDepthBias = Direct3D12::D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psodesc.RasterizerState.DepthClipEnable      = true.into();
        psodesc.RasterizerState.ConservativeRaster   = Direct3D12::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        psodesc.BlendState.RenderTarget[0].BlendEnable           = true.into();
        psodesc.BlendState.RenderTarget[0].SrcBlend              = Direct3D12::D3D12_BLEND_ONE;
        psodesc.BlendState.RenderTarget[0].DestBlend             = Direct3D12::D3D12_BLEND_INV_SRC_ALPHA;
        psodesc.BlendState.RenderTarget[0].BlendOp               = Direct3D12::D3D12_BLEND_OP_ADD;
        psodesc.BlendState.RenderTarget[0].SrcBlendAlpha         = Direct3D12::D3D12_BLEND_ONE;
        psodesc.BlendState.RenderTarget[0].DestBlendAlpha        = Direct3D12::D3D12_BLEND_INV_SRC_ALPHA;
        psodesc.BlendState.RenderTarget[0].BlendOpAlpha          = Direct3D12::D3D12_BLEND_OP_ADD;
        psodesc.BlendState.RenderTarget[0].RenderTargetWriteMask = Direct3D12::D3D12_COLOR_WRITE_ENABLE_ALL.0 as u8;

        psodesc.DepthStencilState.DepthEnable   = false.into();
        psodesc.DepthStencilState.StencilEnable = false.into();

        psodesc.SampleMask = std::ffi::c_uint::MAX; //UINT_MAX;
        psodesc.PrimitiveTopologyType = Direct3D12::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psodesc.NumRenderTargets = 1;
        psodesc.RTVFormats[0] = Dxgi::Common::DXGI_FORMAT_R8G8B8A8_UNORM;
        psodesc.SampleDesc.Count = 1;

        let dx = overlay::dx();

        let pso = dx.create_pipeline_state(&mut psodesc,"EG-Overlay D3D12 ui.rect Pipeline State")
            .expect("Couldn't create Rect Pipeline State.");

        Rect {
            pso: pso,
        }
    }

    pub fn draw(&self, frame: &dx::SwapChainLock, x: i64, y: i64, width: i64, height: i64, color: ui::Color) {
        frame.set_pipeline_state(&self.pso);

        let left   = x as f32;
        let top    = y as f32;
        let right  = (x + width) as f32;
        let bottom = (y + height) as f32;

        frame.set_root_constant_float(left  , 0, 0); // left
        frame.set_root_constant_float(top   , 0, 1); // top
        frame.set_root_constant_float(right , 0, 2); // right
        frame.set_root_constant_float(bottom, 0, 3); // bottom
        frame.set_root_constant_color(color , 0, 4); // colorv
        frame.set_root_constant_ortho_proj(   0, 8); // proj

        frame.set_primitive_topology(Direct3D::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        frame.draw_instanced(4, 1, 0, 0);
    }
}


