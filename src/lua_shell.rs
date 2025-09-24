// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
shell
=====

.. lua:module:: shell

.. code:: lua

    local shell = require 'shell'

The :lua:mod:`shell` module contains utilities for navigating the virtual and
physical file systems on the host computer. This includes hard drives, removable
media, library folders such as 'Documents' and 'Desktop', and network locations.

Functions
---------
*/

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::mem::ManuallyDrop;
use std::sync::Arc;

use windows::Win32::UI::Shell;
use windows::core::Interface;
use windows::Win32::Storage::EnhancedStorage;
use windows::Win32::System::SystemServices;
use windows::Win32::Storage::FileSystem;
use windows::Win32::System::Com;

use crate::overlay::lua::{luaerror};

struct ShellItem {
    comptr: Shell::IShellItem2,
}

impl ShellItem {
    fn desktop() -> Arc<ShellItem> {
        ShellItem::from_known_folder(&Shell::FOLDERID_Desktop)
    }

    fn this_pc() -> Arc<ShellItem> {
        ShellItem::from_known_folder(&Shell::FOLDERID_ComputerFolder)
    }

    fn from_known_folder(guid: &windows::core::GUID) -> Arc<ShellItem> {
        let comptr = unsafe { Shell::SHCreateItemInKnownFolder::<_, Shell::IShellItem2> (
            guid,
            Shell::KF_FLAG_DEFAULT,
            None
        ).unwrap() };

        Arc::new(ShellItem { comptr: comptr })
    }

    fn from_path(path: &str) -> Result<Arc<ShellItem>, ()> {
        let mut pathu16: Vec<u16> = path.encode_utf16().collect();
        pathu16.push(0u16);

        let pathw = windows::core::PWSTR::from_raw(pathu16.as_mut_ptr());

        if let Ok(comptr) = unsafe { Shell::SHCreateItemFromParsingName::<_, _, Shell::IShellItem2> (
            pathw,
            None,
        ) } {
            Ok(Arc::new(ShellItem { comptr: comptr }))
        } else {
            Err(())
        }
    }

    fn display_name(&self) -> String {
        let namew = unsafe { self.comptr.GetDisplayName(Shell::SIGDN_NORMALDISPLAY).unwrap() };

        let name = String::from_utf16_lossy(unsafe { namew.as_wide() } );

        unsafe { Com::CoTaskMemFree(Some(namew.as_ptr() as _)) };

        name
    }

    fn name(&self) -> String {
        let namew = unsafe { self.comptr.GetDisplayName(Shell::SIGDN_PARENTRELATIVEFORADDRESSBAR).unwrap() };
        let name = String::from_utf16_lossy(unsafe { namew.as_wide() });

        unsafe { Com::CoTaskMemFree(Some(namew.as_ptr() as _)) };

        name
    }

    fn path(&self) -> Option<String> {
        let pathw: windows::core::PWSTR;

        match unsafe { self.comptr.GetDisplayName(Shell::SIGDN_FILESYSPATH) } {
            Ok(p) => pathw = p,
            Err(_) => return None,
        }

        let path = String::from_utf16_lossy(unsafe { pathw.as_wide() } );

        unsafe { Com::CoTaskMemFree(Some(pathw.as_ptr() as _)) };

        Some(path)
    }

    fn item_type(&self) -> &str {
        let fspathw = match unsafe { self.comptr.GetDisplayName(Shell::SIGDN_FILESYSPATH) } {
            Ok(s) => s,
            Err(_) => {
                return "other";
            },
        };

        let drivetype = unsafe { FileSystem::GetDriveTypeW(fspathw) };

        unsafe { Com::CoTaskMemFree(Some(fspathw.as_ptr() as _)); }

        match drivetype {
            2 => return "drive-removable",
            3 => return "drive-fixed",
            4 => return "drive-cdrom",
            5 => return "drive-ramdisk",
            _ => {}
    }

        // not a drive at this point
        let attrs = unsafe { self.comptr.GetAttributes(
            SystemServices::SFGAO_LINK   |
            SystemServices::SFGAO_FOLDER |
            SystemServices::SFGAO_FILESYSTEM
        ).unwrap() };

        if attrs.contains(SystemServices::SFGAO_LINK) {
            let linktarget: Shell::IShellItem2;

            match unsafe { self.comptr.BindToHandler::<_, Shell::IShellItem2>(
                None,
                &Shell::BHID_LinkTargetItem
            ) } {
                Ok(lt) => linktarget = lt,
                Err(_) => return "link-other",
            }

            let targetpathw = unsafe { linktarget.GetDisplayName(Shell::SIGDN_FILESYSPATH).unwrap() };

            let targetpath = String::from_utf16_lossy(unsafe { targetpathw.as_wide() });

            unsafe { Com::CoTaskMemFree(Some(targetpathw.as_ptr() as _)); }

            if &targetpath[..2] == "\\\\" {
                return "link-network";
            } else {
                return "link-local";
            }
        }

        if attrs.contains(SystemServices::SFGAO_FOLDER) {
            return "folder";
        }

        if attrs.contains(SystemServices::SFGAO_FILESYSTEM) {
            return "file";
        }

        return "other";
    }

    fn file_type(self: &Arc<ShellItem>) -> Option<String> {
        match unsafe { self.comptr.GetString(&EnhancedStorage::PKEY_ItemType) } {
            Ok(itw) => {
                let itemtype = String::from_utf16_lossy(unsafe { itw.as_wide() });
                unsafe { Com::CoTaskMemFree(Some(itw.as_ptr() as _)); }

                Some(itemtype)
            },
            Err(_) => None
        }
    }

    fn parent(&self) -> Option<Arc<ShellItem>> {
        match unsafe { self.comptr.GetParent() } {
            Ok(p) => {
                let si = p.cast::<Shell::IShellItem2>().unwrap();

                return Some(Arc::new(ShellItem { comptr: si }));
            },
            Err(_) => return None,
        }
    }

    fn children(&self) -> Vec<Arc<ShellItem>> {
        let mut c: Vec<Arc<ShellItem>> = Vec::new();

        if let Ok(en) = unsafe { self.comptr.BindToHandler::<_, Shell::IEnumShellItems>(
            None,
            &Shell::BHID_EnumItems
        ) } {
            let mut enum_children: &mut [Option<Shell::IShellItem>] = &mut [None];

            while unsafe { en.Next(&mut enum_children, None).is_ok() } && enum_children[0].is_some() {
                let child = enum_children[0].as_ref().unwrap().cast::<Shell::IShellItem2>().unwrap();

                c.push(Arc::new( ShellItem { comptr: child } ));

                enum_children[0] = None;
            }
        }

        c
    }
}

const SHELL_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"getshellitem", get_shell_item,
};

pub fn init() {
    crate::lua_manager::add_module_opener("shell", Some(open_module));
}

unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    lua::newtable(l);
    lua::L::setfuncs(l, SHELL_FUNCS, 0);

    return 1;
}

/*** RST
.. lua:function:: getshellitem(location[, path])

    Return a :lua:class:`ShellItem` for the ``location``.

    :param string location:
    :param string path: (Optional)

    ``location`` can be one of the following values:

    - ``'this-pc'``
    - ``'desktop'``
    - ``'path'``

    .. note::

        If ``location`` is ``'path'`` then ``path`` must also be present.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn get_shell_item(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);

    let loc = lua::tostring(l, 1).unwrap();

    match loc.as_str() {
        "this-pc" => pushshellitem(l, &ShellItem::this_pc()),
        "desktop" => pushshellitem(l, &ShellItem::desktop()),
        "path" => {
            if lua::gettop(l) < 2 || lua::luatype(l, 2) != lua::LuaType::LUA_TSTRING {
                luaerror!(l, "path requires a second argument.");
                return 0;
            }

            let path = lua::tostring(l, 2).unwrap();

            if let Ok(item) = ShellItem::from_path(&path) {
                pushshellitem(l, &item);
            } else {
                luaerror!(l, "Couldn't load path: {}", path);
                return 0;
            }
        },
        _ => {
            luaerror!(l, "Invalid location type: {}", loc);
            return 0;
        },
    }

    return 1;
}

const SHELL_ITEM_MT_NAME: &str = "ShellItem";

const SHELL_ITEM_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"__gc"       , shell_item_del,
    c"displayname", shell_item_display_name,
    c"name"       , shell_item_name,
    c"type"       , shell_item_type,
    c"parent"     , shell_item_parent,
    c"children"   , shell_item_children,
    c"path"       , shell_item_path,
    c"filetype"   , shell_item_filetype,
};

fn pushshellitem(l: &lua_State, shellitem: &Arc<ShellItem>) {
    let si_ptr = Arc::into_raw(shellitem.clone());

    let si_lua_ptr: *mut *const ShellItem = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const ShellItem>(), 0))
    };

    unsafe { *si_lua_ptr = si_ptr; }

    if lua::L::newmetatable(l, SHELL_ITEM_MT_NAME) {
        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");

        lua::L::setfuncs(l, SHELL_ITEM_FUNCS, 0);
    }
    lua::setmetatable(l, -2);
}

unsafe fn checkshellitem(l: &lua_State, ind:i32) -> ManuallyDrop<Arc<ShellItem>> {
    let ptr: *mut *const ShellItem = unsafe { std::mem::transmute(lua::L::checkudata(l, ind, SHELL_ITEM_MT_NAME)) };

    ManuallyDrop::new(unsafe { Arc::from_raw(*ptr) })
}

/*** RST
Classes
-------

.. lua:class:: ShellItem

    A :lua:class:`ShellItem` represents a location within a file system.

    This location could correspond to a physical file or folder on a hard drive
    or a virtual location such as 'This PC.'
*/

unsafe extern "C" fn shell_item_del(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    ManuallyDrop::into_inner(si);

    return 0;
}

/*** RST
    .. lua:method:: displayname()

        Return the display name for this item.

        All :lua:class:`ShellItems <ShellItem>` have a valid display name; this
        value is a string formatted suitable for UI display.

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_display_name(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    lua::pushstring(l, &si.display_name());

    return 1;
}

/*** RST
    .. lua:method:: name()

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_name(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    lua::pushstring(l, &si.name());

    return 1;
}

/*** RST
    .. lua:method:: type()

        Return the type of this item.

        One of the following values:

        +-----------------+--------------------------------------------------------+
        | Value           | Description                                            |
        +=================+========================================================+
        | file            | A file within a physical or network file system.       |
        +-----------------+--------------------------------------------------------+
        | folder          | A folder.                                              |
        +-----------------+--------------------------------------------------------+
        | link-local      | A link to a file or folder in a local file system.     |
        +-----------------+--------------------------------------------------------+
        | link-network    | A link to a folder on a network file system.           |
        +-----------------+--------------------------------------------------------+
        | link-other      | A link to a non-file system location, typically a URL. |
        +-----------------+--------------------------------------------------------+
        | drive-removable | A removable local file system root, typically a USB    |
        |                 | flash drive.                                           |
        +-----------------+--------------------------------------------------------+
        | drive-fixed     | A non-removable local file system root, typically a    |
        |                 | hard drive.                                            |
        +-----------------+--------------------------------------------------------+
        | drive-cdrom     | A removable local file system root, specially a CD,    |
        |                 | DVD, or Blu-Ray Disc.                                  |
        +-----------------+--------------------------------------------------------+
        | drive-ramdisk   | A removable and temporary local file system root,      |
        |                 | stored in system RAM.                                  |
        +-----------------+--------------------------------------------------------+

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_type(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    lua::pushstring(l, si.item_type());

    return 1;
}

/*** RST
    .. lua:method:: parent()

        :returns: The :lua:class:`ShellItem` parent of this item or ``nil``.

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_parent(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    match si.parent() {
        Some(p) => pushshellitem(l, &p),
        None    => lua::pushnil(l),
    }

    return 1;
}

/*** RST
    .. lua:method:: children()

        :rtype: sequence

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_children(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    let children = si.children();

    lua::createtable(l, children.len() as i32, 0);

    let mut i = 1;

    for child in &children {
        pushshellitem(l, child);
        lua::seti(l, -2, i);
        i += 1;
    }

    return 1;
}

/*** RST
    .. lua:method:: path()

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_path(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    match si.path() {
        Some(path) => lua::pushstring(l, &path),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
    .. lua:method:: filetype()

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn shell_item_filetype(l: &lua_State) -> i32 {
    let si = unsafe { checkshellitem(l, 1) };

    if let Some(file_type) = si.file_type() {
        lua::pushstring(l, &file_type);
        return 1;
    }

    return 0;
}

