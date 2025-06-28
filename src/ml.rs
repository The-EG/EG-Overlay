// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! MumbleLink data access
pub mod lua;

use std::sync::{Arc, Mutex};

#[allow(unused_imports)]
use crate::logging::{debug, info, warn, error};

use crate::lamath;

use windows::Win32::System::Memory;
use windows::Win32::Foundation;

pub const UI_STATE_MAP_OPEN         : u32 = 0x01;
pub const UI_STATE_COMPASS_TOP_RIGHT: u32 = 0x01 << 1;
pub const UI_STATE_COMPASS_ROTATE   : u32 = 0x01 << 2;

#[repr(C)]
struct GW2MLContext {
    server_address: [u8; 28],
    
    map_id  : u32,
    map_type: u32,
    shard_id: u32,
    instance: u32,
    build_id: u32,
    ui_state: u32,

    compass_width : u16,
    compass_height: u16,

    compass_rotation: f32,

    player_x: f32,
    player_y: f32,

    map_center_x: f32,
    map_center_y: f32,

    map_scale: f32,

    process_id: u32,

    mount_index: u8,
}

#[repr(C)]
struct GW2MumbleLinkData {
    version: u32,
    tick   : u32,

    avatar_position: lamath::Vec3F,
    avatar_front   : lamath::Vec3F,
    avatar_top     : lamath::Vec3F,

    name: [u16; 256],

    camera_position: lamath::Vec3F,
    camera_front   : lamath::Vec3F,
    camera_top     : lamath::Vec3F,

    identity: [u16; 256],

    context_len: u32,

    context: GW2MLContext,

    description: [u16; 2048],
}

pub struct MumbleLink {
    map_handle: usize,
    gw2_ml: &'static GW2MumbleLinkData,

    identity: Mutex<MLIdentityData>,
}

struct MLIdentityData {
    tick: u32, // the last time identity json was parsed
    json: serde_json::Value,
}

fn wide_str_to_string(wide: &[u16]) -> String {
    let mut len = wide.len();

    for n in 0..len {
        if wide[n] == 0x0 {
            len = n;
            break;
        }
    }

    if len == 0 {
        return String::new();
    }

    String::from_utf16_lossy(&wide[..len])
}


impl MumbleLink {
    pub fn new() -> Arc<MumbleLink> {
        info!("Opening MumbleLink shared memory...");

        let map = unsafe { Memory::CreateFileMappingA(
            Foundation::INVALID_HANDLE_VALUE,
            None,
            Memory::PAGE_READWRITE,
            0,
            std::mem::size_of::<GW2MumbleLinkData>() as u32,
            windows::core::s!("MumbleLink")
        ).expect("Couldn't create MumbleLink shared file.") };

        let ml: &'static GW2MumbleLinkData;

        ml = unsafe {
            std::mem::transmute(Memory::MapViewOfFile(
                map,
                Memory::FILE_MAP_ALL_ACCESS,
                0,
                0,
                std::mem::size_of::<GW2MumbleLinkData>()
            ).Value)
        };

        crate::lua_manager::add_module_opener("mumble-link", Some(lua::open_module));

        let ml = Arc::new(MumbleLink {
            map_handle: map.0 as usize,
            gw2_ml: ml,

            identity: Mutex::new(MLIdentityData {
                tick: 0,
                json: serde_json::Value::Null,
            }),
        });

        lua::set_ml(Arc::downgrade(&ml));

        ml
    }

    pub fn version(&self) -> u32 {
        self.gw2_ml.version
    }

    pub fn tick(&self) -> u32 {
        self.gw2_ml.tick
    }

    pub fn avatar_position(&self) -> &lamath::Vec3F {
        &self.gw2_ml.avatar_position
    }

    pub fn avatar_front(&self) -> &lamath::Vec3F {
        &self.gw2_ml.avatar_front
    }

    pub fn avatar_top(&self) -> &lamath::Vec3F {
        &self.gw2_ml.avatar_top
    }

    pub fn name(&self) -> String {
        wide_str_to_string(&self.gw2_ml.name)
    }

    pub fn camera_position(&self) -> &lamath::Vec3F {
        &self.gw2_ml.camera_position
    }

    pub fn camera_front(&self) -> &lamath::Vec3F {
        &self.gw2_ml.camera_front
    }

    pub fn camera_top(&self) -> &lamath::Vec3F {
        &self.gw2_ml.camera_top
    }

    pub fn identity(&self) -> String {
        wide_str_to_string(&self.gw2_ml.identity)
    }

    fn parse_identity_json(&self) {
        let mut ident = self.identity.lock().unwrap();
        
        if self.gw2_ml.tick != ident.tick {
            match serde_json::from_str::<serde_json::Value>(&self.identity()) {
                Ok(v) => {
                    ident.json = v.clone();
                    ident.tick = self.gw2_ml.tick;
                },
                Err(error) => error!("Couldn't parse identity: {}", error),
            }
        }
    }

    fn identity_value(&self, key: &str) -> Result<serde_json::Value, ()> {
        self.parse_identity_json();
        
        let ident = self.identity.lock().unwrap();

        if let serde_json::Value::Object(i) = &ident.json {
            if let Some(v) = i.get(key) {
                return Ok(v.clone())
            }
        }

        Err(())
    }

    pub fn identity_name(&self) -> Option<String> {
        if let Ok(nm) = self.identity_value("name") {
            if let serde_json::Value::String(name) = nm {
                return Some(name.clone());
            } else {
                error!("Identity name isn't a string!");
            }
        }
        
        None
    }

    pub fn identity_profession(&self) -> Option<i64> {
        if let Ok(prof) = self.identity_value("profession") {
            if let serde_json::Value::Number(profnum) = prof {
                return Some(profnum.as_i64().unwrap());
            } else {
                error!("Identity profession isn't a number!");
            }
        }

        None
    }

    pub fn identity_spec(&self) -> Option<i64> {
        if let Ok(spec) = self.identity_value("spec") {
            if let serde_json::Value::Number(specnum) = spec {
                return Some(specnum.as_i64().unwrap());
            } else {
                error!("Identity specialization isn't a number!");
            }
        }

        None
    }

    pub fn identity_race(&self) -> Option<i64> {
        if let Ok(race) = self.identity_value("race") {
            if let serde_json::Value::Number(racenum) = race {
                return Some(racenum.as_i64().unwrap());
            } else {
                error!("Identity race isn't a number!");
            }
        }

        None
    }

    pub fn identity_map_id(&self) -> Option<i64> {
        if let Ok(mapid) = self.identity_value("map_id") {
            if let serde_json::Value::Number(mapid_num) = mapid {
                return Some(mapid_num.as_i64().unwrap());
            } else {
                error!("Identity map_id isn't a number!");
            }
        }

        None
    }

    pub fn identity_world_id(&self) -> Option<i64> {
        if let Ok(worldid) = self.identity_value("world_id") {
            if let serde_json::Value::Number(worldid_num) = worldid {
                return Some(worldid_num.as_i64().unwrap());
            } else {
                error!("Identity world_id isn't a number!");
            }
        }

        None
    }

    pub fn identity_team_color_id(&self) -> Option<i64> {
        if let Ok(colorid) = self.identity_value("team_color_id") {
            if let serde_json::Value::Number(colorid_num) = colorid {
                return Some(colorid_num.as_i64().unwrap());
            } else {
                error!("Identity team_color_id isn't a number!");
            }
        }

        None
    }

    pub fn identity_commander(&self) -> Option<bool> {
        if let Ok(comm) = self.identity_value("commander") {
            if let serde_json::Value::Bool(comm_val) = comm {
                return Some(comm_val);
            } else {
                error!("Identity commander isn't a boolean!");
            }
        }

        None
    }

    pub fn identity_fov(&self) -> Option<f64> {
        if let Ok(fov) = self.identity_value("fov") {
            if let serde_json::Value::Number(fov_num) = fov {
                return Some(fov_num.as_f64().unwrap());
            } else {
                error!("Identity fov isn't a number!");
            }
        }

        None
    }

    pub fn identity_uisz(&self) -> Option<i64> {
        if let Ok(uisz) = self.identity_value("uisz") {
            if let serde_json::Value::Number(uisz_num) = uisz {
                return Some(uisz_num.as_i64().unwrap());
            } else {
                error!("Identity uisz isn't a number!");
            }
        }

        None
    }

    pub fn context_server_address(&self) -> String {

        match i16::from_le_bytes(self.gw2_ml.context.server_address[0..2].try_into().unwrap()) {
            2 => {
                // ipv4
                //let port = u16::from_le_bytes(self.gw2_ml.context.server_address[2..4].try_into().unwrap());
                let a = self.gw2_ml.context.server_address[4];
                let b = self.gw2_ml.context.server_address[5];
                let c = self.gw2_ml.context.server_address[6];
                let d = self.gw2_ml.context.server_address[7];

                format!("{}.{}.{}.{}", a, b, c, d)
            },
            23 => {
                // ipv6
                //let port = u16::from_le_bytes(self.gw2_ml.context.server_address[2..4].try_into().unwrap());

                let a = u16::from_le_bytes(self.gw2_ml.context.server_address[12..14].try_into().unwrap());
                let b = u16::from_le_bytes(self.gw2_ml.context.server_address[14..16].try_into().unwrap());
                let c = u16::from_le_bytes(self.gw2_ml.context.server_address[16..18].try_into().unwrap());
                let d = u16::from_le_bytes(self.gw2_ml.context.server_address[18..20].try_into().unwrap());
                let e = u16::from_le_bytes(self.gw2_ml.context.server_address[20..22].try_into().unwrap());
                let f = u16::from_le_bytes(self.gw2_ml.context.server_address[22..24].try_into().unwrap());
                let g = u16::from_le_bytes(self.gw2_ml.context.server_address[24..26].try_into().unwrap());
                let h = u16::from_le_bytes(self.gw2_ml.context.server_address[26..28].try_into().unwrap());

                format!("[{}:{}:{}:{}:{}:{}:{}:{}]", a, b, c, d, e, f, g, h)
            },
            _ => {
                String::from("(none)")
            }
        }
    }

    pub fn context_map_scale(&self) -> f32 {
        self.gw2_ml.context.map_scale
    }

    pub fn context_ui_state(&self) -> u32 {
        self.gw2_ml.context.ui_state
    }

    pub fn context_compass_width(&self) -> u16 {
        self.gw2_ml.context.compass_width
    }

    pub fn context_compass_height(&self) -> u16 {
        self.gw2_ml.context.compass_height
    }

    pub fn context_map_center_x(&self) -> f32 {
        self.gw2_ml.context.map_center_x
    }

    pub fn context_map_center_y(&self) -> f32 {
        self.gw2_ml.context.map_center_y
    }

    pub fn context_compass_rotation(&self) -> f32 {
        self.gw2_ml.context.compass_rotation
    }
}

impl Drop for MumbleLink {
    fn drop(&mut self) {
        info!("Closing MumbleLink shared memory...");

        let h = Foundation::HANDLE(self.map_handle as *mut std::ffi::c_void);
        let v = Memory::MEMORY_MAPPED_VIEW_ADDRESS {
            Value: self.gw2_ml as *const _ as *mut std::ffi::c_void,
        };
        
        unsafe { Memory::UnmapViewOfFile(v).unwrap(); }
        unsafe { Foundation::CloseHandle(h).unwrap(); }
    }
}
