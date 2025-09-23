// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
overlay
=======

.. lua:module:: overlay

.. code:: lua

    local overlay = require 'overlay'

The :lua:mod:`overlay` module contains core functions that other modules use
to communicate with the overlay.

Functions
---------
*/
use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;
use crate::lua_manager;

use xml::reader::XmlEvent;

// A Lua value that is passed to queueevent
// On the Rust side we store a reference  to the Lua value and then
// use it to push the value back onto the Lua stack when the event is being
// sent.
struct LuaEventData {
    value: i64,
}

impl LuaEventData {
    pub fn new(l: &lua_State, ind: i32) -> Box<LuaEventData> {
        lua::pushvalue(l, ind);

        let refi = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

        Box::new(LuaEventData {
            value: refi,
        })
    }
}

impl Drop for LuaEventData {
    fn drop(&mut self) {
        lua_manager::unref(self.value);
    }
}

impl lua_manager::ToLua for LuaEventData {
    fn push_to_lua(&self, l: &lua_State) {
        lua::rawgeti(l, lua::LUA_REGISTRYINDEX, self.value);
    }
}

const OVERLAY_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"time"                , time,
    c"logdebug"            , log_debug,
    c"loginfo"             , log_info,
    c"logwarn"             , log_warn,
    c"logerror"            , log_error,
    c"addeventhandler"     , add_event_handler,
    c"removeeventhandler"  , remove_event_handler,
    c"addkeybindhandler"   , add_keybind_handler,
    c"removekeybindhandler", remove_keybind_handler,
    c"settings"            , settings,
    c"memusage"            , memusage,
    c"videomemusage"       , videomemusage,
    c"processtime"         , process_time,
    c"queueevent"          , queue_event,
    c"datafolder"          , data_folder,
    c"overlaysettings"     , overlay_settings,

    c"restart"             , restart,

    c"versionstring"       , version_string,

    c"clipboardtext"       , clipboard_text,

    c"sqlite3open"         , sqlite3_open,

    c"webrequest"          , web_request,

    c"parsejson"           , parse_json,

    c"openzip"             , open_zip,

    c"parsexml"            , parse_xml,
};

pub unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    lua::newtable(l);
    lua::L::setfuncs(l, OVERLAY_FUNCS, 0);

    return 1;
}

/*** RST
.. lua:function:: time()

    Returns the factional number of seconds since the overlay was started.

    :rtype: number

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'
        local now = overlay.time()

        overlay.loginfo(string.format('The overlay has been running for %f seconds', now))

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn time(l: &lua_State) -> i32 {
    let t = crate::overlay::uptime().as_secs_f64();
    lua::pushnumber(l, t);

    return 1;
}

pub fn get_lua_source(l: &lua_State) -> String {
    let mut dbg = lua::lua_Debug::default();

    lua::getstack(l, 1, &mut dbg).unwrap();
    lua::getinfo(l, "S", &mut dbg).unwrap();

    let src = unsafe { std::ffi::CStr::from_ptr(dbg.source).to_str().unwrap() };

    src.to_string()
}

pub fn get_module_name(l: &lua_State) -> String {

    let src = get_lua_source(l);

    if !src.starts_with("@") && !src.starts_with("=") { return src; }

    let src_path = std::path::PathBuf::from(&src[1..]);
    let mut curdir = std::env::current_dir().unwrap();
    curdir.push("lua");

    if src_path.starts_with(&curdir) {
        let mut modpath: String = src_path.strip_prefix(&curdir).unwrap().to_string_lossy().into_owned();
        modpath = modpath.replace("\\", ".");
        modpath = String::from(modpath.trim_end_matches(".lua"));
        modpath = String::from(modpath.trim_end_matches(".init"));

        return modpath;
    }

    src_path.as_path().to_str().unwrap().to_string()
}

macro_rules! luawarn {
    ($lua:ident, $($t:tt)+) => {{
        let mut dbg = $crate::lua::lua_Debug::default();

        $crate::lua::getstack($lua, 1, &mut dbg).unwrap();
        $crate::lua::getinfo($lua, "Sl", &mut dbg).unwrap();

        let src = unsafe { std::ffi::CStr::from_ptr(dbg.source).to_str().unwrap() };

        $crate::logging::warn!("{}@{}: {}", src, dbg.currentline, format_args!($($t)*));
    }}
}
pub(crate) use luawarn;

macro_rules! luaerror {
    ($lua:ident, $($t:tt)+) => {{
        let mut dbg = $crate::lua::lua_Debug::default();

        $crate::lua::getstack($lua, 1, &mut dbg).unwrap();
        $crate::lua::getinfo($lua, "Sl", &mut dbg).unwrap();

        let src = unsafe { std::ffi::CStr::from_ptr(dbg.source).to_str().unwrap() };

        $crate::logging::error!("{}@{}: {}", src, dbg.currentline, format_args!($($t)*));
    }}
}
pub(crate) use luaerror;

/*** RST
.. lua:function:: logdebug(message)

    Logs a debug message to the log.

    .. note::

        Lua's ``string.format`` can be used to format messages.

    .. code-block:: lua
        :caption: Example

        overlay.logdebug(string.format("Hello log: %d", 1234))

    :param string message: The log message to display.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn log_debug(l: &lua_State) -> i32 {
    use crate::logging;

    let msg = unsafe { lua::L::checkstring(l, 1) };
    let name = get_module_name(l);

    logging::log(&name, logging::LoggingLevel::Debug, &msg);

    return 0;
}

/*** RST
.. lua:function:: loginfo(message)

    Logs an informational message to the log.

    .. seealso::

        See :lua:func:`logdebug` for notes and example.

    :param string message: The log message to display.

    .. versionhistory::
        :0.3.0: Added

*/
unsafe extern "C" fn log_info(l: &lua_State) -> i32 {
    use crate::logging;

    let msg = unsafe { lua::L::checkstring(l, 1) };
    let name = get_module_name(l);

    logging::log(&name, logging::LoggingLevel::Info, &msg);

    return 0;
}

/*** RST
.. lua:function:: logwarn(message)

    Logs a warning message to the log.

    .. seealso::

        See :lua:func:`logdebug` for notes and example.

    :param string message: The log message to display.

    .. versionhistory::
        :0.3.0: Added

*/
unsafe extern "C" fn log_warn(l: &lua_State) -> i32 {
    use crate::logging;

    let msg = unsafe { lua::L::checkstring(l, 1) };
    let name = get_module_name(l);

    logging::log(&name, logging::LoggingLevel::Warning, &msg);

    return 0;
}

/*** RST
.. lua:function:: logerror(message)

    Logs an error message to the log.

    .. seealso::

        See :lua:func:`logdebug` for notes and example.

    :param string message: The log message to display.

    .. versionhistory::
        :0.3.0: Added

*/
unsafe extern "C" fn log_error(l: &lua_State) -> i32 {
    use crate::logging;

    let msg = unsafe { lua::L::checkstring(l, 1) };
    let name = get_module_name(l);

    logging::log(&name, logging::LoggingLevel::Error, &msg);

    return 0;
}

/*** RST
.. lua:function:: addeventhandler(event, handler)

    Add an event handler for the given event name.

    The handler function will be called every time the given event is
    posted with two arguments: the event name and event data. The data may be
    ``nil``, any Lua data type.

    :param string event: Event type
    :param function handler: Function to be called on the given event
    :returns: A callback ID that can be used with :lua:func:`removeeventhandler`.
    :rtype: integer

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        -- run on_update_once on only the first update event
        local update_event_handler = 0

        local function on_update_once(event, data)
            overlay.loginfo("Update event")
            overlay.removeeventhandler(update_event_handler)
        end

        update_event_handler = overlay.addeventhandler('update', on_update_once)

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn add_event_handler(l: &lua_State) -> i32 {
    let event = unsafe { lua::L::checkstring(l, 1) };

    if lua::luatype(l,2)!=lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "overlay.addeventhandler: argument #2 must be a function");
        return unsafe { lua::error(l) };
    }

    let cbi = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    lua_manager::add_lua_event_handler(&event, cbi);

    lua::pushinteger(l, cbi);

    return 1;
}

/*** RST
.. lua:function:: removeeventhandler(event, cbi)

    Remove an event handler for the given event name. The callback ID is
    returned by :lua:func:`addeventhandler`.

    :param string event: Event type
    :param integer cbi: Callback ID

    .. seealso::

        See :lua:func:`addeventhandler` for example.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn remove_event_handler(l: &lua_State) -> i32 {
    let event = unsafe { lua::L::checkstring(l, 2) };
    let cbi = unsafe { lua::L::checkinteger(l, 2) };

    lua_manager::remove_lua_event_handler(&event, cbi);

    return 0;
}

/*** RST
.. lua:function:: addkeybindhandler(keyname, handler)

    Add a keybind handler for the given key.

    ``keyname`` is a name in the form of ``{mod1}-{mod2}-{key}``, for example
    ``shift-a``, ``ctrl-alt-e``, or ``f``. Individual modifier keys can be bound
    by specifying them directly, ie ``lctrl`` or ``alt-lctrl``.

    The handler function will be called every time the corresponding key is
    pressed.

    If the handler function returns ``true``, the key event will be consumed,
    it will not be sent to other handlers or to GW2.

    .. danger::

        The handler will be run on the event thread. Any blocking or long running
        tasks in the handler will adversely affect input in the entire system.

        Since this function must return a value, and due to the above,
        ``coroutine.yield`` is not allowed and will result in an error.

    :param string keyname:
    :param function handler: A function with the following signature ``function handler(keyname) end``.

    :rtype: integer
    :returns: An ID that can be used with :lua:func:`removekeybindhandler` to remove the keybind.

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local function onkey()
            -- do things

            return true
        end

        -- run onkey everytime ctrl-shift-e is pressed, consuming the event
        overlay.addkeybindhandler('ctrl-shift-e', onkey)

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn add_keybind_handler(l: &lua_State) -> i32 {
    let keyname = unsafe { lua::L::checkstring(l, 1) };

    if lua::luatype(l,2)!=lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "overlay.addkeybindhandler: argument #2 must be a function");
        return unsafe { lua::error(l) };
    }

    let cbi = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    lua_manager::add_lua_keybind_handler(&keyname, cbi);

    lua::pushinteger(l, cbi);

    return 1;
}

/*** RST
.. lua:function:: removekeybindhandler(keyname, cbi)

    Remove a keybind handler for the given key name. The callback ID is returned
    by :lua:func:`addkeybindhandler`.

    :param string keyname:
    :param integer cbi:

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn remove_keybind_handler(l: &lua_State) -> i32 {
    let keyname = unsafe { lua::L::checkstring(l, 2) };
    let cbi = unsafe { lua::L::checkinteger(l, 2) };

    lua_manager::remove_lua_keybind_handler(&keyname, cbi);

    return 0;
}

/*** RST
.. lua:function:: settings(name)

    Create a :lua:class:`settingsstore` named ``name``.

    This function should be used by modules to create a settings store.

    .. seealso::

        The :lua:class:`settingsstore` class.

    :param string name: The name of the settings store, typically the name of the module.
    :rtype: settingsstore

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn settings(l: &lua_State) -> i32 {
    let name = unsafe { lua::L::checkstring(l, 1) };

    let settings = crate::settings::SettingsStore::new(&name);

    crate::settings::lua::pushsettings(l, settings);

    return 1;
}

/*** RST
.. lua:function:: memusage()

    Returns a table containing the overlay's system memory usage.

    This usage is for the application as a whole, not just Lua. A table is
    returned with the following fields:

    +-------------------+--------------------------------------------------------------+
    | Field             | Description                                                  |
    +===================+==============================================================+
    | workingset        | The current memory allocated to the overlay, including       |
    |                   | shared objects (libraries), in bytes.                        |
    +-------------------+--------------------------------------------------------------+
    | peakworkingset    | The maximum amount of memory the overlay has been            |
    |                   | allocated since starting including shared objects, in bytes. |
    +-------------------+--------------------------------------------------------------+
    | privateworkingset | The current memory allocated to the overlay, excluding       |
    |                   | shared objects, in bytes.                                    |
    |                   |                                                              |
    |                   | *This is representative to the overlay's private memory      |
    |                   | and is what the 'Task Manager' will display.*                |
    +-------------------+--------------------------------------------------------------+

    :rtype: table

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local mem = overlay.memusage()

        overlay.loginfo(string.format('The overlay is using %.2f MiB of memory.', mem.workingset / 1024.0 / 1024.0))


    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn memusage(l: &lua_State) -> i32 {
    use windows::Win32::System::ProcessStatus::{
        PROCESS_MEMORY_COUNTERS_EX2,
        GetProcessMemoryInfo,
    };
    use windows::Win32::System::Threading::GetCurrentProcess;

    let mut mem = PROCESS_MEMORY_COUNTERS_EX2::default();
    mem.cb = std::mem::size_of::<PROCESS_MEMORY_COUNTERS_EX2>() as u32;

    unsafe { GetProcessMemoryInfo(GetCurrentProcess(), std::mem::transmute(&mut mem), mem.cb) }
        .expect("Couldn't get process memory counters.");

    lua::newtable(l);
    lua::pushinteger(l, mem.WorkingSetSize as i64);
    lua::setfield(l, -2, "workingset");
    lua::pushinteger(l, mem.PeakWorkingSetSize as i64);
    lua::setfield(l, -2, "peakworkingset");
    lua::pushinteger(l, mem.PrivateWorkingSetSize as i64);
    lua::setfield(l, -2, "privateworkingset");

    return 1;
}

/*** RST
.. lua:function:: videomemusage()

    Returns the overlay's video memory usage, in bytes.

    :rtype: integer

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local gpumem = overlay.videomemusage()

        overlay.loginfo(string.format('Overlay GPU memory: %2.f MiB', gpumem / 1024.0 / 1024.0))


    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn videomemusage(l: &lua_State) -> i32 {
    lua::pushinteger(l, crate::overlay::dx().get_video_mem_used() as i64);

    return 1;
}

macro_rules! filetime_to_u64 {
    ($a:ident) => {{
        ($a.dwHighDateTime as u64) << 32 | ($a.dwLowDateTime as u64)
    }}
}

/*** RST
.. lua:function:: processtime()

    Returns a table containing the CPU time and uptime of the overlay process.

    This can be used to calculate the overlay's CPU usage. The table has the fields
    described below:

    +------------------+----------------------------------------------------------+
    | Field            | Description                                              |
    +==================+==========================================================+
    | processtimetotal | The total time the overlay has been running.             |
    +------------------+----------------------------------------------------------+
    | usertime         | The total time spent executing code within the overlay.  |
    +------------------+----------------------------------------------------------+
    | kerneltime       | The total time spent executing system code for the       |
    |                  | overlay, not including idle time.                        |
    +------------------+----------------------------------------------------------+
    | systemusertime   | The total time spent executing all application code on   |
    |                  | the system.                                              |
    +------------------+----------------------------------------------------------+
    | systemkerneltime | The total time spent executing system code on the system |
    |                  | including idle time.                                     |
    +------------------+----------------------------------------------------------+

    :rtype: table

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn process_time(l: &lua_State) -> i32 {
    use std::time::Duration;

    use windows::Win32::Foundation::FILETIME;
    use windows::Win32::System::Threading::{GetProcessTimes, GetCurrentProcess, GetSystemTimes};
    use windows::Win32::System::Time::SystemTimeToFileTime;
    use windows::Win32::System::SystemInformation::GetSystemTime;

    // times for the user process
    let mut ft_create = FILETIME::default();
    let mut ft_exit   = FILETIME::default();
    let mut ft_kernel = FILETIME::default();
    let mut ft_user   = FILETIME::default();

    // times for the system
    //let mut ft_sys_idle   = FILETIME::default();
    let mut ft_sys_user   = FILETIME::default();
    let mut ft_sys_kernel = FILETIME::default();

    unsafe { GetProcessTimes(GetCurrentProcess(), &mut ft_create, &mut ft_exit, &mut ft_kernel, &mut ft_user).unwrap() };
    unsafe { GetSystemTimes(None, Some(&mut ft_sys_user), Some(&mut ft_sys_kernel)).unwrap() };

    // the current time
    let mut ft_now = FILETIME::default();
    let st_now = unsafe { GetSystemTime() };

    unsafe { SystemTimeToFileTime(&st_now, &mut ft_now).unwrap() };

    let total  = Duration::from_nanos((filetime_to_u64!(ft_now) - filetime_to_u64!(ft_create)) * 100);
    let kernel = Duration::from_nanos(filetime_to_u64!(ft_kernel) * 100);
    let user   = Duration::from_nanos(filetime_to_u64!(ft_user) * 100);

    let sys_user   = Duration::from_nanos(filetime_to_u64!(ft_sys_user) * 100);
    let sys_kernel = Duration::from_nanos(filetime_to_u64!(ft_sys_kernel) * 100);

    lua::newtable(l);

    lua::pushnumber(l, total.as_millis() as f64);
    lua::setfield(l, -2, "processtimetotal");

    lua::pushnumber(l, user.as_millis() as f64);
    lua::setfield(l, -2, "usertime");

    lua::pushnumber(l, kernel.as_millis() as f64);
    lua::setfield(l, -2, "kerneltime");

    lua::pushnumber(l, sys_user.as_millis() as f64);
    lua::setfield(l, -2, "systemusertime");

    lua::pushnumber(l, sys_kernel.as_millis() as f64);
    lua::setfield(l, -2, "systemkerneltime");

    return 1;
}

/*** RST
.. lua:function:: queueevent(event[, data])

    Add a new event to the event queue.

    .. note::

        Events added with this function will be run before the next frame is
        rendered.

    .. warning::

        Module authors should take care to use unique event names. It is
        possible to queue any event with this function, however if proper data
        is not supplied event handlers may behave in unexpected ways.

    :param string event: Event name
    :param data: (Optional) Event data. This can be any Lua value.

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local eventdata = {
            foo = 'bar',
            baz = true,
        }

        overlay.queueevent('my-module-event', eventdata)

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn queue_event(l: &lua_State) -> i32 {
    let event = unsafe { lua::L::checkstring(l, 1) };

    if lua::gettop(l)==1 {
        lua_manager::queue_event(&event, None);
    } else {
        let data = LuaEventData::new(l, 2);
        lua_manager::queue_event(&event, Some(data));
    }

    return 0;
}

/*** RST
.. lua:function:: datafolder(name)

    Returns the full path to the data folder for the given module.

    Modules should store any data other than settings in this folder. The folder
    will be created by this function if it does not already exist.

    :param string name: The name of the module and corresponding folder.
    :rtype: string

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local modulefolder = overlay.datafolder('my-module')

        local f = io.open(modulefolder .. '/data.txt')

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn data_folder(l: &lua_State) -> i32 {
    let name = unsafe { lua::L::checkstring(l, 1) };

    let mut path = std::env::current_exe().unwrap();

    path.pop();
    path.push("data");
    path.push(name);

    if let Err(err) = std::fs::create_dir_all(&path) {
        luaerror!(l, "Couldn't create data directory: {}", err);
        return 0;
    }

    lua::pushstring(l, path.to_str().unwrap());

    return 1;
}

/*** RST
.. lua:function:: overlaysettings()

    Return the :lua:class:`settingsstore` for the overlay.

    This contains core settings for the overlay and UI.

    :rtype: settingsstore

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn overlay_settings(l: &lua_State) -> i32 {
    let settings = crate::overlay::settings();

    crate::settings::lua::pushsettings(l, settings);

    return 1;
}

/*** RST
.. lua:function:: restart()

    Restart the overlay.

    .. danger::

        This will shut down the overlay and restart it immediately.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn restart(_l: &lua_State) -> i32 {
    crate::overlay::restart();

    return 0;
}

/*** RST
.. lua:function:: versionstring()

    Returns a string containing the EG-Overlay version.

    For example: ``"0.3.0-dev"``.

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn version_string(l: &lua_State) -> i32 {
    lua::pushstring(l, crate::version::VERSION_STR);

    return 1;
}


/*** RST
.. lua:function:: clipboardtext([text])

    Set or return the text on the clipboard.

    :param string text: (Optional)

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        -- get the text from the clipboard
        local t = overlay.clipboardtext()

        overlay.loginfo(string.format("Got text from clipboard: %s", t))

        -- set the text on the clipboard
        overlay.clipboardtext("Hello world from EG-Overlay!")

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn clipboard_text(l: &lua_State) -> i32 {
    if lua::gettop(l) >= 1 {
        let text = unsafe { lua::L::checkstring(l, 1) };
        crate::utils::set_clipboard_text(&text);
        return 0;
    } else {
        if let Some(text) = crate::utils::get_clipboard_text() {
            lua::pushstring(l, &text);
        } else {
            lua::pushnil(l);
        }

        return 1;
    }
}

/*** RST
.. lua:function:: sqlite3open(db)

    Open or create a SQLite3 database.

    .. seealso::
        The :lua:class:`sqlite3db` class.

    :param string db: The SQLite3 database path or connection string.

    :rtype: sqlite3db

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local db = overlay.open('some-data.db')

    .. versionhistory::
        :0.3.0:
*/
unsafe extern "C" fn sqlite3_open(l: &lua_State) -> i32 {
    unsafe { crate::lua_sqlite3::sqlite3_open(l) }
}

/*** RST
.. lua:function:: webrequest(url, headers, query_params, callback)

    Queue a web request to the given URL.

    Requests are completed asynchronously, with the results provided to ``callback``.

    :param string url: The full URL. Query parameters can be excluded if they are
        supplied in ``query_params``.
    :param table headers: A list of headers to add to the request.
    :param table query_params: A list of query parameters to add to the URL.
    :param function callback: A function that will be called when the request is
        completed. This function will be called with 3 arguments: the response body
        or ``nil`` if the request failed, the HTTP status code, and a table
        containing the response headers.

    .. note::
        Web requests are currently assumed to be HTTP(S).

    .. warning::
        Do not mix ``query_params`` with parameters supplied in ``url``. This
        function does not check if ``url`` already contains parameters, it simply
        appends ``query_params`` if it contains values.

    .. important::
        All web requests are logged, with the path to the Lua source and line number
        of the ``webrequest`` call.

        The author of EG-Overlay believes that users
        should be able to easily understand when and where web requests are sent.

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        local function on_response(body, code, hdrs)
            overlay.loginfo(string.format('Got %d response from server.', code))
            if body then
                overlay.loginfo(string.format('Response body:\n%s', body))
            end
        end

        local request_headers = {}
        request_headers['X-Special-Header'] = 'SomeValue'

        local params = {}
        params['query_param'] = 1

        overlay.webrequest('https://some.url/path/etc', request_headers, params, on_response)

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn web_request(l: &lua_State) -> i32 {
    if lua::gettop(l) != 4 {
        lua::pushstring(l, "webrequest requires 4 arguments.");
        return unsafe { lua::error(l) };
    }

    if lua::luatype(l, 4) != lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "callback must be a function");
        return unsafe { lua::error(l) };
    }

    if lua::luatype(l, 3) != lua::LuaType::LUA_TTABLE {
        lua::pushstring(l, "query_params must be a table");
        return unsafe { lua::error(l) };
    }

    if lua::luatype(l, 2) != lua::LuaType::LUA_TTABLE {
        lua::pushstring(l, "headers must be a table");
        return unsafe { lua::error(l) };
    }

    let url = unsafe { lua::L::checkstring(l, 1) };

    lua::pushvalue(l, 4);
    let callback = lua::L::ref_(l, lua::LUA_REGISTRYINDEX);

    let mut hdrs: Vec<(String, String)> = Vec::new();

    lua::pushnil(l);
    while lua::next(l, 2) != 0 {
        if lua::luatype(l, -2) != lua::LuaType::LUA_TSTRING {
            lua::pop(l, 2);
            luaerror!(l, "Header keys must be strings.");
            return 0;
        }

        let key = String::from(lua::tostring(l, -2).unwrap());
        let val = String::from(lua::tostring(l, -1).unwrap());
        hdrs.push((key, val));

        lua::pop(l, 1);
    }

    let mut params: Vec<(String, String)> = Vec::new();

    lua::pushnil(l);
    while lua::next(l, 3) != 0 {
        if lua::luatype(l, -2) != lua::LuaType::LUA_TSTRING {
            lua::pop(l, 2);
            luaerror!(l, "Query parameter keys must be strings.");
            return 0;
        }

        let key = String::from(lua::tostring(l, -2).unwrap());
        let val = String::from(lua::tostring(l, -1).unwrap());
        params.push((key, val));

        lua::pop(l, 1);
    }

    let mut dbg = lua::lua_Debug::default();

    lua::getstack(l, 1, &mut dbg).unwrap();
    lua::getinfo(l, "Sl", &mut dbg).unwrap();

    let src = unsafe { std::ffi::CStr::from_ptr(dbg.source).to_str().unwrap() };

    let source = format!("{}@{}", src, dbg.currentline);

    crate::web_request::queue_request(&url, hdrs, params, callback, &source);

    return 0;
}

/*** RST
.. lua:function:: parsejson(JSON)

    Parse a JSON string into a Lua value.

    This function returns ``nil`` if the string can not be parsed.

    :param string JSON:

    .. code-block:: lua
        :caption: Example

        local overlay = require 'overlay'

        -- objects are parsed into tables
        local obj = overlay.parsejson('{"test": 1234}')
        overlay.loginfo(string.format("test: %d", obj.test))

        -- arrays are parsed into sequences
        local arr = overlay.parsejson('["hello world", 1234]')
        for i,val in ipairs(arr) do
            overlay.loginfo(string.format("%d: %s", i, val))
        end

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn parse_json(l: &lua_State) -> i32 {
    let json_str = unsafe { lua::L::checkstring(l, 1) };

    match &serde_json::from_str(&json_str) {
        Ok(val) => crate::lua_json::pushjson(l, val),
        Err(err) => {
            luaerror!(l, "Couldn't parse JSON value: {}", err);
            return 0;
        },
    }

    return 1;
}

/*** RST
.. lua:function:: openzip(path)

    Open the zip file at the given path and return a :lua:class:`zipfile`.

    .. seealso::
        The :lua:class:`zipfile` class.

    .. note::
        If an error occurs while opening/reading the zip file, this function
        will log an error and return ``nil``.

    :rtype: zipfile

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn open_zip(l: &lua_State) -> i32 {
    let path = unsafe { lua::L::checkstring(l, 1) };

    match crate::zip::open_zip(&path) {
        Ok(zip) => crate::zip::lua::pushzipfile(l, zip),
        Err(err) => {
            luaerror!(l, "Couldn't open zip file: {}", err);
            lua::pushnil(l);
        },
    }

    return 1;
}

fn push_ownedname(l: &lua_State, name: &xml::name::OwnedName) {
    lua::createtable(l, 0, 3);

    lua::pushstring(l, &name.local_name);
    lua::setfield(l, -2, "local_name");

    match &name.namespace {
        Some(ns) => lua::pushstring(l, &ns),
        None     => lua::pushnil(l),
    }
    lua::setfield(l, -2, "namespace");

    match &name.prefix {
        Some(p) => lua::pushstring(l, &p),
        None    => lua::pushnil(l),
    }
    lua::setfield(l, -2, "prefix");
}

fn push_startdoc(l: &lua_State) {
    lua::pushstring(l, "start-document");

    lua::pushnil(l);
}

fn push_enddoc(l: &lua_State) {
    lua::pushstring(l, "end-document");

    lua::pushnil(l);
}

fn push_startelement(l: &lua_State, name: &xml::name::OwnedName, attributes: &Vec<xml::attribute::OwnedAttribute>) {
    lua::pushstring(l, "start-element");

    lua::createtable(l, 0, 2); // data

    push_ownedname(l, name);
    lua::setfield(l, -2, "name");

    let mut ai = 1;
    lua::createtable(l, attributes.len() as i32, 0);
    for oa in attributes {
        lua::createtable(l, 0, 2);

        push_ownedname(l, &oa.name);
        lua::setfield(l, -2, "name");

        lua::pushstring(l, &oa.value);
        lua::setfield(l, -2, "value");

        lua::seti(l, -2, ai);

        ai += 1;
    }
    lua::setfield(l, -2, "attributes");
}

fn push_endelement(l: &lua_State, name: &xml::name::OwnedName) {
    lua::pushstring(l, "end-element");

    push_ownedname(l, name);
}

fn push_stringevent(l: &lua_State, event: &str, data: &str) {
    lua::pushstring(l, event);
    lua::pushstring(l, data);
}

/*** RST
.. lua:function:: parsexml(xml, eventcallback)

    Attempt to parse the given XML string in an event driven manner (i.e. SAX).

    ``eventcallback`` will be called on each event, corresponding to various elements
    of the XML document. ``eventcallback`` will be called with 2 arguments:
    the name of the event and the event data (see below).

    This function will only return once the document is fully parsed, returning
    ``true`` on success and ``false`` on any failure.

    .. note::

        Raising an error during the callback will cause parsing to fail and this
        function to return ``false``.

    :param string xml:
    :param function eventcallback:
    :rtype: boolean

    **Event Types**

    ================ ==================================================================
    Event Name       Event Data
    ================ ==================================================================
    start-document   ``nil``
    end-document     ``nil``
    start-element    A Lua table. See below for fields.
    end-element      A Lua table containing the following fields:

                     - ``local_name``: the element name without prefixes or namespaces
                     - ``namespace``: the element name namespace
                     - ``prefix``: the element name prefix

    cdata            A string containing the CDATA.
    comment          A string containing the comment.
    characters       A string containing the text.
    whitespace       A string containing whitespace characters.
    ================ ==================================================================


    **start-element data fields:**

    ========== =================================================================
    Field      Description
    ========== =================================================================
    name       A Lua table containing the following fields:

               - ``local_name``: the element name without prefixes or namespaces
               - ``namespace``: the element name namespace
               - ``prefix``: the element name prefix

    attributes A Lua sequence of tables for each attribute, each with ``name``
               and ``value`` fields
    ========== =================================================================

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn parse_xml(l: &lua_State) -> i32 {
    let xml = unsafe { lua::L::checkstring(l, 1) };

    if lua::luatype(l, 2) != lua::LuaType::LUA_TFUNCTION {
        lua::pushstring(l, "eventcallback must be a function.");
        return unsafe { lua::error(l) };
    }

    let reader = xml::EventReader::new(xml.as_bytes());

    for e in reader {
        match e {
            Ok(event) => {
                lua::pushvalue(l, 2); // the callback function
                match event {
                    XmlEvent::StartDocument{..}                  => push_startdoc(l),
                    XmlEvent::EndDocument                        => push_enddoc(l),
                    XmlEvent::StartElement{name, attributes, ..} => push_startelement(l, &name, &attributes),
                    XmlEvent::EndElement{name}                   => push_endelement(l, &name),
                    XmlEvent::CData(cdata)                       => push_stringevent(l, "cdata", &cdata),
                    XmlEvent::Comment(comment)                   => push_stringevent(l, "comment", &comment),
                    XmlEvent::Characters(char)                   => push_stringevent(l, "characters", &char),
                    XmlEvent::Whitespace(ws)                     => push_stringevent(l, "whitespace", &ws),
                    XmlEvent::ProcessingInstruction{..}          => {},
                }

                if let Err(_err) = lua::pcall(l, 2, 0, 0) {
                    let msg = lua::tostring(l, -1).unwrap_or(String::from("<invalid error message>"));
                    luaerror!(l, "Error while running xml event callback: {}", msg);
                    lua::pop(l, 1);

                    lua::pushboolean(l, false);

                    return 1;
                }
            },
            Err(e) => {
                luaerror!(l, "Error while parsing xml: {}", e);

                lua::pushboolean(l, false);
                return 1;
            },
        }
    }

    lua::pushboolean(l, true);
    return 1;
}


/*** RST
.. include:: /docs/_include/overlayevents.rst
*/
