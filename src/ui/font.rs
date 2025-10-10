// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Font Managment
//!
//! Glyphs are pre-rendered into textures and then each glyph is drawn as
//! a textured quad each frame. Unlike other rendered GUIs, the font textures are
//! not baked up front. Each glyph is lazy loaded (rendered) as needed, with a
//! predefined list of common glyphs loaded at initialization (preload_chars)
//!
//! The textures that hold the glyphs are GLYPH_TEX_SIZE (512x512). This means
//! that larger font sizes may need multiple textures to hold glyphs, especially
//! if additional glyphs outside the preload_chars are needed.
//! This does mean that storage for particularly huge fonts becomes pretty
//! inefficient (ie. 1 texture per glyph for anything over 256 pixels), but I
//! expect that users won't be using fonts with a size over 30 pixels, much less
//! 256.
//!
//! Each texture is stored as a layer in a 2D texture array. All glyphs are for
//! a font are stored in a hash map (of the codepoint). The order the glyphs are
//! rendered is the order in which they will appear in the texture array.

pub mod lua;

use std::sync::Arc;
use std::sync::Mutex;

use std::collections::HashMap;

use crate::overlay;
use crate::ui;
use crate::dx;

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};
use crate::ft;

use windows::Win32::Graphics::Direct3D;
use windows::Win32::Graphics::Direct3D12;
use windows::Win32::Graphics::Dxgi;

/// Characters that are pre-rendered every time a font is loaded.
const PRELOAD_CHARS: &str = concat!(
    " ~!@#$%^&*()_+`1234567890,.;:'\"-=\\|/?><[]{}",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
);

const VERT_CSO: &str = "shaders/font-quad.vs.cso";
const PIXEL_CSO: &str = "shaders/font-quad.ps.cso";


/// The FontManager manages fonts. (haha)
///
/// This is the global state for all fonts, holds all cached fonts and contains
/// the rendering assets. All things that render fonts in the overlay will
/// interact with the FontManager to do so.
pub struct FontManager {
    ft: ft::Library,
    font_cache: Mutex<HashMap<FontKey, Arc<Font>>>,

    pso: Direct3D12::ID3D12PipelineState,
}

/// A unique way of identifying a particular font.
///
/// This is the path to the font file itself, plus any variable axis values
/// that are set.
#[derive(Hash,Clone,Eq,PartialEq)]
struct FontKey {
    path: String,
    size: u32,
    axis_coords: Vec<(String,i32)>,
}

impl std::fmt::Display for FontKey {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut axes: Vec<String> = Vec::new();

        for (nm, val) in &self.axis_coords {
            axes.push(format!("{} = {}", nm, val));
        }


        write!(f, "[{} ({})]", self.path, axes.join(", "))
    }
}

/// Cached metrics for each rendered glyph
struct GlyphMetrics {
    bearing_x: f64,
    bearing_y: f64,
    advance_x: f64,
    bitmap_width: u32,
    bitmap_height: u32,
    char_index: u32,
}

const GLYPH_TEX_SIZE: u64 = 512;

/// Information for each glyph rendered into the texture
struct FontGlyphInfo {
    // where in the texture is it?
    texture_x: u32,
    texture_y: u32,

    metrics: GlyphMetrics,

    texture_num: u16, // which layer in the array is this glyph on?
}

impl FontManager {
    /// Initializes the FontManager.
    pub fn new() -> FontManager {
        debug!("init");

        let lib = ft::Library::new().expect("Couldn't initialize FreeType2.");

        debug!("Loading vertex shader from {}...", VERT_CSO);
        let vertcso = std::fs::read(VERT_CSO).expect(format!("Couldn't read {}", VERT_CSO).as_str());

        debug!("Loading pixel shader from {}...", PIXEL_CSO);
        let pixelcso = std::fs::read(PIXEL_CSO).expect(format!("Couldn't read {}", PIXEL_CSO).as_str());

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

        let pso = dx.create_pipeline_state(&mut psodesc, "EG-Overlay D3D12 ui.font Pipeline State")
            .expect("Couldn't create font pipeline state.");


        let fm = FontManager {
            ft: lib,
            font_cache: Mutex::new(HashMap::new()),
            pso: pso,
        };

        return fm;
    }

    /// Returns a font for the given path and axis_coords.
    ///
    /// If the font hasn't been loaded yet, it will be loaded first and then returned.
    /// `path` should be a valid path to a FreeType2 supported font file.
    pub fn get_font(&self, path: &str, size: u32, axis_coords: &Vec<(String, i32)>) -> Arc<Font> {
        let key = FontKey {
            path: String::from(path),
            size: size,
            axis_coords: axis_coords.clone(),
        };

        if self.font_cache.lock().unwrap().contains_key(&key) {
            return self.font_cache.lock().unwrap().get(&key).unwrap().clone();
        }

        let f = Arc::new(self.new_font(path, size, axis_coords));

        self.font_cache.lock().unwrap().insert(key, f.clone());

        return f;
    }

    /// Returns a font based on the given font, but with a different size
    pub fn get_font_from_font_with_size(&self, base_font: &Arc<Font>, new_size: u32) -> Arc<Font> {
        let mut new_key = base_font.key.clone();

        new_key.size = new_size;

        if self.font_cache.lock().unwrap().contains_key(&new_key) {
            return self.font_cache.lock().unwrap().get(&new_key).unwrap().clone();
        }

        let f = Arc::new(self.new_font(&new_key.path, new_key.size, &new_key.axis_coords));

        self.font_cache.lock().unwrap().insert(new_key, f.clone());

        return f;
    }

    pub fn get_font_from_font_with_size_perc(&self, base_font: &Arc<Font>, new_size_perc: f64) -> Arc<Font> {
        let new_size = (base_font.key.size as f64 * new_size_perc).floor() as u32;

        return self.get_font_from_font_with_size(base_font, new_size);
    }

    fn new_font(&self, path: &str, size: u32, axis_coords: &Vec<(String, i32)>) -> Font {
        let face = self.ft.new_face(path).expect(format!("Couldn't load {}", path).as_str());

        for (axis, val) in axis_coords {
            face.set_axis_coord(axis, *val);
        }
        face.set_pixel_sizes(0, size);

        let max_w = face.max_glyph_width_pixels();
        let max_h = face.max_glyph_height_pixels();

        let glyph_width: u32;
        if max_w > max_h { glyph_width = max_w as u32; }
        else             { glyph_width = max_h as u32; }

        let page_glyph_x = GLYPH_TEX_SIZE / glyph_width as u64;
        let page_max_glyphs = page_glyph_x.pow(2) as u64;

        debug!("New font, {} size {} ({}), {} glyphs per texture page.", path, size, glyph_width, page_max_glyphs);

        let tex = crate::overlay::dx().new_texture_2d_array(
            Dxgi::Common::DXGI_FORMAT_R8_UNORM,
            GLYPH_TEX_SIZE as u32,
            GLYPH_TEX_SIZE as u32,
            1,
            1
        );
        tex.set_name(format!("EG-Overlay D3D12 Font Texture: {}|{}", path, size).as_str());

        let key = FontKey {
            path: String::from(path),
            size: size,
            axis_coords: axis_coords.clone(),
        };

        let f = Font {
            key: key,
            has_kerning: face.has_kerning(),
            face: face,

            glyph_width: glyph_width,
            page_max_glyphs: page_max_glyphs,
            page_glyph_x: page_glyph_x,

            data: Mutex::new( FontMutData {
                glyph_count: 0,
                glyphs: HashMap::new(),
                texture: tex,
                texture_levels: 1u16,
                kerning_data: HashMap::new(),
            }),

            pso: self.pso.clone(),
        };

        for c in PRELOAD_CHARS.chars() {
            f.render_glyph(c);
        }

        f
    }
}

impl Drop for FontManager {
    fn drop(&mut self) {
        debug!("cleanup");
        self.font_cache.lock().unwrap().clear();
    }
}

/// A specific font. A Font will exist for every combination of font file and
/// variable axis values used.
pub struct Font {
    key: FontKey,
    face: ft::Face,
    has_kerning: bool,

    glyph_width: u32,
    // the number of glyphs that can fit on a single layer of the texture array
    page_max_glyphs: u64,
    // the number of glyphs per row in the texture layer
    page_glyph_x: u64,

    data: Mutex<FontMutData>,

    pso: Direct3D12::ID3D12PipelineState,
}

struct FontMutData {
    glyph_count: u64, // the number of glyphs already rendered into the texture
    glyphs: HashMap<u32, FontGlyphInfo>,
    texture: crate::dx::Texture,
    texture_levels: u16,

    // cached kerning data. key is two codepoints, corresponding to 2
    // adjacent characters to perform kerning on. value is x,y offsets
    kerning_data: HashMap<(u32,u32), (f32,f32)>,
}

impl Font {
    // Render a glyph to the underlying texture
    // the glyph here is a UTF 32bit codepoint
    fn render_glyph(&self, glyph: char) {
        let glyph_ind = self.face.get_char_index(glyph as u32);

        //if glyph_ind == 0 {
        //    warn!("No glyph for 0x{:x}", glyph as u32);
        //}

        if let Err(_r) = self.face.load_glyph(glyph_ind, ft::FT_LOAD_DEFAULT) {
            error!("Couldn't load glyph for {:x}", glyph as u32);
            return;
        }

        if let Err(_r) = self.face.render_glyph(ft::FT_Render_Mode::FT_RENDER_MODE_NORMAL) {
            error!("Couldn't render glyph for {:x}", glyph as u32);
            return;
        }

        let glyph_metrics = unsafe { &(*self.face.glyph()).metrics };
        let bitmap = unsafe { &(*self.face.glyph()).bitmap };

        // cache glyph metrics for performance
        let metrics = GlyphMetrics {
            bearing_x: glyph_metrics.horiBearingX as f64 / 64.0,
            bearing_y: glyph_metrics.horiBearingY as f64 / 64.0,
            advance_x: glyph_metrics.horiAdvance  as f64 / 64.0,
            bitmap_width: bitmap.width,
            bitmap_height: bitmap.rows,
            char_index: glyph_ind,
        };

        let mut font_data = self.data.lock().unwrap();

        let glyph_index = font_data.glyph_count;

        font_data.glyph_count += 1;

        if font_data.glyph_count > self.page_max_glyphs * (font_data.texture_levels as u64) {
            // this glyph will spill over onto a new layer in the texture

            let new_texture = crate::overlay::dx().new_texture_2d_array(
                Dxgi::Common::DXGI_FORMAT_R8_UNORM,
                GLYPH_TEX_SIZE as u32,
                GLYPH_TEX_SIZE as u32,
                font_data.texture_levels + 1,
                1
            );
            new_texture.set_name(format!("EG-Overlay D3D12 Font Texture: {}|{}", self.key.path, self.key.size).as_str());

            new_texture.copy_subresources_from(&font_data.texture, font_data.texture_levels as u32);
            font_data.texture_levels += 1;
            font_data.texture = new_texture;
        }

        let texture_num = font_data.texture_levels - 1;

        let glyph_info = FontGlyphInfo {
            metrics: metrics,
            texture_x: (glyph_index % self.page_glyph_x) as u32 * self.glyph_width,
            texture_y: ((glyph_index % self.page_max_glyphs) as u32 / self.page_glyph_x as u32) * self.glyph_width,
            texture_num: texture_num,
        };

        font_data.glyphs.insert(glyph as u32, glyph_info);

        if bitmap.width == 0 || bitmap.rows == 0 { return; }

        let pixels_size = bitmap.width * bitmap.rows;
        let mut pixels = vec![0u8; pixels_size as usize];

        let osettings = crate::overlay::settings();
        let gamma = osettings.get_f64("overlay.ui.font.gammaCorrection").unwrap();

        // FreeType2 gives us uncorrected alpha values, so gamma correct them
        // so that they look better when used as premultiplied alpha values
        for gy in 0..bitmap.rows {
            for gx in 0..bitmap.width {
                let goffset = (gy * bitmap.width) + gx;
                // gamma correction; first scale to 0..1
                let a = unsafe { *bitmap.buffer.add(goffset as usize) as f64 } / 255.0;
                let ca = a.powf(1.0/gamma);
                // then scale it back to 0..255
                pixels[goffset as usize] = (ca * 255.0).ceil() as u8;
            }
        }

        let tex_x: u32 = (glyph_index % self.page_glyph_x) as u32 * self.glyph_width;
        let tex_y: u32 = ((glyph_index % self.page_max_glyphs) as u32 / self.page_glyph_x as u32) * self.glyph_width;

        font_data.texture.write_pixels(
            tex_x,
            tex_y,
            texture_num as u32,
            bitmap.width,
            bitmap.rows,
            Dxgi::Common::DXGI_FORMAT_R8_UNORM,
            &pixels
        );
    }

    /// Renders text with this font at the given location.
    ///
    /// `swapchain` should be obtained from [Dx::start_frame](crate::dx::Dx::start_frame).
    pub fn render_text(
        &self,
        swapchain: &mut dx::SwapChainLock,
        x: i64,
        y: i64,
        text: &str,
        color: ui::Color
    ) {
        let mut data = self.data.lock().unwrap();

        swapchain.set_pipeline_state(&self.pso);
        swapchain.set_texture(0, &data.texture);

        swapchain.set_root_constant_color(color, 0, 12);
        swapchain.set_root_constant_ortho_proj  (0, 16);

        swapchain.set_primitive_topology(Direct3D::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        let mut glyph: u32;
        let mut prev_glyph: u32 = 0;

        let mut penx = x as f32;

        let font_size = unsafe { &*self.face.size() };

        for c in text.chars() {
            let codepoint = c as u32;

            let mut g = data.glyphs.get(&codepoint);

            if g.is_none() {
                drop(data); // unlock the mutex to render_glyph can lock it
                swapchain.flush_all();
                self.render_glyph(c);
                data = self.data.lock().unwrap(); // lock is again
                g = data.glyphs.get(&codepoint);
            }

            glyph = g.unwrap().metrics.char_index;

            /*
            if !data.glyphs.contains_key(&codepoint) {
                drop(data); // unlock the mutex so render_glyph can lock it
                self.render_glyph(c);
                data = self.data.lock().unwrap(); // relock it again
            }

            glyph = data.glyphs.get(&codepoint).unwrap().metrics.char_index;
            */

            let kern_x: f32;
            let kern_y: f32;

            if glyph > 0 && prev_glyph > 0 && self.has_kerning {
                let kern_key = (prev_glyph, glyph);

                if !data.kerning_data.contains_key(&kern_key) {
                    let (ix, iy) = self.face.get_kerning(prev_glyph, glyph);
                    data.kerning_data.insert((prev_glyph, glyph), (ix as f32 / 64.0f32, iy as f32 / 64.0f32));
                    kern_x = ix as f32 / 64.0f32;
                    kern_y = iy as f32 / 64.0f32;
                } else {
                    (kern_x, kern_y) = data.kerning_data.get(&kern_key).unwrap().clone();
                }
            } else {
                kern_x = 0.0;
                kern_y = 0.0;
            }

            penx += kern_x;

            let glyph_info = data.glyphs.get(&codepoint).unwrap();

            if glyph_info.metrics.bitmap_width == 0 {
                penx += glyph_info.metrics.advance_x as f32;
                continue;
            }

            let left   = penx + (glyph_info.metrics.bearing_x as f32);
            let right  = left + (glyph_info.metrics.bitmap_width as f32);
            let top    = kern_y + (y as f32) + (font_size.metrics.ascender as f32 / 64.0f32) - (glyph_info.metrics.bearing_y as f32);
            let bottom = top + (glyph_info.metrics.bitmap_height as f32);

            let tex_left   = glyph_info.texture_x as f32;
            let tex_right  = tex_left + glyph_info.metrics.bitmap_width as f32;
            let tex_top    = glyph_info.texture_y as f32;
            let tex_bottom = tex_top + glyph_info.metrics.bitmap_height as f32;
            let tex_layer  = glyph_info.texture_num as f32;

            swapchain.set_root_constant_float(left      , 0, 0);
            swapchain.set_root_constant_float(top       , 0, 1);
            swapchain.set_root_constant_float(right     , 0, 2);
            swapchain.set_root_constant_float(bottom    , 0, 3);
            swapchain.set_root_constant_float(tex_left  , 0, 4);
            swapchain.set_root_constant_float(tex_top   , 0, 5);
            swapchain.set_root_constant_float(tex_right , 0 ,6);
            swapchain.set_root_constant_float(tex_bottom, 0 ,7);
            swapchain.set_root_constant_float(tex_layer , 0, 8);

            swapchain.draw_instanced(4, 1, 0, 0);

            penx += glyph_info.metrics.advance_x as f32;
            prev_glyph = glyph;
        }
    }

    pub fn get_text_width(&self, text: &str) -> u64 {
        let mut data = self.data.lock().unwrap();

        let mut glyph: u32;
        let mut prev_glyph: u32 = 0;

        let mut penx = 0;

        for c in text.chars() {
            let codepoint = c as u32;
            if !data.glyphs.contains_key(&codepoint) {
                drop(data); // unlock the mutex so render_glyph can lock it

                // render the new glyph between frames
                let dx = crate::overlay::dx();
                let mut swapchain = dx.swapchain();
                swapchain.flush_all();
                self.render_glyph(c);
                drop(swapchain);

                data = self.data.lock().unwrap(); // relock it again
            }

            glyph = data.glyphs.get(&codepoint).unwrap().metrics.char_index;

            let kern_x: f32;
            let _kern_y: f32;

            if glyph > 0 && prev_glyph > 0 && self.has_kerning {
                let kern_key = (prev_glyph, glyph);

                if !data.kerning_data.contains_key(&kern_key) {
                    let (ix, iy) = self.face.get_kerning(prev_glyph, glyph);
                    data.kerning_data.insert((prev_glyph, glyph), (ix as f32 / 64.0f32, iy as f32 / 64.0f32));
                    kern_x = ix as f32 / 64.0f32;
                    _kern_y = iy as f32 / 64.0f32;
                } else {
                    (kern_x, _kern_y) = data.kerning_data.get(&kern_key).unwrap().clone();
                }
            } else {
                kern_x = 0.0;
                _kern_y = 0.0;
            }

            penx += kern_x as u64;

            let glyph_info = data.glyphs.get(&codepoint).unwrap();

            if glyph_info.metrics.bitmap_width == 0 {
                penx += glyph_info.metrics.advance_x as u64;
                continue;
            }

            penx += glyph_info.metrics.advance_x as u64;
            prev_glyph = glyph;
        }

        penx
    }

    pub fn get_line_spacing(&self) -> u64 {
        return unsafe { ((*self.face.size()).metrics.height as f32 / 64.0) as u64 };
    }
}

