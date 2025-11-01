// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
dx
==

.. lua:module:: dx

.. code-block:: lua

    local dx = require 'dx'

The :lua:mod:`dx` module contains functions and classes that can be used to draw
in the 3D scene, below the overlay UI.

Most of the rendering details are abstracted away, so all that is needed is an
image to use for a texture and coordinates for display, either in the 3D scene
(the 'world') or on the map/minimap.

.. note::

    This module will render objects regardless of the UI state or MumbleLink
    data.

    Module authors should track these states and only set elements to be drawn
    when appropriate.

Textures
--------

Textures are managed by the :lua:class:`dxtexturemap` class.
A map can hold multiple textures that can be shared by multiple classes that use
them. Module authors are encouraged to create a single map and use it for all
3D rendering with a module.

Dimensions
~~~~~~~~~~

This module does not enforce any restrictions on texture dimensions when loading
texture data, however internally all textures are stored in square textures
with dimensions that are a power of 2.

This should not affect sprites because the original dimensions of the image are
stored and used when rendering them, but trails may not be rendered as expected
if a non-square and/or no-power of 2 image is used.
*/

use windows::Win32::System::Com;
use windows::Win32::Graphics::Imaging;

use std::mem::ManuallyDrop;

use windows::Win32::Graphics::Direct3D12;
use windows::Win32::Graphics::Direct3D;
use windows::Win32::Graphics::Dxgi;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::{luaL_Reg, luaL_Reg_list};

use crate::logging::{debug};
use crate::overlay::lua::{luawarn, luaerror};

use std::sync::{Arc, Mutex, Weak};
use crate::dx;
use crate::ml;
use crate::ui;

use std::collections::{HashMap, VecDeque};

use crate::lamath;

const SPRITE_LIST_VERT_CSO : &str = "shaders/sprite-list.vs.cso";
const SPRITE_LIST_PIXEL_CSO: &str = "shaders/sprite-list.ps.cso";

const TRAIL_VERT_CSO : &str = "shaders/trail.vs.cso";
const TRAIL_PIXEL_CSO: &str = "shaders/trail.ps.cso";

pub struct DxLua {
    dx: Arc<dx::Dx>,
    ml: Arc<ml::MumbleLink>,
    ui: Arc<ui::Ui>,
    sprite_list_pso: Direct3D12::ID3D12PipelineState,
    trail_pso      : Direct3D12::ID3D12PipelineState,

    sprite_lists: Mutex<VecDeque<Arc<SpriteList>>>,
    trail_lists : Mutex<VecDeque<Arc<TrailList>>>,
}

static DX_LUA: Mutex<Option<Arc<DxLua>>> = Mutex::new(None);


pub fn init(dx: &Arc<dx::Dx>, ml: &Arc<ml::MumbleLink>, ui: &Arc<ui::Ui>) {
    debug!("init");

    crate::lua_manager::add_module_opener("dx", Some(open_module));

    *DX_LUA.lock().unwrap() = Some(Arc::new(DxLua {
        dx: dx.clone(),
        ml: ml.clone(),
        ui: ui.clone(),
        sprite_list_pso: create_sprite_list_pso(dx),
        trail_pso: create_trail_pso(dx),

        sprite_lists: Mutex::new(VecDeque::new()),
        trail_lists : Mutex::new(VecDeque::new()),
    }));
}

pub fn cleanup() {
    debug!("cleanup");

    *DX_LUA.lock().unwrap() = None;
}


pub fn render(frame: &mut dx::SwapChainLock) {
    let dx_lua = DX_LUA.lock().unwrap().as_ref().unwrap().clone();

    let fov: f64;

    if let Some(f) = dx_lua.ml.identity_fov() {
        fov = f;
    } else {
        // no FoV means MumbleLink hasn't been initialize and we aren't in game yet.
        return;
    }

    // needed for mouse ray
    let mouse_x = dx_lua.ui.get_last_mouse_x();
    let mouse_y = dx_lua.ui.get_last_mouse_y();

    // data to setup world view/projection matrices
    let rtv_width = frame.render_target_width();
    let rtv_height = frame.render_target_height();

    let mut avatar_pos = dx_lua.ml.avatar_position().clone();
    let mut camera_pos = dx_lua.ml.camera_position().clone();
    let camera_front = dx_lua.ml.camera_front().clone();

    // meters to inches
    avatar_pos.x *= 39.3701;
    avatar_pos.y *= 39.3701;
    avatar_pos.z *= 39.3701;
    camera_pos.x *= 39.3701;
    camera_pos.y *= 39.3701;
    camera_pos.z *= 39.3701;

    let camera_up = lamath::Vec3F {
        x: 0.0,
        y: 1.0,
        z: 0.0,
    };

    // world
    let world_proj = lamath::Mat4F::perspective_lh(fov as f32, rtv_width as f32 / rtv_height as f32, 1.0, 25000.0);
    let world_view = lamath::Mat4F::camera_facing(&camera_pos, &camera_front, &camera_up);

    // data for map view/projection matrices
    let mapscale = dx_lua.ml.context_map_scale();
    let uistate = dx_lua.ml.context_ui_state();

    let mapfullscreen = (uistate & ml::UI_STATE_MAP_OPEN) > 0;

    let mapw: u32;
    let maph: u32;

    let mut minimapleft: u32 = 0;
    let mut minimaptop: u32 = 0;

    if mapfullscreen {
        mapw = rtv_width;
        maph = rtv_height;
    } else {
        mapw = dx_lua.ml.context_compass_width() as u32;
        maph = dx_lua.ml.context_compass_height() as u32;

        minimapleft = rtv_width - mapw;
        if (uistate & ml::UI_STATE_COMPASS_TOP_RIGHT) == 0 {
            match dx_lua.ml.identity_uisz().unwrap() { // unwrap because by now we know identity is working
                0 => minimaptop = rtv_height - 33 - maph, // small
                1 => minimaptop = rtv_height - 35 - maph, // normal
                2 => minimaptop = rtv_height - 42 - maph, // large
                3 => minimaptop = rtv_height - 45 - maph, // larger
                _ => minimaptop = rtv_height - 35 - maph,
            }
        }
    }

    let mapxsize: f32 = mapw as f32 * mapscale;
    let mapysize: f32 = maph as f32 * mapscale;

    let mapleft  : f32 = -mapxsize / 2.0;
    let mapright : f32 = mapxsize / 2.0;
    let maptop   : f32 = -mapysize / 2.0;
    let mapbottom: f32 = mapysize / 2.0;

    let mapcenterx = dx_lua.ml.context_map_center_x();
    let mapcentery = dx_lua.ml.context_map_center_y();

    let map_proj = lamath::Mat4F::ortho(mapleft, mapright, maptop, mapbottom, 0.0, 1.0);

    let map_view_translate = lamath::Mat4F::translate(-mapcenterx, -mapcentery, 0.0);
    let map_view_rotate = if !mapfullscreen && (uistate & ml::UI_STATE_COMPASS_ROTATE) > 0 {
        lamath::Mat4F::rotatez(dx_lua.ml.context_compass_rotation())
    } else {
        lamath::Mat4F::identity()
    };

    let map_view = map_view_translate * map_view_rotate;

    let mouse_ray = calc_mouse_ray(mouse_x, mouse_y, rtv_width, rtv_height, &world_proj, &world_view);

    let mouse_map_x: f32;
    let mouse_map_y: f32;


    let mouse_in_map = mapfullscreen || (
        mouse_x >= minimapleft as i64 &&
        mouse_x <= rtv_width as i64 &&
        mouse_y >= minimaptop as i64 &&
        mouse_y <= minimaptop as i64 + maph as i64);

    if mouse_in_map {
        let centerx: f32 = if mapfullscreen { rtv_width as f32 / 2.0 } else { minimapleft as f32 + (mapw as f32 / 2.0) };
        let centery: f32 = if mapfullscreen { rtv_height as f32 / 2.0 } else { minimaptop as f32 + (maph as f32 / 2.0) };

        mouse_map_x = mapcenterx + ((mouse_x as f32 - centerx) * mapscale);
        mouse_map_y = mapcentery + ((mouse_y as f32 - centery) * mapscale);
    } else {
        mouse_map_x = 0.0;
        mouse_map_y = 0.0;
    }

    let trail_lists = dx_lua.trail_lists.lock().unwrap();

    if trail_lists.len() > 0 {
        frame.set_pipeline_state(&dx_lua.trail_pso);
        frame.set_primitive_topology(Direct3D::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        frame.set_root_constant_vec3f(&avatar_pos       , 0, 36);
        frame.set_root_constant_vec3f(&camera_pos       , 0, 40);
        frame.set_root_constant_float(minimapleft as f32, 0, 45);
        frame.set_root_constant_float(minimaptop  as f32, 0, 46);
        frame.set_root_constant_float(maph        as f32, 0, 47);

        for trail_list in &*trail_lists {
            let mut tl_inner = trail_list.inner.lock().unwrap();

            if !tl_inner.draw { continue; }

            if !tl_inner.is_map && mapfullscreen { continue; }

            if tl_inner.update_vert_buffer {
                tl_inner.update_vertex_buffer(frame, &dx_lua.dx);
            }

            if tl_inner.vert_buffer.is_none() { continue; }

            if tl_inner.is_map {
                frame.set_root_constant_mat4f(&map_view, 0,  0);
                frame.set_root_constant_mat4f(&map_proj, 0, 16);

                if !mapfullscreen {
                    frame.push_viewport(minimapleft as f32, minimaptop as f32, mapw as f32, maph as f32);
                }
            } else {
                frame.set_root_constant_mat4f(&world_view, 0,  0);
                frame.set_root_constant_mat4f(&world_proj, 0, 16);
            }
            frame.set_root_constant_bool(tl_inner.is_map, 0, 39);

            frame.set_vertex_buffer(0, &tl_inner.vert_buffer_view, tl_inner.vert_buffer.as_ref().unwrap());

            let mut first = 0;
            for i in 0..tl_inner.texture_names.len() {
                if tl_inner.trails[i].len() == 0 { continue; }

                let tex_name = &tl_inner.texture_names[i];
                let tex: &dx::Texture;
                let textures = tl_inner.texture_map.textures.lock().unwrap();

                let trails = &tl_inner.trails[i];

                match textures.get(tex_name.as_str()) {
                    Some(t) => tex = &t.texture,
                    _ => {
                        crate::logging::error!("Invalid texture key: {}", tex_name);
                        continue;
                    }
                }

                frame.set_texture(0, tex);

                for trail in trails {
                    if trail.coord_count == 0 { continue; }

                    frame.set_root_constant_float(trail.fade_near, 0, 43);
                    frame.set_root_constant_float(trail.fade_far , 0, 44);
                    frame.set_root_constant_color(trail.color    , 0, 32);

                    frame.draw_instanced(trail.coord_count, 1, first, 0);

                    first += trail.coord_count;
                }
            }

            if tl_inner.is_map && !mapfullscreen { frame.pop_viewport(); }
        }
    }

    let sprite_lists = dx_lua.sprite_lists.lock().unwrap();

    if sprite_lists.len() > 0 {
        frame.set_pipeline_state(&dx_lua.sprite_list_pso);
        frame.set_primitive_topology(Direct3D::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        frame.set_root_constant_vec3f(&avatar_pos       , 0, 32);
        frame.set_root_constant_vec3f(&camera_pos       , 0, 36);
        frame.set_root_constant_float(minimapleft as f32, 0, 39);
        frame.set_root_constant_float(minimaptop  as f32, 0, 40);
        frame.set_root_constant_float(maph        as f32, 0, 41);

        for sprite_list in &*sprite_lists {
            let mut sl_inner = sprite_list.inner.lock().unwrap();

            sl_inner.draw(
                frame,
                &dx_lua.dx,
                &world_proj,
                &world_view,
                &map_proj,
                &map_view,
                mapfullscreen,
                &camera_pos,
                &mouse_ray,
                minimapleft,
                minimaptop,
                mapw,
                maph,
                mouse_map_x,
                mouse_map_y,
                mouse_in_map
            );
        }
    }
}

fn calc_mouse_ray(
    mouse_x: i64,
    mouse_y: i64,
    window_width: u32,
    window_height: u32,
    proj: &lamath::Mat4F,
    view: &lamath::Mat4F
) -> Option<lamath::Vec3F> {
    // https://antongerdelan.net/opengl/raycasting.html
    // this differs a bit since we are using D3D style LH coordinates

    if mouse_x < 0 || mouse_x > window_width as i64 || mouse_y < 0 || mouse_y > window_height as i64 {
        return None;
    }

    let ray_clip = lamath::Vec4F {
        x: (2.0 * mouse_x as f32) / window_width as f32 - 1.0,
        y: 1.0 - (2.0 * mouse_y as f32) / window_height as f32,
        z: 1.0,
        w: 1.0,
    };

    let proj_inv = proj.inverse();

    let mut ray_eye = proj_inv * ray_clip;
    ray_eye.z = 1.0;
    ray_eye.w = 0.0;

    let view_inv = view.inverse();

    let ray_world = view_inv * ray_eye;

    let rayw3 = lamath::Vec3F { x: ray_world.x, y: ray_world.y, z: ray_world.z };

    Some(rayw3.normalize())
}

fn ray_points_at(x: f32, y: f32, z: f32, radius: f32, origin: &lamath::Vec3F, ray: &lamath::Vec3F) -> bool {
    // more from https://antongerdelan.net/opengl/raycasting.html

    // solving for t = -b +/- sqrt(b^2 - c)

    // origin - center (the point we are testing)
    let omc = lamath::Vec3F {
        x: origin.x - x,
        y: origin.y - y,
        z: origin.z - z,
    };

    let b = ray.dot(&omc);
    let c = omc.dot(&omc) - radius.powi(2);

    let dsqr = b.powi(2) - c;

    if dsqr < 0.0 {
        // miss
        return false;
    }

    if dsqr == 0.0 {
        // a special case, an exact intersection on the boundary radius
        let t = -b; // sqrt(0) = 0

        if t > 0.0 {
            return true;
        }
    } else {
        // the mouse is pointed somewhere inside the radius, check both solutions
        let t1 = -b + dsqr.sqrt();
        let t2 = -b - dsqr.sqrt();

        if t1 > 0.0 || t2 > 0.0 {
            return true;
        }
    }

    // a miss
    false
}

macro_rules! inst_input {
    ($name:literal, $index:literal, $format:expr, $slot:literal, $offset:literal, $step: literal) => {{
        Direct3D12::D3D12_INPUT_ELEMENT_DESC {
            SemanticName: windows::core::s!($name),
            SemanticIndex: $index,
            Format: $format,
            InputSlot: $slot,
            AlignedByteOffset: $offset,
            InputSlotClass: Direct3D12::D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
            InstanceDataStepRate: $step,
        }
    }}
}

macro_rules! vert_input {
    ($name:literal, $index:literal, $format:expr, $slot:literal, $offset:literal, $step: literal) => {{
        Direct3D12::D3D12_INPUT_ELEMENT_DESC {
            SemanticName: windows::core::s!($name),
            SemanticIndex: $index,
            Format: $format,
            InputSlot: $slot,
            AlignedByteOffset: $offset,
            InputSlotClass: Direct3D12::D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            InstanceDataStepRate: $step,
        }
    }}
}

fn create_sprite_list_pso(dx: &Arc<dx::Dx>) -> Direct3D12::ID3D12PipelineState {
    debug!("Loading sprite list vertex shader from {}...", SPRITE_LIST_VERT_CSO);
    let vertcso = std::fs::read(SPRITE_LIST_VERT_CSO).expect(format!("Couldn't read {}", SPRITE_LIST_VERT_CSO).as_str());

    debug!("Loading sprite list pixel shader from {}...", SPRITE_LIST_PIXEL_CSO);
    let pixelcso = std::fs::read(SPRITE_LIST_PIXEL_CSO).expect(format!("Couldn't read {}", SPRITE_LIST_PIXEL_CSO).as_str());

    let inputs = [
        inst_input!{"POSITION" , 0, Dxgi::Common::DXGI_FORMAT_R32G32B32_FLOAT   , 0,   0, 1},
        inst_input!{"MAX_U"    , 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  12, 1},
        inst_input!{"MAX_V"    , 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  16, 1},
        inst_input!{"XY_RATIO" , 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  20, 1},
        inst_input!{"SIZE"     , 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  24, 1},
        inst_input!{"FADE_NEAR", 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  28, 1},
        inst_input!{"FADE_FAR" , 0, Dxgi::Common::DXGI_FORMAT_R32_FLOAT         , 0,  32, 1},
        inst_input!{"COLOR"    , 0, Dxgi::Common::DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  36, 1},
        inst_input!{"FLAGS"    , 0, Dxgi::Common::DXGI_FORMAT_R32_UINT          , 0,  52, 1},
        inst_input!{"ROTATION" , 0, Dxgi::Common::DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  56, 1},
        inst_input!{"ROTATION" , 1, Dxgi::Common::DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  72, 1},
        inst_input!{"ROTATION" , 2, Dxgi::Common::DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  88, 1},
        inst_input!{"ROTATION" , 3, Dxgi::Common::DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 104, 1},
    ];

    let mut psodesc = Direct3D12::D3D12_GRAPHICS_PIPELINE_STATE_DESC::default();

    psodesc.InputLayout.NumElements = inputs.len() as u32;
    psodesc.InputLayout.pInputElementDescs = inputs.as_ptr();

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

    psodesc.DepthStencilState.DepthEnable    = true.into();
    psodesc.DepthStencilState.DepthFunc      = Direct3D12::D3D12_COMPARISON_FUNC_LESS;
    psodesc.DepthStencilState.DepthWriteMask = Direct3D12::D3D12_DEPTH_WRITE_MASK_ALL;
    psodesc.DepthStencilState.StencilEnable  = false.into();
    psodesc.DSVFormat                        = Dxgi::Common::DXGI_FORMAT_D32_FLOAT;

    psodesc.SampleMask = std::ffi::c_uint::MAX; //UINT_MAX;
    psodesc.PrimitiveTopologyType = Direct3D12::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psodesc.NumRenderTargets = 1;
    psodesc.RTVFormats[0] = Dxgi::Common::DXGI_FORMAT_R8G8B8A8_UNORM;
    psodesc.SampleDesc.Count = 1;

    let pso = dx.create_pipeline_state(&mut psodesc, "EG-Overlay D3D12 Sprite List Pipeline State")
        .expect("Couldn't create sprite list pipeline state.");

    return pso;
}

fn create_trail_pso(dx: &Arc<dx::Dx>) -> Direct3D12::ID3D12PipelineState {
    debug!("Loading trail vertex shader from {}...", TRAIL_VERT_CSO);
    let vertcso = std::fs::read(TRAIL_VERT_CSO).expect(format!("Couldn't read {}", TRAIL_VERT_CSO).as_str());

    debug!("Loading trail pixel shader from {}...", TRAIL_PIXEL_CSO);
    let pixelcso = std::fs::read(TRAIL_PIXEL_CSO).expect(format!("Couldn't read {}", TRAIL_PIXEL_CSO).as_str());

    let inputs = [
        vert_input!{"POSITION", 0, Dxgi::Common::DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, 0},
        vert_input!{"TEXUV"   , 0, Dxgi::Common::DXGI_FORMAT_R32G32_FLOAT   , 0, 12, 0},
    ];

    let mut psodesc = Direct3D12::D3D12_GRAPHICS_PIPELINE_STATE_DESC::default();

    psodesc.InputLayout.NumElements = inputs.len() as u32;
    psodesc.InputLayout.pInputElementDescs = inputs.as_ptr();

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

    psodesc.DepthStencilState.DepthEnable    = true.into();
    psodesc.DepthStencilState.DepthFunc      = Direct3D12::D3D12_COMPARISON_FUNC_LESS;
    psodesc.DepthStencilState.DepthWriteMask = Direct3D12::D3D12_DEPTH_WRITE_MASK_ALL;
    psodesc.DepthStencilState.StencilEnable  = false.into();
    psodesc.DSVFormat                        = Dxgi::Common::DXGI_FORMAT_D32_FLOAT;

    psodesc.SampleMask = std::ffi::c_uint::MAX; //UINT_MAX;
    psodesc.PrimitiveTopologyType = Direct3D12::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psodesc.NumRenderTargets = 1;
    psodesc.RTVFormats[0] = Dxgi::Common::DXGI_FORMAT_R8G8B8A8_UNORM;
    psodesc.SampleDesc.Count = 1;

    let pso = dx.create_pipeline_state(&mut psodesc, "EG-Overlay D3D12 Trail Pipeline State")
        .expect("Couldn't create trail pipeline state.");

    return pso;
}

/*** RST
Functions
---------

*/
const DX_LUA_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"texturemap", texturemap_new,
    c"spritelist", spritelist_new,
    c"traillist" , traillist_new,
};

/*** RST
.. lua:function:: texturemap()

    Create a new :lua:class:`dxtexturemap` object.

    :rtype: dxtexturemap

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn texturemap_new(l: &lua_State) -> i32 {
    let tm: Arc<TextureMap> = Arc::new(TextureMap {
        textures: Mutex::new(HashMap::new()),
    });

    let tm_ptr = Arc::into_raw(tm.clone());

    let lua_tm_ptr: *mut *const TextureMap = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const TextureMap>(), 0))
    };

    unsafe { *lua_tm_ptr = tm_ptr; }

    if lua::L::newmetatable(l, TEXTUREMAP_METATABLE_NAME) {
        let dx_lua_ptr = Weak::into_raw(Arc::downgrade(&DX_LUA.lock().unwrap().as_ref().unwrap().clone()));

        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");
        unsafe { lua::pushlightuserdata(l, dx_lua_ptr as *const std::ffi::c_void); }
        lua::L::setfuncs(l, TEXTUREMAP_FUNCS, 1);
    }
    lua::setmetatable(l, -2);

    return 1;
}

/*** RST
.. lua:function:: spritelist(texturemap[, location])

    Create a new :lua:class:`dxspritelist` object.

    :param dxtexturemap texturemap:
    :param string location: (Optional) How the sprites in this list will be
        positioned. See below. Default: ``'world'``.
    :rtype: dxspritelist

    **Location Values**

    =========== ================================================================
    Value       Description
    =========== ================================================================
    ``'world'`` Sprites are drawn within the 3D world, coordinates must be in
                map coordinates.
    ``'map'``   Sprites are dawn on the (mini)map, coordinates must be in
                continent coordinates.
    =========== ================================================================

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn spritelist_new(l: &lua_State) -> i32 {
    let tm = unsafe { checktexturemap(l, 1) };

    let mut is_map = false;

    if lua::gettop(l) >= 2 {
        if let Some(loc) = lua::tostring(l, 2) {
            match loc.as_str() {
                "map" => is_map = true,
                "world" => is_map = false,
                _ => {
                    luaerror!(l, "location must be 'map' or 'world'");
                    return 0;
                }
            }
        } else {
            luaerror!(l, "location must be 'map' or 'world'");
            return 0;
        }
    }

    let inner = SpriteListInner {
        vert_buffer: None,
        vert_buffer_view: Direct3D12::D3D12_VERTEX_BUFFER_VIEW::default(),

        vert_buffer_size: 0,
        update_vert_buffer: false,

        texture_names: Vec::new(),
        sprite_data  : Vec::new(),
        sprite_tags  : Vec::new(),
        mouse_test   : Vec::new(),

        texture_map: (*tm).clone(),

        mouse_hover_tags: Vec::new(),

        is_map: is_map,

        draw: true,
    };


    let sl: Arc<SpriteList> = Arc::new(SpriteList {
        inner: Mutex::new(inner),
    });

    let sl_ptr = Arc::into_raw(sl.clone());

    let lua_sl_ptr: *mut *const SpriteList = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const SpriteList>(), 0))
    };

    unsafe { *lua_sl_ptr = sl_ptr; }

    if lua::L::newmetatable(l, SPRITELIST_METATABLE_NAME) {
        let dx_lua_ptr = Weak::into_raw(Arc::downgrade(&DX_LUA.lock().unwrap().as_ref().unwrap().clone()));

        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");
        unsafe { lua::pushlightuserdata(l, dx_lua_ptr as *const std::ffi::c_void); }
        lua::L::setfuncs(l, SPRITELIST_FUNCS, 1);
    }
    lua::setmetatable(l, -2);

    let dx_lua = get_dx_lua_upvalue(l).unwrap();

    dx_lua.sprite_lists.lock().unwrap().push_back(sl);

    return 1;
}

/*** RST
.. lua:function:: traillist(texturemap[, location])

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn traillist_new(l: &lua_State) -> i32 {
    let tm = unsafe { checktexturemap(l, 1) };

    let mut is_map = false;

    if lua::gettop(l) >= 2 {
        if let Some(loc) = lua::tostring(l, 2) {
            match loc.as_str() {
                "map" => is_map = true,
                "world" => is_map = false,
                _ => {
                    luaerror!(l, "location must be 'map' or 'world'");
                    return 0;
                }
            }
        } else {
            luaerror!(l, "location must be 'map' or 'world'");
            return 0;
        }
    }

    let inner = TrailListInner {
        vert_buffer: None,
        vert_buffer_view: Direct3D12::D3D12_VERTEX_BUFFER_VIEW::default(),

        vert_buffer_size: 0,
        update_vert_buffer: false,

        texture_map: (*tm).clone(),

        texture_names: Vec::new(),
        trails: Vec::new(),

        is_map: is_map,
        draw: true,
    };

    let tl: Arc<TrailList> = Arc::new(TrailList {
        inner: Mutex::new(inner),
    });

    let tl_ptr = Arc::into_raw(tl.clone());

    let lua_tl_ptr: *mut *const TrailList = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const TrailList>(), 0))
    };

    unsafe { *lua_tl_ptr = tl_ptr; }

    if lua::L::newmetatable(l, TRAILLIST_METATABLE_NAME) {
        let dx_lua_ptr = Weak::into_raw(Arc::downgrade(&DX_LUA.lock().unwrap().as_ref().unwrap().clone()));

        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");
        unsafe { lua::pushlightuserdata(l, dx_lua_ptr as *const std::ffi::c_void); }
        lua::L::setfuncs(l, TRAILLIST_FUNCS, 1);
    }
    lua::setmetatable(l, -2);

    let dx_lua = get_dx_lua_upvalue(l). unwrap();

    dx_lua.trail_lists.lock().unwrap().push_back(tl);

    return 1;
}

unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    let dx_lua_ptr = Weak::into_raw(Arc::downgrade(&DX_LUA.lock().unwrap().as_ref().unwrap().clone()));

    lua::newtable(l);
    unsafe { lua::pushlightuserdata(l, dx_lua_ptr as *const std::ffi::c_void); }
    lua::L::setfuncs(l, DX_LUA_FUNCS, 1);

    return 1;
}

fn get_dx_lua_upvalue(l: &lua_State) -> Option<Arc<DxLua>> {
    let dx_lua_ptr: *const DxLua = unsafe {
        std::mem::transmute(lua::touserdata(l, lua::LUA_REGISTRYINDEX -1))
    };

    let dx_lua_weak = ManuallyDrop::new(unsafe { Weak::from_raw(dx_lua_ptr) });

    dx_lua_weak.upgrade()
}


/*** RST
Classes
-------

.. lua:class:: dxtexturemap

    A texture map holds a list of textures that other objects in this module use
    when being displayed.
*/
struct TextureMap {
    textures: Mutex<HashMap<String, Arc<Texture>>>,
}

impl TextureMap {
    pub fn get(&self, name: &str) -> Option<Arc<Texture>> {
        match self.textures.lock().unwrap().get(name) {
            Some(t) => Some(t.clone()),
            None    => None,
        }
    }
}

struct Texture {
    //size: u32,
    max_u: f32,
    max_v: f32,
    xy_ratio: f32,
    texture: dx::Texture,
}

const TEXTUREMAP_METATABLE_NAME: &str = "dx::lua::TextureMap";

const TEXTUREMAP_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc" , texturemap_gc,
    c"clear", texturemap_clear,
    c"add"  , texturemap_add,
    c"has"  , texturemap_has,
};


unsafe fn checktexturemap(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<TextureMap>> {
    let tm_lua_ptr: *mut *const TextureMap = unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, TEXTUREMAP_METATABLE_NAME))
    };

    ManuallyDrop::new(unsafe { Arc::from_raw(*tm_lua_ptr) } )
}

unsafe extern "C" fn texturemap_gc(l: &lua_State) -> i32 {
    let mut tm = unsafe { checktexturemap(l, 1) };

    unsafe { ManuallyDrop::drop(&mut tm); }

    return 0;
}

/*** RST
    .. lua:method:: clear()

        Remove all textures from this map.

        .. danger::

            If objects are still referencing textures within this map after this
            method is called, their draws will not function properly.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn texturemap_clear(l: &lua_State) -> i32 {
    let tm = unsafe { checktexturemap(l, 1) };

    tm.textures.lock().unwrap().clear();

    return 0;
}

/*** RST
    .. lua:method:: add(name, data, mipmaps)

        Add a texture.

        :param string name: The name of the texture, this will be used to
            reference it later when adding data to sprite lists and other
            objects.
        :param string data: The texture data.
        :param boolean mipmaps: Generate mipmaps, default ``true``.


        .. admonition:: Implementation Detail

            EG-Overlay uses the `Windows Imaging Component <https://learn.microsoft.com/en-us/windows/win32/wic/-wic-lh>`_
            to load ``data``, so any `format <https://learn.microsoft.com/en-us/windows/win32/wic/native-wic-codecs>`_
            it supports can be used.

            All textures are loaded as 4 channel BGRA images.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn texturemap_add(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);

    if lua::gettop(l) < 3 {
        lua::pushstring(l, "texturemap:add takes at least 2 arguments");
        unsafe { lua::error(l); }
    }

    let tm = unsafe { checktexturemap(l, 1) };
    let name = lua::tostring(l, 2).unwrap();
    let data: &[u8] = lua::tobytes(l, 3);

    let mut mipmaps = true;

    if lua::gettop(l) >= 4 {
        mipmaps = lua::toboolean(l, 4);
    }

    let mut textures = tm.textures.lock().unwrap();

    if textures.contains_key(&name) {
        luawarn!(l, "Texture {} already exists in this texturemap, overwriting.", name);
    }

    let dx_lua = get_dx_lua_upvalue(l).unwrap();

    // We'll use Windows Imaging Component to load the image data in. It's already
    // present in Windows and can handle any of the formats we should be concerned
    // with already. It'll also help with creating the mipmaps.
    let wicfactory: Imaging::IWICImagingFactory;

    // Get the factory
    match unsafe { Com::CoCreateInstance::<_, Imaging::IWICImagingFactory>(
        &Imaging::CLSID_WICImagingFactory,
        None,
        Com::CLSCTX_INPROC_SERVER
    ) } {
        Ok(fac) => wicfactory = fac,
        Err(err) => {
            luaerror!(l, "Couldn't create WIC factory: {}", err);
            return 0;
        }
    }

    let memstream : Imaging::IWICStream;
    let decoder   : Imaging::IWICBitmapDecoder;
    let frame     : Imaging::IWICBitmapFrameDecode;
    let converter : Imaging::IWICFormatConverter;
    let bitmap    : Imaging::IWICBitmap;
    let bitmaplock: Imaging::IWICBitmapLock;

    let mut pixels_len: u32     = 0;
    let mut pixels    : *mut u8 = std::ptr::null_mut();

    let mut width: u32 = 0;
    let mut height: u32 = 0;

    // Create a stream to hold the image data that we are feeding in
    match unsafe { wicfactory.CreateStream() } {
        Ok(strm) => memstream = strm,
        Err(err) => {
            luaerror!(l, "Couldn't create a WIC stream: {}", err);
            return 0;
        }
    }

    if let Err(err) = unsafe { memstream.InitializeFromMemory(data) } {
        luaerror!(l, "Couldn't initialize texture stream: {}", err);
        return 0;
    }

    // Create a decoder for the input stream. If this errors with
    // "Component not found" that usually means the data is invalid or the file
    // format isn't one WIC can decode.
    match unsafe { wicfactory.CreateDecoderFromStream(
        &memstream,
        std::ptr::null() as *const _,
        Imaging::WICDecodeMetadataCacheOnDemand
    ) } {
        Ok(dec) => decoder = dec,
        Err(err) => {
            luaerror!(l, "Couldn't get image decoder: {}", err);
            return 0;
        }
    }

    // Get a frame...most images only have a single frame.
    match unsafe { decoder.GetFrame(0) } {
        Ok(frm) => frame = frm,
        Err(err) => {
            luaerror!(l, "Couldn't get image frame: {}", err);
            return 0;
        }
    }

    // Create a converter to convert the data from whatever format it happens
    // to be in to the exact format we want.
    match unsafe { wicfactory.CreateFormatConverter() } {
        Ok(con) => converter = con,
        Err(err) => {
            luaerror!(l, "Couldn't create image format converter: {}", err);
            return 0;
        }
    }

    // Initialize the converter with our input data frame and set the output
    // format.
    // BGRA here because RGBA was causing some weird things with B-R swapping
    // channels in mipmaps. weird
    if let Err(err) = unsafe { converter.Initialize(
        &frame,
        &Imaging::GUID_WICPixelFormat32bppBGRA,
        Imaging::WICBitmapDitherTypeNone,
        None,
        0.0,
        Imaging::WICBitmapPaletteTypeCustom
    ) } {
        luaerror!(l, "Couldn't initialize image converter: {}", err);
        return 0;
    }

    // Create a bitmap that reads from the output of the converter above. Reading
    // from this bitmap will be reading converted pixel data.
    match unsafe { wicfactory.CreateBitmapFromSource(&converter, Imaging::WICBitmapCacheOnDemand) } {
        Ok(bm) => bitmap = bm,
        Err(err) => {
            luaerror!(l, "Couldnm't create WIC bitmap: {}", err);
            return 0;
        }
    }

    // We can finally see how big the image is too.
    if let Err(err) = unsafe { bitmap.GetSize(&mut width, &mut height) } {
        luaerror!(l, "Couldn't get bitmap size: {}", err);
        return 0;
    }

    // In order to read the image data, we have to define what part we want.
    // In this case, the entire thing.
    let lockrect = Imaging::WICRect { X: 0, Y: 0, Width: width as i32, Height: height as i32};

    // Then lock it for reading.
    match unsafe { bitmap.Lock(&lockrect, Imaging::WICBitmapLockRead.0 as u32) } {
        Ok(lk) => bitmaplock = lk,
        Err(err) => {
            luaerror!(l, "Couldn't lock bitmap: {}", err);
            return 0;
        }
    }

    // Now we get a raw pointer and length. Woo!
    if let Err(err) = unsafe { bitmaplock.GetDataPointer(&mut pixels_len, &mut pixels) } {
        luaerror!(l, "Couldn't get bitmap data pointer: {}", err);
        return 0;
    }

    // convert it to a slice for more convenient usage in Rust
    let pixels_slice: &[u8] = unsafe { std::slice::from_raw_parts(pixels, pixels_len as usize) };

    // At this point pixels should have valid image data in our desired format.
    // Now, we can create the texture.

    // How big must we make a texture to hold our image? We'll use square,
    // power of 2 texture sizes (8, 16, 32, 64, etc.)
    let mut req_size = 1;
    while req_size < width || req_size < height { req_size <<= 1; }

    let xy_ratio = width  as f32 / height   as f32;
    let max_u    = width  as f32 / req_size as f32;
    let max_v    = height as f32 / req_size as f32;

    let mipmaplevels = if mipmaps {
        (req_size as f64).log2().floor() as u16
    } else { 1 };

    let tex = dx_lua.dx.new_texture_2d(
        Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM,
        req_size, req_size, mipmaplevels
    );
    tex.set_name(format!("EG-Overlay D3D12 TextureMap Texture: {}", name).as_str());
    tex.write_pixels(0, 0, 0, width, height, Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM, pixels_slice);

    // At this point we are done with the pixel data, so release the lock.
    // pixels and pixels_slice are now invalid
    drop(bitmaplock);

    // Now generate mipmaps
    for mlevel in 1..mipmaplevels {
        let mipsize: u32 = req_size / 2.0f32.powi(mlevel as i32) as u32;
        let mipw: u32 = (mipsize as f32 * max_u).floor() as u32;
        let miph: u32 = (mipsize as f32 * max_v).floor() as u32;

        let scaler      : Imaging::IWICBitmapScaler;
        let scaledbitmap: Imaging::IWICBitmap;
        let scaledlock  : Imaging::IWICBitmapLock;

        let scaledrect = Imaging::WICRect { X: 0, Y: 0, Width: mipw as i32, Height: miph as i32 };

        // This is much the same as using the converter above, but with a scaler
        // this time.
        match unsafe { wicfactory.CreateBitmapScaler() } {
            Ok(sc) => scaler = sc,
            Err(err) => {
                luaerror!(l, "Couldn't create bitmap scaler: {}", err);
                return 0;
            }
        }

        if let Err(err) = unsafe { scaler.Initialize(
            &bitmap,
            mipw,
            miph,
            Imaging::WICBitmapInterpolationModeFant // this could eventually be an option to the function
        ) } {
            luaerror!(l, "Couldn't initialize bitmap scaler: {}", err);
            return 0;
        }

        match unsafe { wicfactory.CreateBitmapFromSource(&scaler, Imaging::WICBitmapCacheOnDemand) } {
            Ok(bm) => scaledbitmap = bm,
            Err(err) => {
                luaerror!(l, "Couldn't create scaled bitmap: {}", err);
                return 0;
            }
        }

        match unsafe { scaledbitmap.Lock(&scaledrect, Imaging::WICBitmapLockRead.0 as u32) } {
            Ok(lk) => scaledlock = lk,
            Err(err) => {
                luaerror!(l, "Couldn't lock scaled bitmap: {}", err);
                return 0;
            }
        }

        // raw pixel data, same as above
        let mut mippixels_len: u32 = 0;
        let mut mippixels    : *mut u8 = std::ptr::null_mut();

        if let Err(err) = unsafe { scaledlock.GetDataPointer(&mut mippixels_len, &mut mippixels) } {
            luaerror!(l, "Couldn't get mipmap pixels pointer: {}", err);
            return 0;
        }

        let mippixels_slice: &[u8] = unsafe { std::slice::from_raw_parts(mippixels, mippixels_len as usize) };
        tex.write_pixels(0, 0, mlevel as u32, mipw, miph, Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM, mippixels_slice);
    }

    let t = Texture {
        //size: req_size,
        max_u: max_u,
        max_v: max_v,
        xy_ratio: xy_ratio,
        texture: tex,
    };

    textures.insert(name.clone(), Arc::new(t));

    return 0;
}

/*** RST
    .. lua:method:: has(name)

        Returns ``true`` if this map has a texture named ``name``.

        :param string name:

        :rtype: boolean

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn texturemap_has(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);
    let tm = unsafe { checktexturemap(l, 1) };
    let name = lua::tostring(l, 2).unwrap();

    lua::pushboolean(l, tm.textures.lock().unwrap().contains_key(&name));

    return 1;
}

/*** RST
.. lua:class:: dxspritelist
*/

struct SpriteList {
    inner: Mutex<SpriteListInner>,
}

struct SpriteListInner {
    vert_buffer: Option<Direct3D12::ID3D12Resource>,
    vert_buffer_view: Direct3D12::D3D12_VERTEX_BUFFER_VIEW,

    vert_buffer_size: usize,
    update_vert_buffer: bool,

    // Sprites are grouped by texture name because we can render each set that
    // shares the same texture in a single command.
    // This is not a HashMap because the order of the textures and data needs
    // to be consistent and predictable.
    texture_names: Vec<String>,
    sprite_data: Vec<Vec<SpriteListSprite>>,
    sprite_tags: Vec<Vec<i64>>,
    mouse_test: Vec<Vec<bool>>,

    texture_map: Arc<TextureMap>,

    mouse_hover_tags: Vec<i64>,

    is_map: bool,

    draw: bool,
}

const SPRITE_MEM_SIZE: usize = std::mem::size_of::<SpriteListSprite>();

impl SpriteListInner {
    fn draw(&mut self,
        frame: &mut dx::SwapChainLock,
        dx: &Arc<dx::Dx>,
        world_proj: &lamath::Mat4F,
        world_view: &lamath::Mat4F,
        map_proj: &lamath::Mat4F,
        map_view: &lamath::Mat4F,
        mapfullscreen: bool,
        camera: &lamath::Vec3F,
        mouse_ray: &Option<lamath::Vec3F>,
        minimapleft: u32,
        minimaptop: u32,
        mapw: u32,
        maph: u32,
        mouse_map_x: f32,
        mouse_map_y: f32,
        mouse_in_map: bool

    ) {
        self.mouse_hover_tags.clear();

        if !self.draw { return; }

        if !self.is_map && mapfullscreen { return; }

        if self.update_vert_buffer {
            self.update_vertex_buffer(frame, dx);
        }

        if self.vert_buffer.is_none() { return; }

        if self.is_map {
            frame.set_root_constant_mat4f(map_view, 0,  0);
            frame.set_root_constant_mat4f(map_proj, 0, 16);

            if !mapfullscreen {
                frame.push_viewport(minimapleft as f32, minimaptop as f32, mapw as f32, maph as f32);
            }
        } else {
            frame.set_root_constant_mat4f(world_view, 0,  0);
            frame.set_root_constant_mat4f(world_proj, 0, 16);
        }

        frame.set_root_constant_bool (self.is_map   , 0, 35);

        frame.set_vertex_buffer(0, &self.vert_buffer_view, self.vert_buffer.as_ref().unwrap());

        let mut inst: u32 = 0;
        for i in 0..self.texture_names.len() {
            let tex_name = &self.texture_names[i];
            let tex: &dx::Texture;

            let sprite_data = &self.sprite_data[i];
            let sprite_count = sprite_data.len() as u32;

            if sprite_count == 0 { continue; }

            let textures = self.texture_map.textures.lock().unwrap();
            match textures.get(tex_name.as_str()) {
                Some(t) => tex = &t.texture,
                _ => {
                    crate::logging::error!("Invalid texture key: {}", tex_name);
                    continue;
                },
            }

            frame.set_texture(0, tex);

            frame.draw_instanced(4, sprite_count, 0, inst);
            inst += sprite_count;

            if mouse_ray.is_none() && !self.is_map { continue; }

            for s in 0..sprite_data.len() {
                let mouse_test = self.mouse_test[i][s];

                if !mouse_test { continue; }

                let tags = self.sprite_tags[i][s];
                let sprite = &self.sprite_data[i][s];

                if !self.is_map && !mouse_in_map {
                    if ray_points_at(sprite.x, sprite.y, sprite.z, sprite.size / 2.0, camera, mouse_ray.as_ref().unwrap()) {
                        self.mouse_hover_tags.push(tags);
                    }
                } else if self.is_map && mouse_in_map {
                    let searchdistsq = (sprite.size / 2.0).powi(2);

                    let mousedistsq = (mouse_map_x - sprite.x).powi(2) + (mouse_map_y - sprite.y).powi(2);

                    if mousedistsq <= searchdistsq {
                        self.mouse_hover_tags.push(tags);
                    }
                }

            }
        }

        if self.is_map && !mapfullscreen { frame.pop_viewport(); }
    }

    fn update_vertex_buffer(&mut self, frame: &mut dx::SwapChainLock, dx: &Arc<dx::Dx>) {
        let mut new_size = 0;
        for s in &self.sprite_data {
            new_size += SPRITE_MEM_SIZE * s.len();
        }

        frame.flush_commands();

        if new_size == 0 {
            self.vert_buffer = None;
            self.vert_buffer_size = new_size;

            return;
        } else if self.vert_buffer_size != new_size {
            let vb = dx.new_vertex_buffer(new_size as u64);
            crate::dx::object_set_name(&vb, "EG-Overlay D3D12 SpriteList Vertex Buffer");
            self.vert_buffer_size = new_size;

            self.vert_buffer_view.BufferLocation = unsafe { vb.GetGPUVirtualAddress() };
            self.vert_buffer_view.SizeInBytes = new_size as u32;
            self.vert_buffer_view.StrideInBytes = SPRITE_MEM_SIZE as u32;

            self.vert_buffer = Some(vb);
        }

        let upload = dx.new_upload_buffer(self.vert_buffer_size as u64);
        crate::dx::object_set_name(&upload, "EG-Overlay D3D12 SpriteList Temp. Upload Buffer");

        let mut data: *mut std::ffi::c_void = std::ptr::null_mut();
        let rr = Direct3D12::D3D12_RANGE::default();

        if unsafe { upload.Map(0, Some(&rr), Some(&mut data)) }.is_err() {
            panic!("Couldn't map sprite list upload data.");
        }

        let mut offset = 0;
        for sprites in &self.sprite_data {
            let sprites_size = SPRITE_MEM_SIZE * sprites.len();
            if sprites_size == 0 { continue; }
            unsafe {
                std::ptr::copy_nonoverlapping(sprites.as_ptr() as *const std::ffi::c_void, data.add(offset), sprites_size);
            }
            offset += sprites_size;
        }

        unsafe { upload.Unmap(0, None); }

        let mut copy = dx.copy_queue();
        copy.copy_resource(&upload, self.vert_buffer.as_ref().unwrap());

        self.update_vert_buffer = false;
    }

    fn update_matching(&mut self, l: &lua_State) -> i32 {
        let mut nupdated = 0;

        for ti in 0..self.sprite_data.len() {
            let sprites = &mut self.sprite_data[ti];
            let tags = &self.sprite_tags[ti];

            for si in 0..sprites.len() {
                let sprite = &mut sprites[si];
                let tag    = &tags[si];

                if *tag < 0 { continue; }

                lua::geti(l, lua::LUA_REGISTRYINDEX, *tag);
                let spritetagsind = lua::gettop(l);

                if tags_match(l, spritetagsind, 2) {
                    sprite.update_from_lua_table(l, 3);
                    nupdated += 1;
                }
                lua::pop(l, 1);
            }

        }

        if nupdated > 0 { self.update_vert_buffer = true; }

        lua::pushinteger(l, nupdated);

        return 1;
    }

    fn remove_matching(&mut self, l: &lua_State) -> i32 {
        let mut nremoved = 0;

        for ti in 0..self.sprite_data.len() {
            let sprites = &mut self.sprite_data[ti];
            let tags    = &mut self.sprite_tags[ti];

            let mut si = 0;
            while si < sprites.len() {
                let tag = &tags[si];

                if *tag < 0 {
                    si += 1;
                    continue;
                }

                lua::geti(l, lua::LUA_REGISTRYINDEX, *tag);
                let spritetagsind = lua::gettop(l);

                if tags_match(l, spritetagsind, 2) {
                    lua::L::unref(l, lua::LUA_REGISTRYINDEX, *tag);

                    sprites.remove(si);
                    tags.remove(si);
                    nremoved += 1;
                } else {
                    si += 1;
                }
                lua::pop(l, 1);
            }
        }

        if nremoved > 0 { self.update_vert_buffer = true; }

        lua::pushinteger(l, nremoved);

        return 1;
    }
}

// repr(C) because this a Vec of these will be directly copied into a vertex
// buffer
#[repr(C)]
struct SpriteListSprite {
    x: f32,
    y: f32,
    z: f32,

    max_u: f32,
    max_v: f32,
    xy_ratio: f32,

    size: f32,

    fade_near: f32,
    fade_far : f32,

    r: f32,
    g: f32,
    b: f32,
    a: f32,

    flags: u32,

    rotation: lamath::Mat4F,
}

impl SpriteListSprite {
    fn update_from_lua_table(&mut self, l: &lua_State, table: i32) {
        if lua::getfield(l, table, "x") != lua::LuaType::LUA_TNIL { self.x = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "y") != lua::LuaType::LUA_TNIL { self.y = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "z") != lua::LuaType::LUA_TNIL { self.z = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "size") != lua::LuaType::LUA_TNIL { self.size = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "fadenear") != lua::LuaType::LUA_TNIL { self.fade_near = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "fadefar") != lua::LuaType::LUA_TNIL { self.fade_far = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "billboard") != lua::LuaType::LUA_TNIL {
            let billboard = if lua::toboolean(l, -1) { 1 } else { 0 };
            self.flags = (self.flags & !0x01) | billboard;
        }
        lua::pop(l, 1);

        if lua::getfield(l, table, "color") != lua::LuaType::LUA_TNIL {
            let color = crate::ui::Color::from(lua::tointeger(l, -1));
            self.r = color.r_f32();
            self.g = color.g_f32();
            self.b = color.b_f32();
            self.a = color.a_f32();
        }
        lua::pop(l, 1);

        if lua::getfield(l, table, "rotation") != lua::LuaType::LUA_TNIL {
            let x: f32;
            let y: f32;
            let z: f32;

            lua::geti(l, -1, 1); x = lua::tonumber(l, -1) as f32; lua::pop(l, 1);
            lua::geti(l, -1, 2); y = lua::tonumber(l, -1) as f32; lua::pop(l, 1);
            lua::geti(l, -1, 3); z = lua::tonumber(l, -1) as f32; lua::pop(l, 1);

            let xr = lamath::Mat4F::rotatex(x.to_radians());
            let yr = lamath::Mat4F::rotatey(y.to_radians());
            let zr = lamath::Mat4F::rotatez(z.to_radians());

            let zy = zr * yr;
            self.rotation = zy * xr;
        }
        lua::pop(l, 1);
    }
}

const SPRITELIST_METATABLE_NAME: &str = "dx::lua::SpriteList";

const SPRITELIST_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"          , spritelist_gc,
    c"add"           , spritelist_add,
    c"draw"          , spritelist_draw,
    c"update"        , spritelist_update,
    c"remove"        , spritelist_remove,
    c"clear"         , spritelist_clear,
    c"mousehovertags", spritelist_mouse_hover_tags,
};

unsafe fn checkspritelist(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<SpriteList>> {
    let ptr: *mut *const SpriteList = unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, SPRITELIST_METATABLE_NAME))
    };

    ManuallyDrop::new(unsafe { Arc::from_raw(*ptr) } )
}

unsafe extern "C" fn spritelist_gc(l: &lua_State) -> i32 {
    let mut sl = unsafe { checkspritelist(l, 1) };

    if let Some(dx_lua) = get_dx_lua_upvalue(l) {
        let mut sprite_lists = dx_lua.sprite_lists.lock().unwrap();

        let mut i = 0;

        while i < sprite_lists.len() {
            if Arc::ptr_eq(&*sl, &sprite_lists[i]) {
                sprite_lists.remove(i);
                break;
            } else {
                i += 1;
            }
        }
    }


    { // in a block so we can get an immutable reference to inner now and then
      // a mutable reference when dropping below
        // sprite lists can only be created from Lua, and by the time they are garbage
        // collected Lua already knows that there are no other references to it,
        // so free the other Lua things it references here
        // TODO: is this still true now that it's a mutex?
        let inner = sl.inner.lock().unwrap();

        for tags in &inner.sprite_tags {
            for tag in tags {
                lua::L::unref(l, lua::LUA_REGISTRYINDEX, *tag);
            }
        }
    }

    unsafe { ManuallyDrop::drop(&mut sl); }

    return 0;
}

/*** RST
    .. lua:method:: add(texture, attributes)

        Add a sprite to this list. ``attributes`` must be a table that may have
        the following fields

        ========= ===================================================================
        Field     Description
        ========= ===================================================================
        x         The sprite's X coordinate in map units. Default: ``0.0``.
        y         The sprite's Y coordinate in map units. Default: ``0.0``.
        z         The sprite's Z coordinate in map units. Default: ``0.0``.
        tags      A table of attributes that can be referenced with update or
                  remove.
                  *Note:* the table is referenced, not copied.
        size      The sprite's size, in map units. Default: ``80``.
        color     Tint color and opacity, see :ref:`colors`. Default: ``0xFFFFFFFF``.
        billboard A boolean indicating if the sprite should always face the
                  camera. Default: ``true``.
        rotation  A sequence of 3 numbers, indicating the rotation to be applied
                  to the sprite along the X, Y, and Z axes, in that order. This
                  value is only applicable if ``billboard`` is false.
        fadenear  The distance in map units from the player that the sprite will
                  begin to fade to transparent. Default: ``-1.0``.
                  *Note:* negative values disable distance based fading.
        fadefar   The distance in map units from the player that the sprite will
                  become completely transparent. Default: ``-1.0``.
                  *Note:* negative values disable distance based fading.
        mousetest A boolean value indicating if the mouse position will be checked
                  each frame against the position of this sprite.
        ========= ===================================================================

        :param string texture: The name of the texture, see :lua:meth:`dxtexturemap.add`.
        :param table attributes: See above.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_add(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);
    lua::checkargtype!(l, 3, lua::LuaType::LUA_TTABLE);

    let sl = unsafe { checkspritelist(l, 1) };
    let texname = lua::tostring(l, 2).unwrap();

    let mut inner = sl.inner.lock().unwrap();

    let texture: Arc<Texture>;

    match inner.texture_map.get(&texname) {
        Some(t) => texture = t,
        None    => {
            luaerror!(l, "Texture {} not found in texture map.", texname);
            return 0;
        }
    }

    let mut ti: Option<usize> = None;
    for t in 0..inner.texture_names.len() {
        if texname == inner.texture_names[t] {
            ti = Some(t);
            break;
        }
    }

    let mut s = SpriteListSprite {
        x: 0.0,
        y: 0.0,
        z: 0.0,

        max_u: texture.max_u,
        max_v: texture.max_v,
        xy_ratio: texture.xy_ratio,

        size: 80.0,

        fade_near: -1.0,
        fade_far: -1.0,

        r: 1.0,
        g: 1.0,
        b: 1.0,
        a: 1.0,

        flags: 0x01, // billboard

        rotation: lamath::Mat4F::identity(),
    };

    let mouse_test: bool;
    if lua::getfield(l, 3, "mousetest") != lua::LuaType::LUA_TNIL {
        mouse_test = lua::toboolean(l, -1);
    } else {
        mouse_test = false;
    }
    lua::pop(l, 1);

    s.update_from_lua_table(l, 3);

    let tags_ref = if lua::getfield(l, 3, "tags")!=lua::LuaType::LUA_TNIL {
        lua::L::ref_(l, lua::LUA_REGISTRYINDEX)
    } else {
        lua::pop(l, 1);
        -1
    };

    if let Some(i) = ti {
        inner.sprite_data[i].push(s);
        inner.sprite_tags[i].push(tags_ref);
        inner.mouse_test[i].push(mouse_test);
    } else {
        inner.texture_names.push(texname.clone());
        inner.sprite_data.push(Vec::new());
        inner.sprite_tags.push(Vec::new());
        inner.mouse_test.push(Vec::new());
        inner.sprite_data.last_mut().unwrap().push(s);
        inner.sprite_tags.last_mut().unwrap().push(tags_ref);
        inner.mouse_test.last_mut().unwrap().push(mouse_test);
    }

    inner.update_vert_buffer = true;

    return 0;
}

/*** RST
    .. lua:method:: draw(value)

        Sets if this spritelist should be drawn.

        :param boolean draw:

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_draw(l: &lua_State) -> i32 {
    let sl = unsafe { checkspritelist(l, 1) };
    let val = lua::toboolean(l, 2);

    sl.inner.lock().unwrap().draw = val;

    return 0;
}

fn tags_match(l: &lua_State, target_tags: i32, query_tags: i32) -> bool {
    lua::pushnil(l);

    while lua::next(l, query_tags) != 0 {
        let tagkey = lua::tostring(l, -2).unwrap_or(String::new());

        if lua::getfield(l, target_tags, &tagkey)==lua::LuaType::LUA_TNIL || !lua::compare(l, -1, -2, lua::LUA_OPEQ) {
            // not a match
            // pop the current key, value, and the value from the sprites tag
            // table, which was nil
            lua::pop(l, 3);
            return false;
        }

        lua::pop(l, 2); // pop the sprites tag value and the input value, leaving the key
    }

    return true;
}

/*** RST
    .. lua:method:: update(tags, attributes)

        Update the sprites that have matching tags.

        An empty tags table matches all sprites. A sprite must match all tag
        values given, if a sprite does not have a value for a tag it will not
        match.

        Returns the number of sprites updated.

        :param table tags:
        :param table attributes:
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_update(l: &lua_State) -> i32 {
    lua::checkargtype!(l, 2, lua::LuaType::LUA_TTABLE);
    lua::checkargtype!(l, 3, lua::LuaType::LUA_TTABLE);
    let sl = unsafe { checkspritelist(l, 1) };

    return sl.inner.lock().unwrap().update_matching(l);
}

/*** RST
    .. lua:method:: remove(tags)

        Remove sprites that have matching tags.

        An empty tags table matches all sprites. A sprite must match all tag
        values given, if a sprite does not have a value for a tag it will not
        match.

        :param table tags:
        :returns: The number of sprites removed.
        :rtype: integer

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_remove(l: &lua_State) -> i32 {
    lua::checkargtype!(l, 2, lua::LuaType::LUA_TTABLE);

    let sl = unsafe { checkspritelist(l, 1) };

    return sl.inner.lock().unwrap().remove_matching(l);
}

/*** RST
    .. lua:method:: clear()

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_clear(l: &lua_State) -> i32 {
    let sl = unsafe { checkspritelist(l, 1) };

    let mut inner = sl.inner.lock().unwrap();

    for tags in &inner.sprite_tags {
        for tag in tags {
            lua::L::unref(l, lua::LUA_REGISTRYINDEX, *tag);
        }
    }

    inner.texture_names.clear();
    inner.sprite_data.clear();
    inner.sprite_tags.clear();

    return 0;
}

/*** RST
    .. lua:method:: mousehovertags()

        :rtype: table

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn spritelist_mouse_hover_tags(l: &lua_State) -> i32 {
    let sl = unsafe { checkspritelist(l, 1) };

    let inner = sl.inner.lock().unwrap();

    lua::createtable(l, inner.mouse_hover_tags.len() as i32, 0);

    let mut i = 1;
    for tags in &inner.mouse_hover_tags {
        if lua::rawgeti(l, lua::LUA_REGISTRYINDEX, *tags) == lua::LuaType::LUA_TTABLE {
            // only return tables. if the tags value isn't a table,
            // the sprite may have been removed between the last frame being drawn and now
            lua::seti(l, -2, i);
            i += 1;
        } else {
            lua::pop(l, 1);
        }
    }

    return 1;
}

/*** RST
.. lua:class:: dxtraillist

*/

struct TrailList {
    inner: Mutex<TrailListInner>,
}

struct TrailListInner {
    vert_buffer: Option<Direct3D12::ID3D12Resource>,
    vert_buffer_view: Direct3D12::D3D12_VERTEX_BUFFER_VIEW,

    vert_buffer_size: usize,
    update_vert_buffer: bool,

    texture_map: Arc<TextureMap>,
    texture_names: Vec<String>,

    trails: Vec<Vec<TrailListTrail>>,

    is_map: bool,
    draw: bool,
}

impl TrailListInner {
    fn update_vertex_buffer(&mut self, frame: &mut dx::SwapChainLock, dx: &Arc<dx::Dx>) {
        let mut coords: Vec<Vec<Vec<TrailCoordinate>>> = Vec::new();

        let mut new_size: usize = 0;
        for textrails in &mut self.trails {
            let mut tc: Vec<Vec<TrailCoordinate>> = Vec::new();

            for trail in textrails {
                tc.push(trail.calc_coords(self.is_map));
                new_size += trail.coord_count as usize * std::mem::size_of::<TrailCoordinate>();
            }
            coords.push(tc);
        }

        frame.flush_commands();

        if new_size == 0 {
            self.vert_buffer = None;
            self.vert_buffer_size = new_size;

            return;
        } else if self.vert_buffer_size != new_size {
            let vb = dx.new_vertex_buffer(new_size as u64);
            crate::dx::object_set_name(&vb, "EG-Overlay D3D12 TrailList Vertex Buffer");
            self.vert_buffer_size = new_size;

            self.vert_buffer_view.BufferLocation = unsafe { vb.GetGPUVirtualAddress() };
            self.vert_buffer_view.SizeInBytes = new_size as u32;
            self.vert_buffer_view.StrideInBytes = std::mem::size_of::<TrailCoordinate>() as u32;

            self.vert_buffer = Some(vb);
        }

        let upload = dx.new_upload_buffer(self.vert_buffer_size as u64);
        crate::dx::object_set_name(&upload, "EG-Overaly D3D12 TrailList Temp. Upload Buffer");

        let mut data: *mut std::ffi::c_void = std::ptr::null_mut();
        let rr = Direct3D12::D3D12_RANGE::default();

        if unsafe { upload.Map(0, Some(&rr), Some(&mut data)) }.is_err() {
            panic!("Couldn't map trail upload data.");
        }

        let mut offset = 0;
        for textrails in &coords {
            for trail in textrails {
                let tvbosize = trail.len() * std::mem::size_of::<TrailCoordinate>();

                if tvbosize == 0 { continue; }

                unsafe {
                    std::ptr::copy_nonoverlapping(trail.as_ptr() as *const std::ffi::c_void, data.add(offset), tvbosize);
                }

                offset += tvbosize;
            }
        }

        unsafe { upload.Unmap(0, None); }

        let mut copy = dx.copy_queue();
        copy.copy_resource(&upload, self.vert_buffer.as_ref().unwrap());

        self.update_vert_buffer = false;
    }

    fn remove_matching(&mut self, l: &lua_State) -> i32 {
        let mut nremoved = 0;

        for textrails in &mut self.trails {
            let mut ti = 0;
            while ti < textrails.len() {
                if textrails[ti].tags < 0 {
                    ti += 1;
                    continue;
                }

                lua::geti(l, lua::LUA_REGISTRYINDEX, textrails[ti].tags);
                let trailtags = lua::gettop(l);

                if tags_match(l, trailtags, 2) {
                    lua::L::unref(l, lua::LUA_REGISTRYINDEX, textrails[ti].tags);

                    textrails.remove(ti);
                    nremoved += 1;
                } else {
                    ti += 1;
                }
                lua::pop(l, 1);
            }
        }

        if nremoved > 0 { self.update_vert_buffer = true; }

        lua::pushinteger(l, nremoved);

        return 1;
    }
}

struct TrailListTrail {
    points: Vec<lamath::Vec3F>,

    coord_count: u32,

    fade_near: f32,
    fade_far: f32,

    color: crate::ui::Color,

    size: f32,
    wall: bool,

    tags: i64,
}

#[repr(C)]
struct TrailCoordinate {
    x: f32,
    y: f32,
    z: f32,

    u: f32,
    v: f32,
}

impl TrailListTrail {
    fn calc_coords(&mut self, map: bool) -> Vec<TrailCoordinate> {
        let mut coords: Vec<TrailCoordinate> = Vec::new();

        if self.points.len() == 0 {
            self.coord_count = 0;
            return coords;
        }

        self.coord_count = ((self.points.len() as u32 - 1) * 2) + 2;

        let up = if self.wall && !map {
            // if this is a wall, 'up' is to the east
            lamath::Vec3F { x: 1.0, y: 0.0, z: 0.0 }
        } else if map {
            // on the map up is Z
            lamath::Vec3F { x: 0.0, y: 0.0, z: 1.0 }
        } else {
            // otherwise up is Y
            lamath::Vec3F { x: 0.0, y: 1.0, z: 0.0 }
        };


        for i in 0..(self.points.len()-1) {
            // each segment of the trail is made up of 2 points: p1 and p2
            let p1 = &self.points[i];
            let p2 = &self.points[i+1];

            /*
                In order to display a flat 'ribbon' trail, we need 4 points,
                arranged around p1 and p2.

                We calculate a,b,c,d based on the points p1 and p2

                c<----p2----->d
                      ^
                      |
                      |
                      |
                      |
                      |
                a<----p1----->b

                Our 6 coords are the vertices of 2 triangles:
                 - b, a, d
                 - d, a, c

                The most efficient way of storing and drawing these triangles will
                be a triangle strip. Each segment will use 4 vertices: b, a, d, c
                which will be drawn in b, a, d and d, a, c order.

                The next segment will use d, c, e, f and so on.
            */

            // forward, from p1 to p2
            let forward = (*p2 - *p1).normalize();

            // side is a vector perpendicular to forward and up
            let mut side = up.crossproduct(&forward).normalize();

            // toside is our vector to b and d
            // and the opposite direction is to a and c
            let mut toside = side.mulf(self.size / 2.0);

            // if this is the first segment then calculate a and b, otherwise
            // c and d from the previous segment will become a and b
            if i==0 {
                // b
                coords.push(TrailCoordinate {
                    x: p1.x + toside.x,
                    y: p1.y + toside.y,
                    z: p1.z + toside.z,
                    u: 1.0,
                    v: 0.0,
                });

                // a
                coords.push(TrailCoordinate {
                    x: p1.x - toside.x,
                    y: p1.y - toside.y,
                    z: p1.z - toside.z,
                    u: 0.0,
                    v: 0.0,
                });
            } else {
                // adjust side and toside to be the mean of the prior side vector
                // and the current one
                let prior_forward = (self.points[i] - self.points[i-1]).normalize();
                let prior_side = up.crossproduct(&prior_forward).normalize();

                side = lamath::Vec3F {
                    x: (prior_side.x + side.x) / 2.0,
                    y: (prior_side.y + side.y) / 2.0,
                    z: (prior_side.z + side.z) / 2.0,
                };

                toside = side.mulf(self.size / 2.0);

                let l = coords.len();

                let m1 = &mut coords[l-1];
                m1.x = p1.x - toside.x;
                m1.y = p1.y - toside.y;
                m1.z = p1.z - toside.z;


                let m2 = &mut coords[l-2];
                m2.x = p1.x + toside.x;
                m2.y = p1.y + toside.y;
                m2.z = p1.z + toside.z;

                // TODO: adjust the v coordinates too
            }

            let mut section_len = (*p2 - *p1).length();

            // If the segment is too long fading won't be calculated properly
            // so insert extra points along forward.
            // I'm not exactly sure what's going on, but I suspect it's something to
            // do with how the z/depth isn't linear and the calculated fade distance
            // isn't being interpolated properly when the end vertex is beyond a
            // certain Z distance away. In any case, breaking long segments up into
            // multiple smaller ones makes fading with fade_near/fade_far work much
            // better on very long segments.
            if section_len > 5000.0 && !map {
                let extrapoints: u32 = unsafe { (section_len / 5000.0).to_int_unchecked() };

                self.coord_count += extrapoints * 2;

                for ep in 0..extrapoints {
                    let len = 5000.0 * (ep as f32+ 1.0);

                    // vector from p1 to this extra point
                    let fp = lamath::Vec3F {
                        x: forward.x * len,
                        y: forward.y * len,
                        z: forward.z * len,
                    };

                    let p = *p1 + fp;

                    let epv = - (5000.0 / self.size) + coords.last().unwrap().v;

                    coords.push(TrailCoordinate {
                        x: p.x + toside.x,
                        y: p.y + toside.y,
                        z: p.z + toside.z,
                        u: 1.0,
                        v: epv,
                    });

                    coords.push(TrailCoordinate {
                        x: p.x - toside.x,
                        y: p.y - toside.y,
                        z: p.z - toside.z,
                        u: 0.0,
                        v: epv,
                    });

                    section_len -= 5000.0;
                }
            }

            let section_frac = section_len / self.size;

            let p2v = -section_frac + coords.last().unwrap().v;

            // d
            coords.push(TrailCoordinate {
                x: p2.x + toside.x,
                y: p2.y + toside.y,
                z: p2.z + toside.z,
                u: 1.0,
                v: p2v,
            });

            // c
            coords.push(TrailCoordinate {
                x: p2.x - toside.x,
                y: p2.y - toside.y,
                z: p2.z - toside.z,
                u: 0.0,
                v: p2v,
            });
        }

        return coords;
    }

    fn update_from_lua_table(&mut self, l: &lua_State, table: i32) -> bool {
        let mut update_vert_buffer = false;

        if lua::getfield(l, table, "fadenear") != lua::LuaType::LUA_TNIL { self.fade_near = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "fadefar") != lua::LuaType::LUA_TNIL { self.fade_far = lua::tonumber(l, -1) as f32; }
        lua::pop(l, 1);

        if lua::getfield(l, table, "color") != lua::LuaType::LUA_TNIL { self.color = ui::Color::from(lua::tonumber(l, -1) as u32); }
        lua::pop(l, 1);

        if lua::getfield(l, table, "size") != lua::LuaType::LUA_TNIL {
            self.size = lua::tonumber(l, -1) as f32;
            update_vert_buffer = true;
        }
        lua::pop(l, 1);

        if lua::getfield(l, table, "wall") != lua::LuaType::LUA_TNIL {
            self.wall = lua::toboolean(l, -1);
            update_vert_buffer = true;
        }
        lua::pop(l, 1);

        if lua::getfield(l, table, "points") != lua::LuaType::LUA_TNIL {
            let points = lua::gettop(l);
            let c = lua::L::len(l, points);

            if c < 2 {
                luaerror!(l, "trails must have at least 2 points.");

                return update_vert_buffer;
            }

            self.points.clear();

            for i in 1..(c+1) {
                lua::geti(l, points, i as i64); // sequence of x,y,z

                let p = lua::gettop(l);
                lua::geti(l, p, 1);
                lua::geti(l, p, 2);
                lua::geti(l, p, 3);

                let v = lamath::Vec3F {
                    x: lua::tonumber(l, -3) as f32,
                    y: lua::tonumber(l, -2) as f32,
                    z: lua::tonumber(l, -1) as f32,
                };

                lua::pop(l, 4);

                self.points.push(v);
            }

            update_vert_buffer = true;
        }
        lua::pop(l, 1);

        return update_vert_buffer;
    }
}

const TRAILLIST_METATABLE_NAME: &str = "dx::lua::TrailList";

const TRAILLIST_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"  , traillist_gc,
    c"draw"  , traillist_draw,
    c"add"   , traillist_add,
    c"remove", traillist_remove,
    c"clear" , traillist_clear,
};

unsafe fn checktraillist(l: &lua_State, ind: i32) -> ManuallyDrop<Arc<TrailList>> {
    let ptr: *mut *const TrailList = unsafe {
        std::mem::transmute(lua::L::checkudata(l, ind, TRAILLIST_METATABLE_NAME))
    };

    ManuallyDrop::new(unsafe { Arc::from_raw(*ptr) } )
}

unsafe extern "C" fn traillist_gc(l: &lua_State) -> i32 {
    let mut tl = unsafe { checktraillist(l, 1) };

    if let Some(dx_lua) = get_dx_lua_upvalue(l) {
        let mut trail_lists = dx_lua.trail_lists.lock().unwrap();

        let mut i = 0;

        while i < trail_lists.len() {
            if Arc::ptr_eq(&*tl, &trail_lists[i]) {
                trail_lists.remove(i);
                break;
            } else {
                i += 1;
            }
        }
    }


    { // in a block so we can get an immutable reference to inner now and then
      // a mutable reference when dropping below
        // sprite lists can only be created from Lua, and by the time they are garbage
        // collected Lua already knows that there are no other references to it,
        // so free the other Lua things it references here
        let inner = tl.inner.lock().unwrap();

        for tex_trails in &inner.trails {
            for trail in tex_trails {
                if trail.tags > 0 {
                    lua::L::unref(l, lua::LUA_REGISTRYINDEX, trail.tags);
                }
            }
        }
    }

    unsafe { ManuallyDrop::drop(&mut tl); }

    return 0;
}

/*** RST
    .. lua:method:: draw(value)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn traillist_draw(l: &lua_State) -> i32 {
    let tl = unsafe { checktraillist(l, 1) };
    let val = lua::toboolean(l, 2);

    tl.inner.lock().unwrap().draw = val;

    return 0;
}

/*** RST
    .. lua:method:: add(texturename, attributes)

        Create a new trail.

        ``attributes`` must be a table with the following fields:

        ======== ===============================================================
        Field    Description
        ======== ===============================================================
        points   A sequence of sequences, trail points. ie. { {1,1,1}, {2,2,2} }
        tags     A table of attributes that can be used other methods of this
                 list to update or remove trails with matching tags.
                 *Note:* the table is referenced directly, not copied.
        fadenear A number that indicates how far away from the player a trail
                 begins to fade to transparent.
        fadefar  A number that indicates how far away from the player a trail
                 will become completely transparent.
        ======== ===============================================================

        :param string texturename: The name of a texture in the texture list
            this trail list references.
        :param table attributes: See above.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn traillist_add(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);
    lua::checkargtype!(l, 3, lua::LuaType::LUA_TTABLE);
    let tl = unsafe { checktraillist(l, 1) };
    let texname = lua::tostring(l, 2).unwrap();

    if lua::getfield(l, 3, "points")!=lua::LuaType::LUA_TTABLE {
        lua::pop(l, 1);
        luaerror!(l, "points must be a table.");
        return 0;
    } else {
        lua::pop(l, 1);
    }

    let mut inner = tl.inner.lock().unwrap();

    //let texture: Rc<Texture>;

    match inner.texture_map.get(&texname) {
        Some(_) => { },
        None    => {
            luaerror!(l, "Texture {} not found in texture map.", texname);
            return 0;
        }
    }

    let mut ti: Option<usize> = None;
    for t in 0..inner.texture_names.len() {
        if texname == inner.texture_names[t] {
            ti = Some(t);
            break;
        }
    }

    let mut t = TrailListTrail {
        points: Vec::new(),

        coord_count: 0,

        fade_near: -1.0,
        fade_far: -1.0,

        color: crate::ui::Color::from(0xFFFFFFFFu32),

        size: 40.0,
        wall: false,
        tags: -1,
    };

    if lua::getfield(l, 3, "tags")!=lua::LuaType::LUA_TNIL {
        t.tags = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);
    } else {
        lua::pop(l, 1);
    }

    t.update_from_lua_table(l, 3);

    if let Some(i) = ti {
        inner.trails[i].push(t);
    } else {
        inner.texture_names.push(texname.clone());
        inner.trails.push(Vec::new());
        inner.trails.last_mut().unwrap().push(t);
    }

    inner.update_vert_buffer = true;

    return 0;
}

/*** RST
    .. lua:method:: remove(tags)

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn traillist_remove(l: &lua_State) -> i32 {
    lua::checkargtype!(l, 2, lua::LuaType::LUA_TTABLE);
    let tl = unsafe { checktraillist(l, 1) };

    return tl.inner.lock().unwrap().remove_matching(l);
}

/*** RST
    .. lua:method:: clear()

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn traillist_clear(l: &lua_State) -> i32 {
    let tl = unsafe { checktraillist(l, 1) };

    let mut inner = tl.inner.lock().unwrap();

    for tex_trails in &inner.trails {
        for trail in tex_trails {
            if trail.tags > 0 {
                lua::L::unref(l, lua::LUA_REGISTRYINDEX, trail.tags);
            }
        }
    }

    inner.texture_names.clear();
    inner.trails.clear();

    return 0;
}
