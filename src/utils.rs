// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Miscellaneous utility functions.

use windows::Win32::System::DataExchange;
use windows::Win32::System::Console;
use windows::Win32::System::Com;
use windows::Win32::System::Memory;
use windows::Win32::Foundation;

use crate::logging::error;

/// Returns [true] if the overlay process has a console available for output,
/// [false] otherwise.
pub fn have_console() -> bool {
    unsafe {
        return Console::AttachConsole(Console::ATTACH_PARENT_PROCESS).is_ok();
    }
}

/// Initialize Windows COM for the calling thread.
///
/// This should only be called once per thread.
pub fn init_com_for_thread() {
    unsafe {
        let r = Com::CoInitializeEx(None, Com::COINIT_MULTITHREADED);

        if r == Foundation::S_FALSE {
            panic!("COM already initialized on this thread.");
        }

        if r == Foundation::RPC_E_CHANGED_MODE {
            panic!("COM already initialized on this thread in a different mode.");
        }
    }
}

/// Shutdown Windows COM for the calling thread.
///
/// This should be called once for each time [init_com_for_thread] is called.
pub fn uninit_com_for_thread() {
    unsafe {
        Com::CoUninitialize();
    }
}

/// Return the clipboard contents as text if possible.
pub fn get_clipboard_text() -> Option<String> {
    if let Err(err) = unsafe { DataExchange::OpenClipboard(None) } {
        error!("Couldn't open clipboard: {}", err);
        return None;
    }

    match unsafe { DataExchange::GetClipboardData(0x01) } { // CF_TEXT
        Ok(h) => {
            let hg = Foundation::HGLOBAL(h.0);
            let textptr = unsafe { Memory::GlobalLock(hg) };
            let textcstr = unsafe { std::ffi::CStr::from_ptr(textptr as *const i8) };

            let text = textcstr.to_string_lossy().to_string();

            unsafe { let _ = Memory::GlobalUnlock(hg); }
            unsafe { let _ = DataExchange::CloseClipboard(); }

            return Some(text);
        },
        Err(err) => {
            unsafe { let _ = DataExchange::CloseClipboard(); }
            error!("Couldn't get text from clipboard: {}", err);
            return None;
        }
    }
}

/// Set the clipboard contents to the given text
pub fn set_clipboard_text(text: &str) {
    if let Err(err) = unsafe { DataExchange::OpenClipboard(None) } {
        error!("Couldn't open clipboard: {}", err);
        return;
    }

    if let Err(err) = unsafe { DataExchange::EmptyClipboard() } {
        unsafe { let _ = DataExchange::CloseClipboard(); }
        error!("Couldn't clear clipboard: {}", err);
        return;
    }

    let cstr = std::ffi::CString::new(text).unwrap();
    let bytes = cstr.as_bytes_with_nul();

    let glbltext: Foundation::HGLOBAL;

    match unsafe { Memory::GlobalAlloc(Memory::GMEM_MOVEABLE, bytes.len()) } {
        Ok(h) => glbltext = h,
        Err(err) => {
            unsafe { let _ = DataExchange::CloseClipboard(); }
            error!("Couldn't allocate global memory: {}", err);
            return;
        }
    }

    let textptr = unsafe { Memory::GlobalLock(glbltext) };

    unsafe { std::ptr::copy_nonoverlapping(bytes.as_ptr(), textptr as *mut u8, bytes.len()); }

    unsafe { let _ = Memory::GlobalUnlock(glbltext); }

    if let Err(err) = unsafe { DataExchange::SetClipboardData(0x01, Some(Foundation::HANDLE(glbltext.0))) } {
        error!("Couldn't set clipboard text: {}", err);
    }
    unsafe { let _ = DataExchange::CloseClipboard(); }
}
