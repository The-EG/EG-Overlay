// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Logging utilities

#![allow(dead_code)]

use std::collections::HashMap;
use std::ffi::CString;
use std::ffi::CStr;

use std::sync::Mutex;

pub mod sinks;

#[doc(hidden)]
static LOGGING: Mutex<Option<LoggingState>> = Mutex::new(None);

/// Verbosity level
#[derive(Clone, Copy)]
#[derive(PartialEq, PartialOrd)]
pub enum LoggingLevel {
    None    = 0,
    Error   = 1,
    Warning = 2,
    Info    = 3,
    Debug   = 4,
}

impl LoggingLevel {
    /// Returns a [LoggingLevel] value based on a string.
    ///
    /// Unrecognized strings will result in [LoggingLevel::None].
    pub fn from(name: &str) -> LoggingLevel {
        match name.to_uppercase().as_str() {
            "ERROR"   => { LoggingLevel::Error   },
            "WARNING" => { LoggingLevel::Warning },
            "INFO"    => { LoggingLevel::Info    },
            "DEBUG"   => { LoggingLevel::Debug   },
            _         => { LoggingLevel::None    },
        }
    }
}

impl std::fmt::Display for LoggingLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            LoggingLevel::None    => f.pad("NONE"),
            LoggingLevel::Error   => f.pad("ERROR"),
            LoggingLevel::Warning => f.pad("WARNING"),
            LoggingLevel::Info    => f.pad("INFO"),
            LoggingLevel::Debug   => f.pad("DEBUG"),
        }
    }
}

/// A logging destination, such as a text file or console output.
pub trait Sink: Send + Sync {
    fn write(&mut self, message: &str);
    fn flush(&mut self);
}

/// The global logging state
struct LoggingState {
    default_level: LoggingLevel,
    target_levels: HashMap<String, LoggingLevel>,
    sinks: Vec<Box<dyn Sink>>,
}

/// Initializes the global logging state and sets the default logging level to
/// the given value.
pub fn init(default_level: LoggingLevel) {   
    let state = LoggingState {
        default_level: default_level,
        target_levels: HashMap::new(),
        sinks: Vec::new(),
    };

    *LOGGING.lock().unwrap() = Some(state);
}

pub fn cleanup() {
    *LOGGING.lock().unwrap() = None;
}

/// Adds a sink to the global logging state.
pub fn add_sink(sink: Box<dyn Sink>) {
    LOGGING.lock().unwrap().as_mut().unwrap().sinks.push(sink);
}

/// Sets the default logging level to a new value.
pub fn set_default_level(default_level: LoggingLevel) {
    LOGGING.lock().unwrap().as_mut().unwrap().default_level = default_level;
}

/// Log a message made up of formatting args at the given level.
///
/// This function is primarily only called by other logging functions. In most
/// cases [log()] or one of the macros should be used instead.
pub fn log_fmt(target: &str, level: LoggingLevel, args: std::fmt::Arguments) {
    let s = std::fmt::format(args);
    log(target, level, &s);
}

/// Log a message at the given level.
pub fn log(target: &str, level: LoggingLevel, message: &str) {
    let mut logging_lock = LOGGING.lock().unwrap();
    let logging = logging_lock.as_mut().unwrap();

    let l = if let Some(x) = logging.target_levels.get(target) {
        x
    } else {
        &logging.default_level
    };

    if level > *l { return; }

    let mut datetime = String::new();

    let tb = __timeb64::default();
    let local = tm::default();

    unsafe {
        _ftime64_s(&tb);
        _localtime64_s(&local, &tb.time);
    }

    let mut buf: [i8;25] = [0; 25];
    let fmt = CString::new("%Y-%m-%d %T").unwrap();

    unsafe { strftime(buf.as_mut_ptr(), 25, fmt.as_ptr(), &local); }

    let t = unsafe { CStr::from_ptr(buf.as_ptr()).to_str().unwrap() };
    datetime.push_str(t);

    let logmsg = format!("{}.{:03} | {:^7} | {} | {}", datetime, tb.millitm, level, target, message);

    for sink in &mut logging.sinks {
        sink.write(&logmsg);
        //if level < LoggingLevel::Debug {
            sink.flush();
        //}
    }
}

#[doc(hidden)]
#[derive(Default)]
#[repr(C)]
struct __timeb64 {
    time: i64,
    millitm: u16,
    timezone: i16,
    dstflag: i16,
}

#[doc(hidden)]
#[derive(Default)]
#[repr(C)]
struct tm {
    tm_sec: i64,
    tm_min: i64,
    tm_hour: i64,
    tm_mday: i64,
    tm_mon: i64,
    tm_year: i64,
    tm_wday: i64,
    tm_yday: i64,
    tm_isdst: i64,
}

unsafe extern "C" {
    #[doc(hidden)]
    fn _ftime64_s(timeptr: &__timeb64) -> i64;
    #[doc(hidden)]
    fn _localtime64_s(tmdest: &tm, sourcetime: &i64) -> i64;
    #[doc(hidden)]
    fn strftime(dest: *mut i8, maxsize: u64, format: *const i8, timeptr: &tm) -> u64;
}

// first 12 characters removed from the module path since it will always be eg_overlay::
macro_rules! log_ {
    ($level:expr, $($msg:tt)+) => {{
        $crate::logging::log_fmt(&module_path!()[12..], $level, format_args!($($msg)*));
    }}
}
pub(crate) use log_ as log;

macro_rules! info {
    ($($t:tt)+) => {{
        $crate::logging::log!($crate::logging::LoggingLevel::Info, $($t)*);
    }}
}
pub(crate) use info;

macro_rules! debug {
    ($($t:tt)+) => {{
        $crate::logging::log!($crate::logging::LoggingLevel::Debug, $($t)*);
    }}
}
pub(crate) use debug;

macro_rules! warn_ {
    ($($t:tt)+) => {{
        $crate::logging::log!($crate::logging::LoggingLevel::Warning, $($t)*);
    }}
}
pub(crate) use warn_ as warn;

macro_rules! error {
    ($($t:tt)+) => {{
        $crate::logging::log!($crate::logging::LoggingLevel::Error, $($t)*);
    }}
}
pub(crate) use error;
