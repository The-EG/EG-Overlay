// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT


/*** RST
path
====

.. lua:module:: path

.. code:: lua

    local path = require 'path'

The :lua:mod:`path` module contains functions for analyzing and altering file
path strings.

It is essentially an interface to Rust's `std::path <https://doc.rust-lang.org/std/path>`_.
*/


use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::path::Path;

use crate::overlay::lua::luaerror;

const PATH_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"ancestors"    , ancestors,
    c"canonicalize" , canonicalize,
    c"components"   , components,
    c"endswith"     , ends_with,
    c"exists"       , exists,
    c"extension"    , extension,
    c"filename"     , file_name,
    c"filesteam"    , file_stem,
    c"hasroot"      , has_root,
    c"isabsolute"   , is_absolute,
    c"isdir"        , is_dir,
    c"isfile"       , is_file,
    c"isrelative"   , is_relative,
    c"issymlink"    , is_symlink,
    c"join"         , join,
    c"parent"       , parent,
    c"startswith"   , starts_with,
    c"stripprefix"  , strip_prefix,
    c"withextension", with_extension,
    c"withfilename" , with_file_name,
};

macro_rules! path_bool_func {
    ($name: tt) => {
        unsafe extern "C" fn $name(l: &lua_State) -> i32 {
            lua::checkargstring!(l, 1);

            let p = lua::tostring(l, 1).unwrap();
            let path = Path::new(&p);

            lua::pushboolean(l, path.$name());

            return 1;
        }
    }
}

macro_rules! path_opt_string_func {
    ($name: tt) => {
        unsafe extern "C" fn $name(l: &lua_State) -> i32 {
            lua::checkargstring!(l, 1);

            let p = lua::tostring(l, 1).unwrap();
            let path = Path::new(&p);

            match path.$name() {
                Some(s) => lua::pushstring(l, &s.to_string_lossy()),
                None => lua::pushnil(l),
            }

            return 1;
        }
    }
}

pub fn init() {
    crate::lua_manager::add_module_opener("path", Some(open_module));
}

unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    lua::newtable(l);
    lua::L::setfuncs(l, PATH_FUNCS, 0);

    return 1;
}

/*** RST
Functions
---------

.. lua:function:: ancestors(path)

    Return a list of ``path`` and its ancestors.

    :rtype: sequence

    .. code-block:: lua
        :caption: Example

        local path = require 'path'

        -- a is:
        --  '../some/path/foo/bar'
        --  '../some/path/foo'
        --  '../some/path'
        --  '../some'
        --  '..'
        --  ''
        for i,a in ipairs(path.ancestors('../some/path/foo/bar')) do

        end

    .. seealso::

        Rust's `std::path::Path.ancestors <https://doc.rust-lang.org/std/path/struct.Path.html#method.ancestors>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn ancestors(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);

    let mut table_i = 1;
    lua::newtable(l);
    for a in path.ancestors() {
        lua::pushstring(l, &a.to_string_lossy());
        lua::seti(l, -2, table_i);
        table_i += 1;
    }

    return 1;
}

/*** RST
.. lua:function:: canonicalize(path)

    Return the canonical, absolute form of ``path`` with all intermediate
    components and symbolic links resolved.

    If ``path`` is invalid, ``nil`` is returned instead.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.canonicalize <https://doc.rust-lang.org/std/path/struct.Path.html#method.canonicalize>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn canonicalize(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);

    match path.canonicalize() {
        Ok(r) => lua::pushstring(l, &r.to_string_lossy()),
        Err(err) => {
            luaerror!(l, "error in path.canonicalize: {}", err);
            lua::pushnil(l);
        }
    }

    return 1;
}

/*** RST
.. lua:function:: components(path)

    Return a list of components that make up ``path``.

    When parsing the path, there is a small amount of normalization:

    Repeated separators are ignored, so ``a/b`` and ``a//b`` both have ``a`` and
    ``b`` as components.

    Occurrences of ``.`` are normalized away, except if they are at the beginning
    of the path. For example, ``a/./b``, ``a/b/``, ``a/b/``. and ``a/b`` all have
    ``a`` and ``b`` as components, but ``./a/b`` starts with an additional
    ``cur-dir`` component.

    A trailing slash is normalized away, ``/a/b`` and ``/a/b/`` are equivalent.

    Note that no other normalization takes place; in particular, ``a/c`` and
    ``a/b/../c`` are distinct, to account for the possibility that ``b`` is a
    symbolic link (so its parent isn’t ``a``).

    Components are returned as a sequence of strings. The first string is the type
    of component, and any following strings are the path fragment(s) of the component.

    The component type will be one of the following values:

    +----------------------+---------------------------------------------------+
    | Value                | Description                                       |
    +======================+===================================================+
    | prefix-verbatim      | A verbatim prefix, e.g. ``\\?\\foo``.             |
    +----------------------+---------------------------------------------------+
    | prefix-verbatim-unc  | A verbatim prefix using Windows' Uniform Naming   |
    |                      | Convention, e.g. ``\\?\\UNC\server\share``.       |
    +----------------------+---------------------------------------------------+
    | prefix-verbatim-disk | A verbatim disk prefix, e.g. ``\\?\C:``.          |
    +----------------------+---------------------------------------------------+
    | prefix-device-ns     | A device namespace prefix, e.g. ``\\.\COM1``.     |
    +----------------------+---------------------------------------------------+
    | prefix-unc           | A Windows' Uniform Naming Convention prefix, e.g. |
    |                      | ``\\server\share``.                               |
    +----------------------+---------------------------------------------------+
    | prefix-disk          | A prefix for a given disk drive, e.g. ``C:``.     |
    +----------------------+---------------------------------------------------+
    | root-dir             | The root directory component, appears after any   |
    |                      | prefix an before anything else.                   |
    +----------------------+---------------------------------------------------+
    | cur-dir              | A reference to the current directory, i.e. ``.``. |
    +----------------------+---------------------------------------------------+
    | parent-dir           | A reference to the parent directory, i.e. ``..``. |
    +----------------------+---------------------------------------------------+
    | normal               | A normal path component, e.g. ``a`` and ``b`` in  |
    |                      | ``a/b``.                                          |
    +----------------------+---------------------------------------------------+

    :rtype: sequence

    .. code-block:: lua
        :caption: Example

        local path = require 'path'

        -- c is:
        --  {'parent-dir'}
        --  {'normal', 'foo'}
        --  {'normal', 'bar'}
        for i,c in ipairs(path.components('../foo/bar')) do

        end

    .. seealso::

        Rust's `std::path::Path.components <https://doc.rust-lang.org/std/path/struct.Path.html#method.components>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn components(l: &lua_State) -> i32 {
    use std::path::{Component, Prefix};

    lua::checkargstring!(l, 1);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);

    macro_rules! push_component {
        ($typ: literal) => {{
            lua::pushstring(l, $typ);
            lua::seti(l, -2, 1);
        }};
        ($typ: literal, $val: expr) => {{
            lua::pushstring(l, $typ);
            lua::seti(l, -2, 1);
            lua::pushstring(l, $val);
            lua::seti(l, -2, 2);
        }};
        ($typ: literal, $val1: expr, $val2: expr) => {{
            push_component!($typ, $val1);
            lua::pushstring(l, $val2);
            lua::seti(l, -2, 3);
        }};
    }

    lua::newtable(l);
    let mut ci = 1;
    for c in path.components() {
        lua::newtable(l);
        match c {
            Component::Prefix(prefix) => {
                match prefix.kind() {
                    Prefix::Verbatim(p)        => push_component!("prefix-verbatim"     , &p.to_string_lossy()),
                    Prefix::VerbatimUNC(p1,p2) => push_component!("prefix-verbatim-unc" , &p1.to_string_lossy(), &p2.to_string_lossy()),
                    Prefix::VerbatimDisk(p)    => push_component!("prefix-verbatim-disk", str::from_utf8(&[p]).unwrap()),
                    Prefix::DeviceNS(p)        => push_component!("prefix-device-ns"    , &p.to_string_lossy()),
                    Prefix::UNC(p1,p2)         => push_component!("prefix-unc"          , &p1.to_string_lossy(), &p2.to_string_lossy()),
                    Prefix::Disk(p)            => push_component!("prefix-disk"         , str::from_utf8(&[p]).unwrap()),
                }
            },
            Component::RootDir   => push_component!("root-dir"),
            Component::CurDir    => push_component!("cur-dir"),
            Component::ParentDir => push_component!("parent-dir"),
            Component::Normal(p) => push_component!("normal", &p.to_string_lossy()),
        }

        lua::seti(l, -2, ci);
        ci += 1;
    }

    return 1;
}

/*** RST
.. lua:function:: endswith(path, child)

    Returns ``true`` if ``child`` is a suffix of ``path``.

    Only considers whole path components to match.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.ends_with <https://doc.rust-lang.org/std/path/struct.Path.html#method.ends_with>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn ends_with(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);
    let child = lua::tostring(l, 2).unwrap();

    lua::pushboolean(l, path.ends_with(&child));

    return 1;
}

/*** RST
.. lua:function:: exists(path)

    Returns ``true`` if ``path`` points at an existing entity.

    If an error occurs, information is logged and ``nil`` is returned instead.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.try_exists <https://doc.rust-lang.org/std/path/struct.Path.html#method.try_exists>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn exists(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);

    match path.try_exists() {
        Ok(r) => lua::pushboolean(l, r),
        Err(e) => {
            luaerror!(l, "Error while checking {}: {}", p, e);
            lua::pushnil(l);
        }
    }

    return 1;
}



/*** RST
.. lua:function:: extension(path)

    Return the extension (without the leadin dot) of ``path``, if possible.

    The extension is:

    - ``nil`` if there is no file name
    - ``nil`` if there is no embedded ``.``
    - ``nil`` if the file name begins with ``.`` and no other ``.`` within
    - Otherwise, the portion of the file name after the final ``.``

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.extension <https://doc.rust-lang.org/std/path/struct.Path.html#method.extension>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_opt_string_func!(extension);

/*** RST
.. lua:function:: filename(path)

    Return the final component of ``path``, if there is one.

    If the path is a normal file, this is the file name. If it’s the path of a
    directory, this is the directory name.

    Returns ``nil`` if the path terminates in ``..``.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.file_name <https://doc.rust-lang.org/std/path/struct.Path.html#method.file_name>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_opt_string_func!(file_name);

/*** RST
.. lua:function:: filestem(path)

    Return the stem (non-extension) portion of the file name in ``path``.

    The stem is:

    - ``nil`` if there is no file name
    - The entire file name if there is no embedded ``.``
    - The entire file name if the file name begins with ``.`` and has no other ``.`` within
    - Otherwise, the portion of the file name before the final ``.``

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.file_stem <https://doc.rust-lang.org/std/path/struct.Path.html#method.file_stem>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_opt_string_func!(file_stem);

/*** RST
.. lua:function:: hasroot(path)

    Return ``true`` if ``path`` has a root component.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.has_root <https://doc.rust-lang.org/std/path/struct.Path.html#method.has_root>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(has_root);

/*** RST
.. lua:function:: isabsolute(path)

    Return ``true`` if ``path`` is absolute, i.e., it is independent of the current
    directory.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.is_absolute <https://doc.rust-lang.org/std/path/struct.Path.html#method.is_absolute>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(is_absolute);

/*** RST
.. lua:function:: isdir(path)

    Return ``true`` if ``path`` exists and is pointing at a directory.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.is_dir <https://doc.rust-lang.org/std/path/struct.Path.html#method.is_dir>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(is_dir);

/*** RST
.. lua:function:: isfile(path)

    Return ``true`` if ``path`` exists and is pointing at a regular file.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.is_file <https://doc.rust-lang.org/std/path/struct.Path.html#method.is_file>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(is_file);

/*** RST
.. lua:function:: isrelative(path)

    Return ``true`` if ``path`` is relative.

    :rtype:: boolean

    .. seealso::

        Rust's `std::path::Path.is_relative <https://doc.rust-lang.org/std/path/struct.Path.html#method.is_relative>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(is_relative);

/*** RST
.. lua:function:: issymlink(path)

    Return ``true`` if ``path`` exists and is pointing at a symbolic link.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.is_symlink <https://doc.rust-lang.org/std/path/struct.Path.html#method.is_symlink>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_bool_func!(is_symlink);

/*** RST
.. lua:function:: join(path1, path2)

    Return a new path from ``path2`` adjoined to ``path1``.

    If ``path2`` is absolute, it will be returned instead.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.join <https://doc.rust-lang.org/std/path/struct.Path.html#method.join>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn join(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p1 = lua::tostring(l, 1).unwrap();
    let path1 = Path::new(&p1);
    let p2 = lua::tostring(l, 2).unwrap();
    let path2 = Path::new(&p2);

    let pb = path1.join(path2);

    lua::pushstring(l, &pb.to_string_lossy());

    return 1;
}

/*** RST
.. lua:function:: parent(path)

    Return ``path`` without its final component, if there is one.

    Returns ``''`` for relative paths with one component, or ``nil`` if the path
    terminates in a root or prefix, or if it's an empty string.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.parent <https://doc.rust-lang.org/std/path/struct.Path.html#method.parent>`_.

    .. versionhistory::
        :0.3.0: Added
*/
path_opt_string_func!(parent);

/*** RST
.. lua:function:: startswith(path, base)

    Returns ``true`` if ``base`` if a prefix of ``path``.

    Only considers whole path components to match.

    :rtype: boolean

    .. seealso::

        Rust's `std::path::Path.starts_with <https://doc.rust-lang.org/std/path/struct.Path.html#method.starts_with>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn starts_with(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);
    let base = lua::tostring(l, 2).unwrap();

    lua::pushboolean(l, path.starts_with(&base));

    return 1;
}

/*** RST
.. lua:function:: stripprefix(path, base)

    Returns a path that when joined onto base yields ``path``.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.strip_prefix <https://doc.rust-lang.org/std/path/struct.Path.html#method.strip_prefix>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn strip_prefix(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);
    let base = lua::tostring(l, 2).unwrap();

    match path.strip_prefix(base) {
        Ok(p) => lua::pushstring(l, &p.to_string_lossy()),
        Err(_) => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: withextension(path, extension)

    Return ``path`` but with the given ``extension``.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.with_extension <https://doc.rust-lang.org/std/path/struct.Path.html#method.with_extension>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn with_extension(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);
    let ext = lua::tostring(l, 2).unwrap();

    let pb = path.with_extension(&ext);

    lua::pushstring(l, &pb.to_string_lossy());

    return 1;
}

/*** RST
.. lua:function:: withfilename(path, filename)

    Return ``path`` but with the given ``filename``.

    If ``path`` already ends in a filename, it will be replaced.

    :rtype: string

    .. seealso::

        Rust's `std::path::Path.with_file_name <https://doc.rust-lang.org/std/path/struct.Path.html#method.with_file_name>`_.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn with_file_name(l: &lua_State) -> i32 {
    lua::checkargstring!(l, 1);
    lua::checkargstring!(l, 2);

    let p = lua::tostring(l, 1).unwrap();
    let path = Path::new(&p);
    let f = lua::tostring(l, 2).unwrap();

    let pb = path.with_file_name(&f);

    lua::pushstring(l, &pb.to_string_lossy());

    return 1;
}
