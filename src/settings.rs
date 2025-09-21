// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Settings API
#![allow(dead_code)]

pub mod lua;

use crate::logging::{debug, info, warn, error};

use std::collections::HashMap;

use std::sync::atomic;
use std::sync::Mutex;
use std::sync::Arc;

use std::fs;
use std::path;

/// A collection of settings, backed by a JSON file.
pub struct SettingsStore {
    save_on_set: atomic::AtomicBool,

    file_path: path::PathBuf,

    data: Mutex<serde_json::Value>,
    defaults: Mutex<HashMap<String, serde_json::Value>>,
}

/// Returns the value from the JSON object based on a path.
///
/// For example, given the following data:
///
/// ```json
/// {
///   "level_one": {
///     "two": 3
///   }
/// }
/// ```
///
/// `get_value_for_path(data, "levelone.two")` would return `3`.
fn get_value_for_path<'a>(data: &'a mut serde_json::Value, path: &str) -> Option<&'a mut serde_json::Value> {
    let path_parts: Vec<&str> = path.split('.').collect();

    if path_parts.len()==1 {
        return data.get_mut(path_parts[0]);
    } else {
        if let Some(val) = data.get_mut(path_parts[0]) {
            return get_value_for_path(val, &path_parts[1..].join("."));
        }
    }

    return None;
}

/// Creates a set of JSON objects for the given path.
///
/// An object/key for each level of the path will be crated as necessary.
fn create_values_for_path(data: &mut serde_json::Value, path: &str) {
    let mut cur_val = data;

    for p in path.split('.') {
        if cur_val.get_mut(p).is_none() {
            cur_val[p] = serde_json::json!({});
        } else {
            let v = cur_val.get(p).unwrap();
            if !v.is_object() && !v.is_array() {
                return;
            }
        }

        cur_val = cur_val.get_mut(p).unwrap();
    }
}

impl SettingsStore {
    /// Creates a new [SettingsStore].
    ///
    /// Existing settings will be loaded from `settings/<name>.json` if the file
    /// exists, otherwise an empty store will be created.
    pub fn new(name: &str) -> Arc<SettingsStore> {

        let mut settings_dir = std::env::current_dir().unwrap();
        settings_dir.push("settings");

        if !fs::exists(&settings_dir).unwrap() {
            debug!("Creating {}", settings_dir.display());
            fs::create_dir(&settings_dir).expect("Can't create settings/");
        }

        let mut file_path = settings_dir.clone();
        file_path.push(String::from(name) + ".json");
        let data: serde_json::Value;

        if fs::exists(&file_path).unwrap() {
            info!("Loading {}...", file_path.display());
            let json_bytes = std::fs::read(&file_path)
                .expect(format!("Couldn't read {}", file_path.display()).as_str());
            let json_str = String::from_utf8(json_bytes)
                .expect(format!("Invalid UTF-8 in {}", file_path.display()).as_str());
            data = serde_json::from_str(&json_str)
                .expect(format!("Couldn't parse {}", file_path.display()).as_str());
        } else {
            warn!("Creating new settings file: {}", file_path.display());
            data = serde_json::json!({});
            std::fs::write(&file_path, data.to_string())
                .expect(format!("Couldn't write {}", file_path.display()).as_str());
        }

        return Arc::new(SettingsStore {
            save_on_set: atomic::AtomicBool::new(true),
            file_path: file_path,
            data: Mutex::new(data),
            defaults: Mutex::new(HashMap::new()),
        });
    }

    /// Saves the settings in this store to the backing JSON file.
    pub fn save(&self) {
        let data = self.data.lock().unwrap();
        let val: &serde_json::Value = &data;
        let jsonstr = serde_json::to_string_pretty(val).unwrap();
        std::fs::write(&self.file_path, jsonstr)
            .expect(format!("Couldn't write {}", self.file_path.display()).as_str());
    }

    /// Sets a default value for the given key.
    ///
    /// The value will not be saved into the backing JSON file, but any requests
    /// for `key` will return `value`.
    ///
    /// If a value, including `value` is later [SettingsStore::set] for the `key`,
    /// that value will be used instead.
    pub fn set_default_value<T: serde::Serialize>(&self, key: &str, value: T) {
        self.defaults.lock().unwrap().insert(String::from(key), serde_json::json!(value));
    }

    /// Sets the `value` of `key`.
    ///
    /// If `key` already exists the existing value is discarded.
    pub fn set<T: serde::Serialize>(&self, key: &str, value: T) {
        let mut data = self.data.lock().unwrap();

        let key_parts: Vec<&str> = key.split('.').collect();

        let parent_key = key_parts[..key_parts.len()-1].join(".");
        let value_key = key_parts[key_parts.len()-1];

        let parent: &mut serde_json::Value;

        if parent_key.len()==0 {
            parent = &mut data;
        } else {
            create_values_for_path(&mut data, &parent_key);
            if let Some(p) = get_value_for_path(&mut data, &parent_key) {
                parent = p;
            } else {
                error!("Couldn't set value {} in {}", value_key, parent_key);
                return;
            }
        }

        parent[value_key] = serde_json::json!(value);

        drop(data);

        if self.save_on_set.load(atomic::Ordering::Relaxed) {
            self.save();
        }
    }

    pub fn remove(&self, key: &str) -> bool {
        let mut data = self.data.lock().unwrap();

        let key_parts: Vec<&str> = key.split('.').collect();

        let parent_key = key_parts[..key_parts.len()-1].join(".");
        let value_key = key_parts[key_parts.len()-1];

        let parent: &mut serde_json::Value;

        if parent_key.len()==0 {
            parent = &mut data;
        } else {
            create_values_for_path(&mut data, &parent_key);
            if let Some(p) = get_value_for_path(&mut data, &parent_key) {
                parent = p;
            } else {
                error!("Couldn't remove value {} in {}", value_key, parent_key);
                return false;
            }
        }

        if let Some(parentobj) = parent.as_object_mut() {
            let ret: bool = if let Some(_) = parentobj.remove(value_key) { true  } else { false };

            drop(data);

            if ret && self.save_on_set.load(atomic::Ordering::Relaxed) {
                self.save();
            }

            return ret;
        }

        false
    }

    pub fn get(&self, key: &str) -> Option<serde_json::Value> {
        let mut data = self.data.lock().unwrap();

        if let Some(v) = get_value_for_path(&mut data, key) {
            return Some(v.clone());
        }

        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            return Some(val.clone());
        }

        None
    }

    /// Returns the value for `key` as a [String].
    ///
    /// If the value can not be represented as a string [None] is returned
    /// instead and a warning message is logged.
    pub fn get_string(&self, key: &str) -> Option<String> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(s) = val.as_str() {
                return Some(String::from(s));
            }
        }

        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(s) = val.as_str() {
                return Some(String::from(s));
            }
        }

        warn!("Couldn't get string value for {}", key);

        None
    }
  
    /// Returns the value for `key` as a [f64].
    ///
    /// If the value can not be represented as a float [None] is returned
    /// instead and a warning message is logged.
    pub fn get_f64(&self, key: &str) -> Option<f64> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(f) = val.as_f64() {
                return Some(f);
            }
        }
        
        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(f) = val.as_f64() {
                return Some(f);
            }
        }

        warn!("Could't get f64 value for {}", key);

        None
    }

    /// Returns the value for `key` as a [u64].
    ///
    /// If the value can not be represented as an unsigned integer [None] is returned
    /// instead and a warning message is logged.
    pub fn get_u64(&self, key: &str) -> Option<u64> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(u) = val.as_u64() {
                return Some(u);
            }
        }
        
        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(u) = val.as_u64() {
                return Some(u);
            }
        }

        warn!("Could't get u64 value for {}", key);

        None
    }

    /// Returns the value for `key` as an [i64].
    ///
    /// If the value can not be represented as a signed integer [None] is returned
    /// instead and a warning message is logged.
    pub fn get_i64(&self, key: &str) -> Option<i64> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(i) = val.as_i64() {
                return Some(i);
            }
        }
        
        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(i) = val.as_i64() {
                return Some(i);
            }
        }

        warn!("Could't get i64 value for {}", key);

        None
    }

    /// Returns the value for `key` as a [bool].
    ///
    /// If the value can not be represented as a boolean value [None] is returned
    /// instead and a warning message is logged.
    pub fn get_bool(&self, key: &str) -> Option<bool> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(b) = val.as_bool() {
                return Some(b);
            }
        }

        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(b) = val.as_bool() {
                return Some(b);
            }
        }

        warn!("Couldn't get bool value for {}", key);

        None
    }

    /// Returns the value for `key` as a [serde_json::Value::Object].
    ///
    /// The returned object is a clone, changing it will not change the settings store.
    ///
    /// If the value is not an object [None] is returned instead.
    pub fn get_object(&self, key: &str) -> Option<serde_json::Map<String, serde_json::Value>> {
        let mut data = self.data.lock().unwrap();

        if let Some(val) = get_value_for_path(&mut data, key) {
            if let Some(obj) = val.as_object() {
                return Some(obj.clone());
            }
        }

        if let Some(val) = self.defaults.lock().unwrap().get(key) {
            if let Some(obj) = val.as_object() {
                return Some(obj.clone());
            }
        }

        warn!("Couldn't get object value for {}", key);

        None
    }

    pub fn get_color(&self, key: &str) -> Option<crate::ui::Color> {
        if let Some(ival) = self.get_u64(key) {
            Some(crate::ui::Color::from(ival as u32))
        } else {
            None
        }
    }
}
