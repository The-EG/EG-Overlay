// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

//! Asynchronous HTTP(s) requests
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};

use std::collections::{VecDeque, HashMap};

use std::ffi::{CString, CStr};

#[allow(unused_imports)]
use crate::logging::{info,debug,warn,error};

use windows::Win32::Networking::WinInet;
use windows::Win32::UI::Shell;
use windows::Win32::Foundation;

static WR_STATE: Mutex<WebRequestState> = Mutex::new(WebRequestState {
    internet: 0,
    thread: None,
});

static WR_RUNNING: AtomicBool = AtomicBool::new(true);

static WR_REQUESTS: Mutex<VecDeque<Request>> = Mutex::new(VecDeque::new());

struct WebRequestState {
    internet: usize,
    thread: Option<std::thread::JoinHandle<()>>,
}

pub fn init() {
    let hint = unsafe { WinInet::InternetOpenA(
        windows::core::s!("EG-Overlay/0.3.0"),
        WinInet::INTERNET_OPEN_TYPE_PRECONFIG.0,
        windows::core::PCSTR::null(),
        windows::core::PCSTR::null(),
        0
    )};

    if hint.is_null() {
        panic!("Couldn't initialize WinInet.");
    }

    WR_STATE.lock().unwrap().internet = hint as usize;

    let t = std::thread::Builder::new().name("EG-Overlay Web Request Thread".to_string()).spawn(move || {
        web_request_thread();
    }).expect("Couldn't spawn web request thread.");

    WR_STATE.lock().unwrap().thread = Some(t);
}

pub fn cleanup() {
    let t = WR_STATE.lock().unwrap().thread.take().unwrap();

    WR_RUNNING.store(false, Ordering::Relaxed);

    t.join().unwrap();

    let hint = WR_STATE.lock().unwrap().internet as *const std::ffi::c_void;

    unsafe { WinInet::InternetCloseHandle(hint) }.unwrap();
}

fn web_request_thread() {
    debug!("Request thread starting...");

    while WR_RUNNING.load(Ordering::Relaxed) {
        if let Some(req) = WR_REQUESTS.lock().unwrap().pop_front() {
            perform(&req);
        }

        std::thread::sleep(std::time::Duration::from_millis(25));
    }

    debug!("Request thread ending...");
}

struct Request {
    url: String,

    headers: Vec<(String, String)>,
    query_params: Vec<(String, String)>,

    lua_callback: i64,
    lua_source: String,
}

/// Queues a web request
///
/// Currently, this assumes URL is HTTP or HTTPS.
/// `callback` must be a Lua reference ID to a Lua callback function.
/// `source` is used to log where in code this request came from.
pub fn queue_request(
    url: &str,
    headers: Vec<(String, String)>,
    query_params: Vec<(String, String)>,
    callback: i64, source: &str
) {
    let req = Request {
        url: String::from(url),

        headers: headers,
        query_params: query_params,

        lua_callback: callback,
        lua_source: String::from(source),
    };

    WR_REQUESTS.lock().unwrap().push_back(req);
}

struct Response {
    status: i64,
    body: Vec<i8>,
    headers: HashMap<String, String>,
    target_ref: i64,
}

impl Drop for Response {
    fn drop(&mut self) {
        crate::lua_manager::unref(self.target_ref);
    }
}

impl crate::lua_manager::ToLua for Response {
    fn push_to_lua(&self, l: &crate::lua::lua_State) {
        crate::lua::newtable(l);

        crate::lua::pushinteger(l, self.status);
        crate::lua::setfield(l, -2, "status");

        crate::lua::pushbytes(l, self.body.as_slice());
        crate::lua::setfield(l, -2, "body");

        crate::lua::newtable(l);
        for (hdr, val) in self.headers.iter() {
            crate::lua::pushstring(l, val);
            crate::lua::setfield(l, -2, hdr);
        }
        crate::lua::setfield(l, -2, "headers");
    }
}

fn escape_url(url: &str) -> Result<String, windows::core::Error> {
    let url_cstr = CString::new(url).unwrap();
    let url_pcstr = windows::core::PCSTR(url_cstr.as_bytes().as_ptr());

    let mut escaped_url_bytes = vec![0u8;1];

    let mut escaped_url = windows::core::PSTR::from_raw(escaped_url_bytes.as_mut_ptr());
    let mut escaped_len: u32 = 1;

    let r = unsafe { Shell::UrlEscapeA(url_pcstr, escaped_url, &mut escaped_len, 0) };

    match r {
        Ok(_) => {
            // url was 0 length?
            return Err(windows::core::Error::new(windows::core::HRESULT(1),"Couldn't escape zero length URL"));
        },
        Err(err) => {
            if err.code() != Foundation::E_POINTER {
                // some other error happened
                return Err(err);
            }
        }
    }

    // we now have the length
    escaped_url_bytes = vec![0u8;(escaped_len + 1) as usize];
    escaped_url = windows::core::PSTR::from_raw(escaped_url_bytes.as_mut_ptr());

    unsafe { Shell::UrlEscapeA(url_pcstr, escaped_url, &mut escaped_len, 0) }?;

    let escaped_url_cstr = CStr::from_bytes_until_nul(&escaped_url_bytes).unwrap();

    Ok(String::from(escaped_url_cstr.to_string_lossy()))
}

fn get_resp_headers(hreq: *const std::ffi::c_void) -> HashMap<String, String> {
    let mut hdrs: HashMap<String, String> = HashMap::new();

    let mut raw_hdrs_len: u32 = 0;
    let mut raw_hdrs: Vec<u8> = Vec::new();

    if let Err(err) = unsafe { WinInet::HttpQueryInfoA(
        hreq,
        WinInet::HTTP_QUERY_RAW_HEADERS,
        None,
        &mut raw_hdrs_len,
        None
    )} {
        if err.code() != Foundation::ERROR_INSUFFICIENT_BUFFER.into() {
            error!("Couldn't get response buffers: {}", err);
            return hdrs;
        }
    }

    raw_hdrs.resize(raw_hdrs_len as usize, 0u8);

    if unsafe { WinInet::HttpQueryInfoA(
        hreq,
        WinInet::HTTP_QUERY_RAW_HEADERS,
        Some(raw_hdrs.as_mut_ptr() as *mut std::ffi::c_void),
        &mut raw_hdrs_len,
        None
    )}.is_err() {
        error!("Couldn't get response buffers.");
        return hdrs;
    }

    let mut cur_name = String::new();
    let mut cur_val  = String::new();
    let mut in_name = true;

    for c in raw_hdrs {
        if in_name {
            if c == b':' {
                in_name = false;
            } else if c == b'\0' {
                // header with no value?
                cur_name.clear();
                cur_val.clear();
            } else {
                cur_name.push(c as char);
            }
        } else {
            if c == b'\0' {
                in_name = true;

                if let Some(h) = hdrs.get_mut(&cur_name) {
                    h.push_str(format!(",{}", cur_val.trim()).as_str());
                } else {
                    hdrs.insert(cur_name.clone(), String::from(cur_val.trim()));
                }

                cur_name.clear();
                cur_val.clear();
            } else {
                cur_val.push(c as char);
            }
        }
    }

    hdrs
}

fn perform(request: &Request) {
    // the entire URL with query parameters
    let mut url = request.url.clone();

    for p in &request.query_params {
        if request.query_params[0] == *p {
            url += "?";
        } else {
            url += "&";
        }

        url += format!("{}={}", p.0, p.1).as_str();
    }

    let escaped_url: String;

    let re = escape_url(&url);

    match re {
        Ok(eu) => escaped_url = eu,
        Err(err) => {
            error!("Couldn't escape URL ({}): {}", url, err);
            return;
        }
    }

    let mut hdrs = String::new();
    for h in &request.headers {
        hdrs += format!("{}: {}\r\n", h.0, h.1).as_str();
    }

    let hintval = WR_STATE.lock().unwrap().internet;
    let hint = hintval as *const std::ffi::c_void;

    let headers: Option<&[u8]> = if hdrs.len() > 0 {
        Some(hdrs.as_bytes())
    } else {
        None
    };

    let escaped_url_c = CString::new(escaped_url.as_str()).unwrap();
    let escaped_url_pcstr = windows::core::PCSTR(escaped_url_c.as_bytes().as_ptr());

    let hreq = unsafe { WinInet::InternetOpenUrlA(hint, escaped_url_pcstr, headers, 0, None) };

    if hreq.is_null() {
        error!("Couldn't open URL: {}", escaped_url);
        return;
    }

    let mut data: Vec<i8> = Vec::new();

    let mut chunk = vec![0i8; 1024];
    let mut bytes_read: u32 = 0;

    while unsafe {
        WinInet::InternetReadFile(hreq, chunk.as_mut_ptr() as *mut std::ffi::c_void, 1024, &mut bytes_read)
    }.is_ok() {
        if bytes_read == 0 { break; }

        data.extend_from_slice(&chunk[0..bytes_read as usize]);
    }

    let resp_hdrs = get_resp_headers(hreq);

    let mut status_code: u32 = 0;
    let mut code_len: u32 = std::mem::size_of::<u32>() as u32;

    if let Err(err) = unsafe { WinInet::HttpQueryInfoA(
        hreq,
        WinInet::HTTP_QUERY_STATUS_CODE | WinInet::HTTP_QUERY_FLAG_NUMBER,
        Some(&mut status_code as *mut _ as *mut std::ffi::c_void),
        &mut code_len,
        None
    )} {
        unsafe { WinInet::InternetCloseHandle(hreq).unwrap(); }
        error!("Couldn't get HTTP Query Info: {}", err);
        return;
    }

    unsafe { WinInet::InternetCloseHandle(hreq).unwrap(); }

    if status_code >= 200 && status_code <400 {
        info!("{}: GET {} -> {}", request.lua_source, url, status_code);
    } else {
        warn!("{}: GET {} -> {}", request.lua_source, url, status_code);
    }

    let resp = Box::new(Response {
        status: status_code as i64,
        body: data,
        target_ref: request.lua_callback,
        headers: resp_hdrs,
    });

    crate::lua_manager::queue_targeted_event(request.lua_callback, Some(resp));
}
