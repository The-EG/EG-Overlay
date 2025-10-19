// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
use std::io::Write;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::MutexGuard;
use std::fs::{OpenOptions, File};
use std::io::BufWriter;

use std::ffi::CString;

use windows::Win32::System::Diagnostics::Debug::OutputDebugStringA;
use windows::core::PCSTR;

pub struct DbgSink { }

impl DbgSink {
    pub fn new() -> DbgSink {
        return DbgSink { }
    }
}

impl crate::logging::Sink for DbgSink {
    fn write(&mut self, message: &str) {
        unsafe {
            OutputDebugStringA(PCSTR(CString::new(message).unwrap().as_bytes().as_ptr()));
            OutputDebugStringA(windows::core::s!("\n"));
        }
    }

    fn flush(&mut self) { }
}

impl Clone for DbgSink {
    fn clone(&self) -> Self {
        return DbgSink::new();
    }
}

pub struct ConsoleSink {
    writer: Arc<Mutex<BufWriter<std::io::Stderr>>>,
}

impl ConsoleSink {
    pub fn new() -> ConsoleSink {
        return ConsoleSink {
            writer: Arc::new(Mutex::new(BufWriter::new(std::io::stderr()))),
        };
    }

    fn lock_writer(&self) -> MutexGuard<'_, BufWriter<std::io::Stderr>> {
        return self.writer.lock().unwrap();
    }
}

impl crate::logging::Sink for ConsoleSink {
    fn write(&mut self, message: &str) {
        let mut w = self.lock_writer();
        w.write_all(message.as_bytes()).unwrap();
        w.write("\n".as_bytes()).unwrap();

    }

    fn flush(&mut self) {
        self.lock_writer().flush().unwrap();
    }
}

impl Drop for ConsoleSink {
    fn drop(&mut self) {
        self.lock_writer().flush().unwrap();
    }
}

impl Clone for ConsoleSink {
    fn clone(&self) -> Self {
        return ConsoleSink {
            writer: self.writer.clone(),
        }
    }
}

pub struct FileSink {
    file: Arc<Mutex<BufWriter<File>>>,
}

impl FileSink {
    pub fn new(path: &str) -> FileSink {
        let f = OpenOptions::new().append(true).create(true).open(path).expect("Couldn't open log file.");
        let w = BufWriter::new(f);

        return FileSink {
            file: Arc::new(Mutex::new(w)),
        }
    }

    fn lock_file(&self) -> MutexGuard<'_, BufWriter<File>> {
        return self.file.lock().unwrap();
    }
}

impl crate::logging::Sink for FileSink {
    fn write(&mut self, message: &str) {
        let mut f = self.lock_file();

        f.write_all(message.as_bytes()).unwrap();
        f.write("\n".as_bytes()).unwrap();
    }

    fn flush(&mut self) {
        self.lock_file().flush().unwrap();
    }
}

impl Drop for FileSink {
    fn drop(&mut self) {
        self.lock_file().flush().unwrap();
    }
}

impl Clone for FileSink {
    fn clone(&self) -> Self {
        return FileSink {
            file: self.file.clone(),
        }
    }
}
