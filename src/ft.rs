// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! FreeType 2 binding
//!
//! This is a rather low level and primitive binding to FreeType2.
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use std::ffi::{c_short, c_ushort, c_long, c_ulong, c_int, c_uint, c_char, c_uchar, c_void};
use std::ffi::{CString, CStr};

pub const FT_LOAD_DEFAULT                     :i32 = 0x0;
pub const FT_LOAD_NO_SCALE                    :i32 = 1i32 << 0;
pub const FT_LOAD_NO_HINTING                  :i32 = 1i32 << 1;
pub const FT_LOAD_RENDER                      :i32 = 1i32 << 2;
pub const FT_LOAD_NO_BITMAP                   :i32 = 1i32 << 3;
pub const FT_LOAD_VERTICAL_LAYOUT             :i32 = 1i32 << 4;
pub const FT_LOAD_FORCE_AUTOHINT              :i32 = 1i32 << 5;
pub const FT_LOAD_CROP_BITMAP                 :i32 = 1i32 << 6;
pub const FT_LOAD_PEDANTIC                    :i32 = 1i32 << 7;
pub const FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH :i32 = 1i32 << 9;
pub const FT_LOAD_NO_RECURSE                  :i32 = 1i32 << 10;
pub const FT_LOAD_IGNORE_TRANSFORM            :i32 = 1i32 << 11;
pub const FT_LOAD_MONOCHROME                  :i32 = 1i32 << 12;
pub const FT_LOAD_LINEAR_DESIGN               :i32 = 1i32 << 13;
pub const FT_LOAD_SBITS_ONLY                  :i32 = 1i32 << 14;
pub const FT_LOAD_NO_AUTOHINT                 :i32 = 1i32 << 15;
  /* Bits 16-19 are used by `FT_LOAD_TARGET_` */
pub const FT_LOAD_COLOR                       :i32 = 1i32 << 20;
pub const FT_LOAD_COMPUTE_METRICS             :i32 = 1i32 << 21;
pub const FT_LOAD_BITMAP_METRICS_ONLY         :i32 = 1i32 << 22;
pub const FT_LOAD_NO_SVG                      :i32 = 1i32 << 24;

pub const FT_FACE_FLAG_SCALABLE        :i32 = 1i32 <<  0;
pub const FT_FACE_FLAG_FIXED_SIZES     :i32 = 1i32 <<  1;
pub const FT_FACE_FLAG_FIXED_WIDTH     :i32 = 1i32 <<  2;
pub const FT_FACE_FLAG_SFNT            :i32 = 1i32 <<  3;
pub const FT_FACE_FLAG_HORIZONTAL      :i32 = 1i32 <<  4;
pub const FT_FACE_FLAG_VERTICAL        :i32 = 1i32 <<  5;
pub const FT_FACE_FLAG_KERNING         :i32 = 1i32 <<  6;
pub const FT_FACE_FLAG_FAST_GLYPHS     :i32 = 1i32 <<  7;
pub const FT_FACE_FLAG_MULTIPLE_MASTERS:i32 = 1i32 <<  8;
pub const FT_FACE_FLAG_GLYPH_NAMES     :i32 = 1i32 <<  9;
pub const FT_FACE_FLAG_EXTERNAL_STREAM :i32 = 1i32 << 10;
pub const FT_FACE_FLAG_HINTER          :i32 = 1i32 << 11;
pub const FT_FACE_FLAG_CID_KEYED       :i32 = 1i32 << 12;
pub const FT_FACE_FLAG_TRICKY          :i32 = 1i32 << 13;
pub const FT_FACE_FLAG_COLOR           :i32 = 1i32 << 14;
pub const FT_FACE_FLAG_VARIATION       :i32 = 1i32 << 15;
pub const FT_FACE_FLAG_SVG             :i32 = 1i32 << 16;
pub const FT_FACE_FLAG_SBIX            :i32 = 1i32 << 17;
pub const FT_FACE_FLAG_SBIX_OVERLAY    :i32 = 1i32 << 18;

type FT_Short = c_short;
type FT_UShort = c_ushort;
type FT_Int = c_int;
type FT_UInt = c_uint;
type FT_Pos = c_long;
type FT_Long = c_long;
type FT_ULong = c_ulong;
type FT_Fixed = c_long;
type FT_Error = c_int;

#[repr(C)]
pub struct FT_PTR {
    _foo: [u8;0],
}

#[repr(C)]
pub struct FT_SubGlyphRec {
    _foo: [u8;0],
}

#[repr(C)]
pub struct FT_Bitmap_Size {
    pub width: FT_Short,
    pub height: FT_Short,

    pub size: FT_Pos,

    pub y_ppem: FT_Pos,
    pub x_ppem: FT_Pos,
}

#[repr(C)]
pub struct FT_CharMapRec {
    pub face: *const FT_FaceRec,
    pub encoding: FT_Encoding,
    pub platform_id: FT_UShort,
    pub encoding_id: FT_UShort,
}

#[repr(C)]
pub struct FT_Generic {
    pub data: *const std::ffi::c_void,
    pub finalizer: *const extern "C" fn(*const std::ffi::c_void),
}

#[repr(C)]
pub struct FT_BBox {
    pub xMin: FT_Pos,
    pub yMin: FT_Pos,
    pub xMax: FT_Pos,
    pub yMax: FT_Pos,
}

#[repr(C)]
pub struct FT_GlyphSlotRec {
    pub library: *const FT_LibraryRec,
    pub face: *const FT_FaceRec,
    pub next: *const FT_GlyphSlotRec,
    pub glyph_index: FT_UInt,
    pub generic: FT_Generic,

    pub metrics: FT_Glyph_Metrics,
    pub linearHoriAdvance: FT_Fixed,
    pub linearVertAdvance: FT_Fixed,
    pub advance: FT_Vector,

    pub format: FT_Glyph_Format,

    pub bitmap: FT_Bitmap,
    pub bitmap_left: FT_Int,

    pub outline: FT_Outline,
    pub num_subglyphs: FT_UInt,
    pub subglyphs: *const FT_SubGlyphRec,

    pub control_data: *const FT_PTR,
    pub control_len: c_long,

    pub lsb_delta: FT_Pos,
    pub rsb_delta: FT_Pos,

    pub other: *const FT_PTR,

    internal: *const FT_PTR,
}

#[repr(C)]
pub enum FT_Glyph_Format {
    FT_GLYPH_FORMAT_NONE      = 0,
    FT_GLYPH_FORMAT_COMPOSITE = ('c' as isize) << 24 | ('o' as isize) << 16 | ('m' as isize) << 8 | ('p' as isize),
    FT_GLYPH_FORMAT_BITMAP    = ('b' as isize) << 24 | ('i' as isize) << 16 | ('t' as isize) << 8 | ('s' as isize),
    FT_GLYPH_FORMAT_OUTLINE   = ('o' as isize) << 24 | ('u' as isize) << 16 | ('t' as isize) << 8 | ('l' as isize),
    FT_GLYPH_FORMAT_PLOTTER   = ('p' as isize) << 24 | ('l' as isize) << 16 | ('o' as isize) << 8 | ('t' as isize),
    FT_GLYPH_FORMAT_SVG       = ('S' as isize) << 24 | ('V' as isize) << 16 | ('V' as isize) << 8 | (' ' as isize),
}

#[repr(C)]
pub struct FT_Bitmap {
    pub rows: c_uint,
    pub width: c_uint,
    pub pitch: c_int,
    pub buffer: *const c_uchar,
    pub num_grays: c_ushort,
    pub pixel_mode: c_uchar,
    pub pallette_mode: c_uchar,
    pub pallet: *const FT_PTR,
}

#[repr(C)]
pub struct FT_Outline {
    pub n_contours: c_ushort,
    pub n_points: c_ushort,

    pub points: *const FT_Vector,
    pub tags: *const c_uchar,
    pub contours: *const c_ushort,

    pub flags: c_int,
}

#[repr(C)]
pub struct FT_SizeRec {
    pub face: *const FT_FaceRec,
    pub generic: FT_Generic,
    pub metrics: FT_Size_Metrics,
    pub internal: *const FT_PTR,
}

#[repr(C)]
pub struct FT_Size_Metrics {
    pub x_ppem: FT_UShort,
    pub y_ppem: FT_UShort,

    pub x_scale: FT_Fixed,
    pub y_scale: FT_Fixed,

    pub ascender: FT_Pos,
    pub descender: FT_Pos,
    pub height: FT_Pos,
    pub max_advance: FT_Pos,
}

#[repr(C)]
pub struct FT_Vector {
    pub x: FT_Pos,
    pub y: FT_Pos,
}

#[repr(C)]
pub struct FT_DriverRec {
    _foo: [u8;0],
}

#[repr(C)]
pub struct FT_MemoryRec {
    pub user: *const FT_PTR,
    pub alloc: Option<unsafe extern "C" fn(*const FT_MemoryRec, c_long)>,
    pub free: Option<unsafe extern "C" fn(*const c_void)>,
    pub realloc: Option<unsafe extern "C" fn(*const FT_MemoryRec, c_long, c_long, *const c_void)>,
}

#[repr(C)]
pub struct FT_StreamRec {
    pub value: c_long,
    pub pointer: *const FT_PTR,
}

#[repr(C)]
pub struct FT_ListRec {
    pub head: *const FT_ListNodeRec,
    pub tail: *const FT_ListNodeRec,
}

#[repr(C)]
pub struct FT_ListNodeRec {
    pub prev: *const FT_ListNodeRec,
    pub next: *const FT_ListNodeRec,
    pub data: *const c_void,
}

#[repr(C)]
pub enum FT_Encoding {
    FT_ENCODING_NONE = 0,

    FT_ENCODING_MS_SYMBOL = ('s' as isize) << 24 | ('y' as isize) << 16 | ('m' as isize) << 8 | ('b' as isize),
    FT_ENCODING_UNICODE   = ('u' as isize) << 24 | ('n' as isize) << 16 | ('i' as isize) << 8 | ('c' as isize),

    FT_ENCODING_SJIS      = ('s' as isize) << 24 | ('j' as isize) << 16 | ('i' as isize) << 8 | ('s' as isize),
    FT_ENCODING_PRC       = ('g' as isize) << 24 | ('b' as isize) << 16 | (' ' as isize) << 8 | (' ' as isize),
    FT_ENCODING_BIG5      = ('b' as isize) << 24 | ('i' as isize) << 16 | ('g' as isize) << 8 | ('5' as isize),
    FT_ENCODING_WANSUNG   = ('w' as isize) << 24 | ('a' as isize) << 16 | ('n' as isize) << 8 | ('s' as isize),
    FT_ENCODING_JOHAB     = ('j' as isize) << 24 | ('o' as isize) << 16 | ('h' as isize) << 8 | ('a' as isize),

    FT_ENCODING_ADOBE_STANDARD = ('A' as isize) << 24 | ('D' as isize) << 16 | ('O' as isize) << 8 | ('B' as isize),
    FT_ENCODING_ADOBE_EXPERT   = ('A' as isize) << 24 | ('D' as isize) << 16 | ('B' as isize) << 8 | ('E' as isize),
    FT_ENCODING_ADOBE_CUSTOM   = ('A' as isize) << 24 | ('D' as isize) << 16 | ('B' as isize) << 8 | ('C' as isize),
    FT_ENCODING_ADOBE_LATN_1   = ('l' as isize) << 24 | ('a' as isize) << 16 | ('t' as isize) << 8 | ('1' as isize),

    FT_ENCODING_OLD_LATIN_2 = ('l' as isize) << 24 | ('a' as isize) << 16 | ('t' as isize) << 8 | ('2' as isize),

    FT_ENCODING_APPLE_ROMAN = ('a' as isize) << 24 | ('r' as isize) << 16 | ('m' as isize) << 8 | ('n' as isize),
}

#[repr(C)]
pub enum FT_Render_Mode {
    FT_RENDER_MODE_NORMAL = 0,
    FT_RENDER_MODE_LIGHT  = 1,
    FT_RENDER_MODE_MONO   = 2,
    FT_RENDER_MODE_LCD    = 3,
    FT_RENDER_MODE_LCD_V  = 4,
    FT_RENDER_MODE_SDF    = 5,
}

#[repr(C)]
pub struct FT_Glyph_Metrics {
    pub width: FT_Pos,
    pub height: FT_Pos,

    pub horiBearingX: FT_Pos,
    pub horiBearingY: FT_Pos,
    pub horiAdvance: FT_Pos,

    pub vertBearingX: FT_Pos,
    pub vertBearingY: FT_Pos,
    pub vertAdvance: FT_Pos,
}

#[repr(C)]
struct FT_Face_InternalRec {
    _foo: [u8;0],
}

// raw pointers aren't Send/Sync, but I need Face below to be able to be put
// into a Mutex. So, store the opaque pointer and only access the actual struct
// when needed. Still just as unsafe, but it's FFI...
#[repr(C)]
pub struct FT_FaceRecPtr {
    _foo: [u8;0],
}

// A rust friendly wrapper around a FreeType Face. This isn't publicly
// constructed, instead use new_face from Library
pub struct Face {
    face: &'static FT_FaceRecPtr,
    library: &'static FT_LibraryRec,
    path: String,
}

impl Face {
    fn new(library: &'static FT_LibraryRec, path: &str) -> Result<Face, ()> {
        let mut face_ptr: *const FT_FaceRec = 0 as *const _;

        let path_str = CString::new(path).unwrap();

        if unsafe { FT_New_Face(library, path_str.as_ptr(), 0, &mut face_ptr) } != 0 {
            error!("Couldn't load font from {}", path);
            return Err(());
        }

        Ok(Face{
            face: unsafe { std::mem::transmute(face_ptr) },
            library: library,
            path: String::from(path),
        })
    }

    unsafe fn ft_face(&self) -> &'static FT_FaceRec {
        unsafe { std::mem::transmute(self.face) }
    }

    pub fn family_name(&self) -> Option<&'static str> {
        if let Ok(s) = unsafe { CStr::from_ptr(self.ft_face().family_name).to_str() } {
            return Some(s);
        }

        None
    }

    pub fn style_name(&self) -> Option<&'static str> {
        if let Ok(s) = unsafe { CStr::from_ptr(self.ft_face().style_name).to_str() } {
            return Some(s);
        }

        None
    }

    // The maximum glyph width of this face, using the current size and config
    pub fn max_glyph_width_pixels(&self) -> i32 {
        unsafe {
            let face = self.ft_face();
            FT_MulFix(face.bbox.xMax - face.bbox.xMin, (*face.size).metrics.x_scale) / 64
        }
    }

    pub fn max_glyph_height_pixels(&self) -> i32 {
        unsafe {
            let face = self.ft_face();
            FT_MulFix(face.bbox.yMax - face.bbox.yMin, (*face.size).metrics.y_scale) / 64
        }
    }

    unsafe fn get_mm_var(&self) -> Result<*const FT_MM_Var,()> {
        let mut mm_ptr: *const FT_MM_Var = 0 as *const _;

        if unsafe { FT_Get_MM_Var(self.ft_face(), &mut mm_ptr) } != 0 {
            return Err(());
        }

        Ok(mm_ptr)
    }

    pub fn set_axis_to_default(&self, axis_tag: &str) {
        let tag_bytes = axis_tag.as_bytes();

        if tag_bytes.len() != 4 {
            error!("Axis tag is not 4 bytes long: {}", axis_tag);
            return;
        }

        let tag_val = (tag_bytes[0] as u32) << 24 |
                      (tag_bytes[1] as u32) << 16 |
                      (tag_bytes[2] as u32) <<  8 |
                      (tag_bytes[3] as u32);

        if let Ok(mm_var) = unsafe { self.get_mm_var() } {
            let num_axis = unsafe { (*mm_var).num_axis } as usize;
            let axes = unsafe { std::slice::from_raw_parts((*mm_var).axis, num_axis) };
            let mut coords = vec![0; num_axis];

            if unsafe { FT_Get_Var_Design_Coordinates(self.ft_face(), num_axis as u32, coords.as_mut_ptr()) } != 0 {
                error!("Can't get variable axes coordinates for {}.", self.path);
                return;
            }

            for i in 0..num_axis {
                if axes[i].tag != tag_val {
                    continue;
                }

                let def = axes[i].def / 0x10000;

                debug!("Setting axis coordinate for {} to default ({}).", axis_tag, def);
                coords[i] = axes[i].def;

                if unsafe { FT_Set_Var_Design_Coordinates(self.ft_face(), num_axis as u32, coords.as_ptr()) } != 0 {
                    error!("FT_Set_var_Design_Coordinates failed.");
                }

                if unsafe { FT_Done_MM_Var(self.library, mm_var) } != 0 {
                    error!("FT_Done_MM_Var failed.");
                }

                return; // return out after setting the values
            }

            // only get here if the tag was never found
            if unsafe { FT_Done_MM_Var(self.library, mm_var) } != 0 {
                error!("FT_Done_MM_Var failed.");
            }
            warn!("Variable axis '{}' not found in font ({}).", axis_tag, self.path);
        } else {
            warn!("Can't set axes on non-variable font ({}).", self.path);
        }

    }

    // Set variable axis coord (value). This allows to use and set variable fonts
    // attributes, like weight. The tag must be the tag name, like 'wght'.
    pub fn set_axis_coord(&self, axis_tag: &str, coord: i32) {
        let tag_bytes = axis_tag.as_bytes();

        if tag_bytes.len() != 4 {
            error!("Axis tag is not 4 bytes long: {}", axis_tag);
            return;
        }

        let tag_val = (tag_bytes[0] as u32) << 24 |
                      (tag_bytes[1] as u32) << 16 |
                      (tag_bytes[2] as u32) <<  8 |
                      (tag_bytes[3] as u32);

        if let Ok(mm_var) = unsafe { self.get_mm_var() } {
            let num_axis = unsafe { (*mm_var).num_axis } as usize;
            let axes = unsafe { std::slice::from_raw_parts((*mm_var).axis, num_axis) };
            let mut coords = vec![0; num_axis];

            if unsafe { FT_Get_Var_Design_Coordinates(self.ft_face(), num_axis as u32, coords.as_mut_ptr()) } != 0 {
                error!("Can't get variable axes coordinates for {}.", self.path);
                return;
            }

            for i in 0..num_axis {
                if axes[i].tag != tag_val {
                    continue;
                }

                // only get here if this is the correct tag
                let min = axes[i].minimum / 0x10000;
                let max = axes[i].maximum / 0x10000;

                let mut val = coord;
                if val > max {
                    warn!("Axis coordinate out of range for {}: {} ({} - {}).", axis_tag, val, min, max);
                    val = max;
                } else if val < min {
                    warn!("Axis coordinate out of range for {}: {} ({} - {}).", axis_tag, val, min, max);
                    val = min
                }
                //debug!("Setting axis coordinate for {} to {}.", axis_tag, val);
                coords[i] = val * 0x10000;

                if unsafe { FT_Set_Var_Design_Coordinates(self.ft_face(), num_axis as u32, coords.as_ptr()) } != 0 {
                    error!("FT_Set_var_Design_Coordinates failed.");
                }

                if unsafe { FT_Done_MM_Var(self.library, mm_var) } != 0 {
                    error!("FT_Done_MM_Var failed.");
                }

                return; // return out after setting the values, don't keep looking
            }

            // only get here if the tag was never found
            if unsafe { FT_Done_MM_Var(self.library, mm_var) } != 0 {
                error!("FT_Done_MM_Var failed.");
            }
            warn!("Variable axis '{}' not found in font ({}).", axis_tag, self.path);
        } else {
            warn!("Can't set axes on non-variable font ({}).", self.path);
        }
    }

    pub fn set_pixel_sizes(&self, pixel_width: u32, pixel_height: u32) {
        unsafe {
            FT_Set_Pixel_Sizes(self.ft_face(), pixel_width, pixel_height);
        }
    }

    pub fn get_char_index(&self, codepoint: u32) -> u32 {
        unsafe {
            FT_Get_Char_Index(self.ft_face(), codepoint)
        }
    }

    pub fn load_glyph(&self, glyph_index: u32, load_flags: i32) -> Result<(),i32> {
        let r = unsafe { FT_Load_Glyph(self.ft_face(), glyph_index, load_flags) };

        if r == 0 {
            return Ok(());
        }

        Err(r)
    }

    pub fn render_glyph(&self, render_mode: FT_Render_Mode) -> Result<(),i32> {
        let r = unsafe { FT_Render_Glyph(self.ft_face().glyph, render_mode) };

        if r == 0 {
            return Ok(());
        }

        Err(r)
    }

    pub fn has_kerning(&self) -> bool {
        unsafe { self.ft_face().face_flags & FT_FACE_FLAG_KERNING != 0 }
    }

    pub fn get_kerning(&self, left_glyph: u32, right_glyph: u32) -> (i32, i32) {
        let mut kern = FT_Vector {
            x: 0,
            y: 0,
        };

        let r = unsafe { FT_Get_Kerning(self.ft_face(), left_glyph, right_glyph, 0, &mut kern) };

        if r == 0 {
            return (0, 0);
        }

        (kern.x, kern.y)
    }

    pub unsafe fn glyph(&self) -> *const FT_GlyphSlotRec {
        unsafe { self.ft_face().glyph }
    }

    pub unsafe fn size(&self) -> *const FT_SizeRec {
        unsafe { self.ft_face().size }
    }

    /*
    pub unsafe fn glyph_metrics(&self) -> &'static FT_Glyph_Metrics {
        unsafe { &(*self.ft_face().glyph).metrics }
    }

    pub unsafe fn glyph_bitmap(&self) -> &'static FT_Bitmap {
        unsafe { &(*self.ft_face().glyph).bitmap }
    }
    */
}

impl Drop for Face {
    fn drop(&mut self) {
        unsafe { FT_Done_Face(self.ft_face()); }
    }
}

#[repr(C)]
pub struct FT_FaceRec {
    pub num_faces: FT_Long,
    pub face_index: FT_Long,

    pub face_flags: FT_Long,
    pub style_flags: FT_Long,

    pub num_glyphs: FT_Long,

    pub family_name: *const c_char,
    pub style_name: *const c_char,

    pub num_fixed_sizes: FT_Int,
    pub available_sizes: *const FT_Bitmap_Size,

    pub num_charmaps: FT_Int,
    pub charmaps: *const FT_CharMapRec,

    pub generic: FT_Generic,

    pub bbox: FT_BBox,

    pub units_per_EM: FT_UShort,
    pub ascender: FT_Short,
    pub descender: FT_Short,
    pub height: FT_Short,

    pub max_advance_width: FT_Short,
    pub max_advance_height: FT_Short,

    pub underline_position: FT_Short,
    pub underline_thickness: FT_Short,

    pub glyph: *const FT_GlyphSlotRec,
    pub size: *const FT_SizeRec,
    pub charmap: FT_CharMapRec,

    pub driver: *const FT_DriverRec,
    pub memory: *const FT_MemoryRec,
    pub stream: *const FT_StreamRec,

    pub sizes_list: FT_ListRec,

    pub autohint: FT_Generic,
    pub extensions: *const FT_PTR,

    internal: *const FT_Face_InternalRec,
}

pub struct Library {
    ft_lib: &'static FT_LibraryRec,
}

impl Library {
    pub fn new() -> Result<Library, ()> {
        let mut ftlibptr: *const FT_LibraryRec = 0 as *const _;

        unsafe {
            if FT_Init_FreeType(&mut ftlibptr) != 0 {
                return Err(());
            }
        }

        Ok(Library {
            ft_lib: unsafe { &*ftlibptr },
        })
    }

    pub fn new_face(&self, path: &str) -> Result<Face, ()> {
        Face::new(self.ft_lib, path)
    }
}

impl Drop for Library {
    fn drop(&mut self) {
        unsafe { FT_Done_FreeType(self.ft_lib); }
    }
}

#[repr(C)]
pub struct FT_LibraryRec {
    _foo: [u8;0],
}

#[repr(C)]
pub struct FT_MM_Var {
    pub num_axis: FT_UInt,
    pub num_designs: FT_UInt,
    pub num_namedstyles: FT_UInt,
    pub axis: *const FT_Var_Axis,
    pub namedstyle: *const FT_Var_Named_Style,
}

#[repr(C)]
pub struct FT_Var_Axis {
    pub name: *const c_char,

    pub minimum: FT_Fixed,
    pub def: FT_Fixed,
    pub maximum: FT_Fixed,

    pub tag: FT_ULong,
    pub strid: FT_UInt,
}

#[repr(C)]
pub struct FT_Var_Named_Style {
    pub coords: *const FT_Fixed,
    pub strid: FT_UInt,
    pub psid: FT_UInt,
}

#[link(name="freetype")]
unsafe extern "C" {
    fn FT_Init_FreeType(alibrary: *mut *const FT_LibraryRec) -> c_int;
    fn FT_Done_FreeType(library: *const FT_LibraryRec) -> c_int;

    fn FT_New_Face(
        library: *const FT_LibraryRec,
        filepathname: *const c_char,
        face_index: FT_Long,
        aface: *mut *const FT_FaceRec
    ) -> c_int;
    fn FT_Done_Face(face: *const FT_FaceRec) -> c_int;

    fn FT_Set_Pixel_Sizes(face: *const FT_FaceRec, pixel_width: FT_UInt, pixel_height: FT_UInt) -> c_int;

    fn FT_Get_MM_Var(face: *const FT_FaceRec, amaster: *mut *const FT_MM_Var) -> c_int;
    fn FT_Done_MM_Var(library: *const FT_LibraryRec, amaster: *const FT_MM_Var) -> c_int;
    fn FT_Get_Var_Design_Coordinates(face: *const FT_FaceRec, num_coords: FT_UInt, coords: *mut FT_Fixed) -> c_int;
    fn FT_Set_Var_Design_Coordinates(face: *const FT_FaceRec, num_coords: FT_UInt, coords: *const FT_Fixed) -> c_int;

    fn FT_MulFix(a: FT_Long, b: FT_Long) -> FT_Long;

    fn FT_Get_Char_Index(face: *const FT_FaceRec, charcode: FT_ULong) -> c_uint;
    fn FT_Load_Glyph(face: *const FT_FaceRec, glyph_index: FT_UInt, load_flags: i32) -> c_int;
    fn FT_Render_Glyph(slot: *const FT_GlyphSlotRec, render_mode: FT_Render_Mode) -> c_int;

    fn FT_Get_Kerning(
        face: *const FT_FaceRec,
        left_glyph: FT_UInt,
        right_glyph: FT_UInt,
        kern_mode: FT_UInt,
        akerning: *mut FT_Vector
    ) -> c_int;
}
