// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
Zip Files
=========

.. lua:class:: zipfile

    The ZipFile class allows access to a zip file and the compressed files within.

*/
use crate::zip::ZipFile;

use std::rc::Rc;
use std::mem::ManuallyDrop;

use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::ops::DerefMut;

const ZIP_METATABLE_NAME: &str = "ZipFile";

const ZIP_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"entries", entries,
    c"__gc"   , __gc,
    c"content", content,
};

pub fn pushzipfile(l: &lua_State, zip: Rc<ZipFile>) {
    let zip_ptr = Rc::into_raw(zip.clone());

    let lua_zip_ptr: *mut *const ZipFile = unsafe {
        std::mem::transmute(lua::newuserdatauv(l, std::mem::size_of::<*const ZipFile>(), 0))
    };

    unsafe { *lua_zip_ptr = zip_ptr; }

    if lua::L::newmetatable(l, ZIP_METATABLE_NAME) {
        lua::pushvalue(l, -1);
        lua::setfield(l, -2, "__index");

        lua::L::setfuncs(l, ZIP_FUNCS, 0);
    }
    lua::setmetatable(l, -2);
}

pub unsafe fn checkzipfile(l: &lua_State, ind: i32) -> ManuallyDrop<Rc<ZipFile>> {
    let ptr: *mut *const ZipFile = unsafe { std::mem::transmute(lua::L::checkudata(l, ind, ZIP_METATABLE_NAME)) };

    ManuallyDrop::new(unsafe { Rc::from_raw(*ptr) })
}

unsafe extern "C" fn __gc(l: &lua_State) -> i32 {
    let mut zip = unsafe { checkzipfile(l, 1) };

    unsafe { ManuallyDrop::drop(&mut zip); }

    return 0;
}

/*** RST
    .. lua:method:: entries()

        Returns a list of all entries (files and directories) of this zip file.

        The returned Lua table is a sequence of tables, one for each entry,
        with the following fields:

        +--------------+-------------------------------------------------------+
        | Field        | Description                                           |
        +==============+=======================================================+
        | name         | File name. This is a full file name relative to the   |
        |              | root of the zip file and will contain path separators |
        |              | if the entry is located inside a directory.           |
        +--------------+-------------------------------------------------------+
        | is_directory | ``true`` if this entry is a directory.                |
        +--------------+-------------------------------------------------------+

        .. warning::
            Entries will be returned in a random order.

        :rtype: table

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn entries(l: &lua_State) -> i32 {
    let zip = unsafe { checkzipfile(l, 1) };

    let mut i = 1;
    lua::createtable(l, zip.central_directory.len() as i32, 0);
    for (name, cd) in zip.central_directory.iter() {
        lua::createtable(l, 0, 2);

        lua::pushstring(l, &name);
        lua::setfield(l, -2, "name");

        if cd.external_attrs & 0x010 > 0 { lua::pushboolean(l, true);  }
        else                             { lua::pushboolean(l, false); }
        lua::setfield(l, -2, "is_directory");

        lua::seti(l, -2, i);
        i += 1;
    }

    return 1;
}

/*** RST
    .. lua:method:: content(path)

        Get the content of the entry/file at path.

        If path is not valid for this zip, ``nil`` is returned.

        :param string path:

        :rtype: string

        .. versionhistory::
            :0.3.0: Added
*/
unsafe extern "C" fn content(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 2);
    let mut zipref = unsafe { checkzipfile(l, 1) };
    let zip: &mut ZipFile = Rc::get_mut(zipref.deref_mut()).unwrap();
    let path = lua::tostring(l, 2).unwrap();

    match zip.file_content(&path) {
        Ok(data) => {
            let mut v = std::mem::ManuallyDrop::new(data);
            let p = v.as_mut_ptr();
            let len = v.len();
            let cap = v.capacity();

            let bytes = unsafe { Vec::from_raw_parts(p as *mut i8, len, cap) };
            lua::pushbytes(l, &bytes)
        },
        Err(err) => {
            crate::overlay::lua::luawarn!(l, "Error while getting zip file content: {}", err);
            lua::pushnil(l);
        },
    }

    return 1;
}
