// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT

/*** RST
mumble-link
===========

.. lua:module:: mumble-link

.. code-block:: lua

    local ml = require 'mumble-link'

The :lua:mod:`mumble-link` module allows access to the MumbleLink shared memory
data from Guild Wars 2. The shared memory is read on each function call.

Functions
---------
*/
use crate::lua;
use crate::lua::lua_State;
use crate::lua::luaL_Reg;
use crate::lua::luaL_Reg_list;

use std::sync::{Arc,Weak,Mutex};
use std::mem::ManuallyDrop;

struct MlLuaState {
    ml: Weak<crate::ml::MumbleLink>,
}

static ML: Mutex<MlLuaState> = Mutex::new(MlLuaState{
    ml: Weak::new(),
});

pub fn set_ml(ml: Weak<crate::ml::MumbleLink>) {
    ML.lock().unwrap().ml = ml;
}

const ML_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"version"               , version,
    c"tick"                  , tick,
    c"avatarposition"        , avatar_position,
    c"avatarfront"           , avatar_front,
    c"avatartop"             , avatar_top,
    c"name"                  , name,
    c"cameraposition"        , camera_position,
    c"camerafront"           , camera_front,
    c"cameratop"             , camera_top,
};

const ID_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"name"          , identity_name,
    c"profession"    , identity_profession,
    c"professionname", identity_profession_name,
    c"spec"          , identity_spec,
    c"race"          , identity_race,
    c"racename"      , identity_race_name,
    c"mapid"         , identity_map_id,
    c"worldid"       , identity_world_id,
    c"teamcolorid"   , identity_team_color_id,
    c"commander"     , identity_commander,
    c"fov"           , identity_fov,
    c"uisz"          , identity_uisz,
    c"uiszname"      , identity_uisz_name,
};

const CONTEXT_FUNCS: &[luaL_Reg] = luaL_Reg_list!{
    c"serveraddress"  , context_server_address,
    c"mapid"          , context_map_id,
    c"maptype"        , context_map_type,
    c"maptypename"    , context_map_type_name,
    c"shardid"        , context_shard_id,
    c"instance"       , context_instance,
    c"buildid"        , context_build_id,
    c"uistate"        , context_ui_state,
    c"compasswidth"   , context_compass_width,
    c"compassheight"  , context_compass_height,
    c"compassrotation", context_compass_rotation,
    c"playerposition" , context_player_position,
    c"mapcenter"      , context_map_center,
    c"mapscale"       , context_map_scale,
    c"processid"      , context_process_id,
    c"mount"          , context_mount,
    c"mountname"      , context_mount_name,
};

pub unsafe extern "C" fn open_module(l: &lua_State) -> i32 {
    // store the MumbleLink weak reference in up values
    // so the Lua calls don't require a mutex lock every time
    let ml_ptr = Weak::into_raw(ML.lock().unwrap().ml.clone());

    lua::newtable(l);
    unsafe { lua::pushlightuserdata(l, ml_ptr as *const std::ffi::c_void); }
    lua::L::setfuncs(l, ML_FUNCS, 1);

    // identity
    lua::newtable(l);
    unsafe { lua::pushlightuserdata(l, ml_ptr as *const std::ffi::c_void); }
    lua::L::setfuncs(l, ID_FUNCS, 1);
    lua::setfield(l, -2, "identity");

    // context
    lua::newtable(l);
    unsafe { lua::pushlightuserdata(l, ml_ptr as *const std::ffi::c_void); }
    lua::L::setfuncs(l, CONTEXT_FUNCS, 1);
    lua::setfield(l, -2, "context");

    return 1;
}

// returns an upgraded MumbleLink reference from the weak that was stored in
// upvalues above
fn get_ml_upvalue(l: &lua_State) -> Arc<crate::ml::MumbleLink> {
    let ml_ptr: *const crate::ml::MumbleLink = unsafe {
        std::mem::transmute(lua::touserdata(l, lua::LUA_REGISTRYINDEX - 1)) // up value 1
    };
    
    let ml_weak = ManuallyDrop::new(unsafe { Weak::from_raw(ml_ptr) } );

    ml_weak.upgrade().unwrap()
}

/*** RST
.. lua:function:: version()

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn version(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.version() as i64);

    return 1;
}

/*** RST
.. lua:function:: tick()

    The current game tick. This is incremented roughly every frame and can be
    used to determine if the MumbleLink information is being actively updated
    by the game.

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn tick(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.tick() as i64);

    return 1;
}

/*** RST
.. lua:function:: avatarposition()

    The player's current position in the game world in GW2.

    .. note::
        
        This is the position in map units using meters and is represented in a
        rendering fashion. X is east/west, Y is elevation (up/down), and Z is
        north/south.

    .. code-block:: lua
        :caption: Example

        x, y, z = ml.avatarposition()

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn avatar_position(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let ap = ml.avatar_position();

    lua::pushnumber(l, ap.x as f64);
    lua::pushnumber(l, ap.y as f64);
    lua::pushnumber(l, ap.z as f64);
    
    return 3;
}

/*** RST
.. lua:function:: avatarfront()

    A vector pointing towards the direction the player is facing in the GW2 world.

    See the note on :lua:func:`avatarposition` about GW2 coordinates.

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn avatar_front(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let af = ml.avatar_front();

    lua::pushnumber(l, af.x as f64);
    lua::pushnumber(l, af.y as f64);
    lua::pushnumber(l, af.z as f64);

    return 3;
}

/*** RST
.. lua:function:: avatartop()

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn avatar_top(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let at = ml.avatar_top();

    lua::pushnumber(l, at.x as f64);
    lua::pushnumber(l, at.y as f64);
    lua::pushnumber(l, at.z as f64);

    return 3;
}

/*** RST
.. lua:function:: name()

    The GW2 MumbleLink name.

    This should always return ``"Guild Wars 2"``.

    .. seealso::
        For the character's name, see :lua:func:`identity.name`.

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushstring(l, &ml.name());

    return 1;
}

/*** RST
.. lua:function:: cameraposition()

    The current camera position in the game world in GW2.

    .. note::
        
        This is the position in map units using meters and is represented in a
        rendering fashion. X is east/west, Y is elevation (up/down), and Z is
        north/south.

    .. code-block:: lua
        :caption: Example

        x, y, z = ml.cameraposition()

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn camera_position(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let p = ml.camera_position();

    lua::pushnumber(l, p.x as f64);
    lua::pushnumber(l, p.y as f64);
    lua::pushnumber(l, p.z as f64);

    return 3;
}

/*** RST
.. lua:function:: camerafront()

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn camera_front(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let p = ml.camera_front();

    lua::pushnumber(l, p.x as f64);
    lua::pushnumber(l, p.y as f64);
    lua::pushnumber(l, p.z as f64);

    return 3;
}

/*** RST
.. lua:function:: cameratop()

    :returns: 3 numbers.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn camera_top(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    let p = ml.camera_top();

    lua::pushnumber(l, p.x as f64);
    lua::pushnumber(l, p.y as f64);
    lua::pushnumber(l, p.z as f64);

    return 3;
}

/*** RST

Identity
~~~~~~~~

The following values are parsed from the ``identity`` value of the MumbleLink
data.

The functions below are located on a table named ``identity`` within this module.

For example:

.. code-block:: lua
    
    local ml = require 'mumble-link'

    local charname = ml.identity.name()

.. important::
    The :lua:mod:`mumble-link` module will only parse the identity JSON once per
    :lua:func:`tick`, however module authors should still take care to only call
    these functions when necessary.

.. |identity_parse_note| replace:: This function will return ``nil`` if the
    MumbleLink identity data does not contain valid JSON.

.. lua:currentmodule:: mumble-link.identity

.. lua:function:: name()

    Returns the GW2 character name.

    :rtype: string

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_name() {
        Some(nm) => lua::pushstring(l, &nm),
        None     => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: profession()
  
    Returns the GW2 character profession ID.

    +-------+--------------+
    | Value | Description  |
    +=======+==============+
    | 1     | Guardian     |
    +-------+--------------+
    | 2     | Warrior      |
    +-------+--------------+
    | 3     | Engineer     |
    +-------+--------------+
    | 4     | Ranger       |
    +-------+--------------+
    | 5     | Thief        |
    +-------+--------------+
    | 6     | Elementalist |
    +-------+--------------+
    | 7     | Mesmer       |
    +-------+--------------+
    | 8     | Necromancer  |
    +-------+--------------+
    | 9     | Revenant     |
    +-------+--------------+

    .. seealso::
        Function :lua:func:`professionname`:
            Returns the profession as a string instead of the ID.

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_profession(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_profession() {
        Some(prof) => lua::pushinteger(l, prof),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: professionname()

    Returns the GW2 character profession name.

    One of:

    * guardian
    * warrior
    * engineer
    * ranger
    * thief
    * elementalist
    * mesmer
    * necromancer
    * revenant

    :rtype: string

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_profession_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    if let Some(prof) = ml.identity_profession() {
        match prof {
            1 => lua::pushstring(l, "guardian"    ),
            2 => lua::pushstring(l, "warrior"     ),
            3 => lua::pushstring(l, "engineer"    ),
            4 => lua::pushstring(l, "ranger"      ),
            5 => lua::pushstring(l, "thief"       ),
            6 => lua::pushstring(l, "elementalist"),
            7 => lua::pushstring(l, "mesmer"      ),
            8 => lua::pushstring(l, "necromancer" ),
            9 => lua::pushstring(l, "revenant"   ),
            _ => lua::pushstring(l, "unknown"     ),
        }
    } else {
        lua::pushnil(l);
    }

    return 1;
}

/*** RST
.. lua:function:: spec()
    
    Returns the GW2 character's third specialization ID, or 0 if none.

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_spec(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_spec() {
        Some(spec) => lua::pushinteger(l, spec),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: race()
    
    Returns the GW2 character's race ID.

    +-------+-------------+
    | Value | Description |
    +=======+=============+
    | 0     | Asura       |
    +-------+-------------+
    | 1     | Charr       |
    +-------+-------------+
    | 2     | Human       |
    +-------+-------------+
    | 3     | Norn        |
    +-------+-------------+
    | 4     | Sylvari     |
    +-------+-------------+

    .. seealso::
        Function :lua:func:`racename`:
            This returns the race as a string instead of the ID number.

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_race(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_race() {
        Some(race) => lua::pushinteger(l, race),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: racename()

    Returns the GW2 character's race as a string.

    One of: asura, charr, human, norn, or sylvari.

    :rtype: string

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_race_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    if let Some(race) = ml.identity_race() {
        match race {
            0 => lua::pushstring(l, "asura"),
            1 => lua::pushstring(l, "charr"),
            2 => lua::pushstring(l, "human"),
            3 => lua::pushstring(l, "norn"),
            4 => lua::pushstring(l, "sylvari"),
            _ => lua::pushstring(l, "unknown"),
        }
    } else {
        lua::pushnil(l);
    }

    return 1;
}

/*** RST
.. lua:function:: mapid()

    Returns the current Map ID.

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. seealso::
        The context value :lua:func:`mumble-link.context.mapid` should be preferred over this.

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_map_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_map_id() {
        Some(mapid) => lua::pushinteger(l, mapid),
        None        => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: worldid()

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_world_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_world_id() {
        Some(worldid) => lua::pushinteger(l, worldid),
        None          => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: teamcolorid()

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_team_color_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_team_color_id() {
        Some(colorid) => lua::pushinteger(l, colorid),
        None          => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: commander()

    Returns ``true`` if the player has a commander tag active or ``false`` if not.

    :rtype: boolean

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_commander(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_commander() {
        Some(comm) => lua::pushboolean(l, comm),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: fov()
    
    Returns the vertical field-of-view.

    :rtype: number

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_fov(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_fov() {
        Some(fov) => lua::pushnumber(l, fov),
        None      => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: uisz()

    Returns a value indicating the GW2 UI size:

    +-------+-------------+
    | Value | Description |
    +=======+=============+
    | 0     | Small       |
    +-------+-------------+
    | 1     | Normal      |
    +-------+-------------+
    | 2     | Large       |
    +-------+-------------+
    | 3     | Larger      |
    +-------+-------------+

    :rtype: integer

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_uisz(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    match ml.identity_uisz() {
        Some(uisz) => lua::pushinteger(l, uisz),
        None       => lua::pushnil(l),
    }

    return 1;
}

/*** RST
.. lua:function:: uiszname()
    
    Returns the GW2 UI Size:

    * small
    * normal
    * large
    * larger

    :rtype: string

    .. note::
        |identity_parse_note|

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn identity_uisz_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    if let Some(uisz) = ml.identity_uisz() {
        match uisz {
            0 => lua::pushstring(l, "small"),
            1 => lua::pushstring(l, "normal"),
            2 => lua::pushstring(l, "large"),
            3 => lua::pushstring(l, "larger"),
            _ => lua::pushstring(l, "unknown"),
        }
    } else {
        lua::pushnil(l);
    }

    return 1;
}

/*** RST
Context
~~~~~~~

The following values are parsed from the ``context`` struct of the MumbleLink
data.

The functions below are located on a table named ``context`` within this module.

For example:

.. code-block:: lua
    
    local ml = require 'mumble-link'

    local buildid = ml.context.buildid()


.. lua:currentmodule:: mumble-link.context

.. lua:function:: serveraddress()

    The ipv4 or ipv6 address of the map server.

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_server_address(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushstring(l, &ml.context_server_address());

    return 1;
}

/*** RST
.. lua:function:: mapid()

    Returns the current GW2 map ID.

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_map_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.map_id as i64);

    return 1;
}

/*** RST
.. lua:function:: maptype()

    Returns the current GW2 map type ID.

    +-------+------------------------------------------------------------------+
    | Value | Description                                                      |
    +=======+==================================================================+
    | 0     | Redirect or when the MubleLink data has not been initialized     |
    |       | by the game yet                                                  |
    +-------+------------------------------------------------------------------+
    | 1     | Character creation (this value is never actually returned, the   |
    |       | MumbleLink data is not updated until a character is logged in)   |
    +-------+------------------------------------------------------------------+
    | 2     | Competitive PvP                                                  |
    +-------+------------------------------------------------------------------+
    | 3     | GvG                                                              |
    +-------+------------------------------------------------------------------+
    | 4     | Instance                                                         |
    +-------+------------------------------------------------------------------+
    | 5     | Public (open world maps, etc.)                                   |
    +-------+------------------------------------------------------------------+
    | 6     | Tutorial                                                         |
    +-------+------------------------------------------------------------------+
    | 7     | Tournament                                                       |
    +-------+------------------------------------------------------------------+
    | 8     | User Tournament                                                  |
    +-------+------------------------------------------------------------------+
    | 9     | Eternal Battlegrounds                                            |
    +-------+------------------------------------------------------------------+
    | 10    | Blue Borderlands                                                 |
    +-------+------------------------------------------------------------------+
    | 11    | Green Borderlands                                                |
    +-------+------------------------------------------------------------------+
    | 12    | Red Borderlands                                                  |
    +-------+------------------------------------------------------------------+
    | 13    | WvW Reward                                                       |
    +-------+------------------------------------------------------------------+
    | 14    | Obsidian Sanctum                                                 |
    +-------+------------------------------------------------------------------+
    | 15    | Edge of the Mists                                                |
    +-------+------------------------------------------------------------------+
    | 16    | Public Mini                                                      |
    +-------+------------------------------------------------------------------+
    | 17    | Big Battle                                                       |
    +-------+------------------------------------------------------------------+
    | 18    | Armistice Bastion                                                |
    +-------+------------------------------------------------------------------+

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_map_type(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.map_type as i64);

    return 1;
}

/*** RST
.. lua:function:: maptypename()

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_map_type_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushstring(l, 
        match ml.gw2_ml.context.map_type {
            0 => "redirect",
            1 => "character-creation",
            2 => "pvp",
            3 => "gvg",
            4 => "instance",
            5 => "public",
            6 => "tutorial",
            7 => "tournament",
            8 => "user-tournament",
            9 => "eternal-battlegrounds",
            10 => "blue-borderlands",
            11 => "green-borderlands",
            12 => "red-borderlands",
            13 => "wvw-reward",
            14 => "obsidian-sanctum",
            15 => "edge-of-the-mists",
            16 => "public-mini",
            17 => "big-battle",
            18 => "armistice-bastion",
            _ => "unknown",
        }
    );

    return 1;
}

/*** RST
.. lua:function:: shardid()

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_shard_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.shard_id as i64);

    return 1;
}

/*** RST
.. lua:function:: instance()

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_instance(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.instance as i64);

    return 1;
}

/*** RST
.. lua:function:: buildid()
    
    The GW2 Build ID.

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_build_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.build_id as i64);

    return 1;
}

/*** RST
.. lua:function:: uistate()

    A bitmask indicating various states of the GW2 UI:

    +-----+--------------------------------------------------------------------+
    | Bit | Description                                                        |
    +=====+====================================================================+
    | 1   | Map is open.                                                       |
    +-----+--------------------------------------------------------------------+
    | 2   | Compass (mini-map) is located at the top left. If not, lower left. |
    +-----+--------------------------------------------------------------------+
    | 3   | Compass (mini-map) has rotation enabled.                           |
    +-----+--------------------------------------------------------------------+
    | 4   | GW2 window has focus.                                              |
    +-----+--------------------------------------------------------------------+
    | 5   | GW2 is in a competitive game mode.                                 |
    +-----+--------------------------------------------------------------------+
    | 6   | A GW2 textbox has focus.                                           |
    +-----+--------------------------------------------------------------------+
    | 7   | Player is in combat.                                               |
    +-----+--------------------------------------------------------------------+

    :rtype: integer

    .. code-block:: lua
        :caption: Example

        local ml = require 'mumble-link'

        if ml.uistate() & 8 then
            -- window has focus
            ...
        end

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_ui_state(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.ui_state as i64);

    return 1;
}

/*** RST
.. lua:function:: compasswidth()

    The width in pixels of the compass (mini-map).

    :rtype: integer

    .. versionhistory::
        :0.3.0: added
*/
unsafe extern "C" fn context_compass_width(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.compass_width as i64);

    return 1;
}

/*** RST
.. lua:function:: compassheight()

    The height in pixels of the compass (mini-map).

    :rtype: integer

    .. versionhistory::
        :0.3.0: added
*/
unsafe extern "C" fn context_compass_height(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.compass_height as i64);

    return 1;
}

/*** RST
.. lua:function:: compassrotation()

    The compass (mini-map) rotation, in radians.

    :rtype: number

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_compass_rotation(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushnumber(l, ml.gw2_ml.context.compass_rotation as f64);

    return 1
}

/*** RST
.. lua:function:: playerposition()

    The player's current position (x,y) in continent coordinates.

    :returns: 2 numbers

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_player_position(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushnumber(l, ml.gw2_ml.context.player_x as f64);
    lua::pushnumber(l, ml.gw2_ml.context.player_y as f64);

    return 2;
}

/*** RST
.. lua:function:: mapcenter()

    The center of the (mini)map in continent coordinates.

    :returns: 2 numbers

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_map_center(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushnumber(l, ml.gw2_ml.context.map_center_x as f64);
    lua::pushnumber(l, ml.gw2_ml.context.map_center_y as f64);

    return 2;
}

/*** RST
.. lua:function:: mapscale()

    :rtype: number

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_map_scale(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushnumber(l, ml.gw2_ml.context.map_scale as f64);

    return 1;
}

/*** RST
.. lua:function:: processid()

    The GW2 process ID.

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_process_id(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.process_id as i64);

    return 1;
}

/*** RST
.. lua:function:: mount()
    
    Returns the mount ID:

    +-------+------------------------+
    | Value | Description            |
    +=======+========================+
    | 0     | Player not on a mount. |
    +-------+------------------------+
    | 1     | Jackal                 |
    +-------+------------------------+
    | 2     | Griffon                |
    +-------+------------------------+
    | 3     | Springer               |
    +-------+------------------------+
    | 4     | Skimmer                |
    +-------+------------------------+
    | 5     | Raptor                 |
    +-------+------------------------+
    | 6     | Roller Beetle          |
    +-------+------------------------+
    | 7     | Warclaw                |
    +-------+------------------------+
    | 8     | Skyscale               |
    +-------+------------------------+
    | 9     | Skiff                  |
    +-------+------------------------+
    | 10    | Seige Turtle           |
    +-------+------------------------+

    .. seealso::
        :lua:func:`mountname` returns this value as a string instead of the ID.

    :rtype: integer

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_mount(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushinteger(l, ml.gw2_ml.context.mount_index as i64);

    return 1;
}

/*** RST
.. lua:function:: mountname()

    Returns the current mount:

    * none
    * jackal
    * griffon
    * springer
    * skimmer
    * raptor
    * roller-beetle
    * warclaw
    * skyscale
    * skiff
    * seige-turtle

    :rtype: string

    .. versionhistory::
        :0.3.0: Added
*/
unsafe extern "C" fn context_mount_name(l: &lua_State) -> i32 {
    let ml = get_ml_upvalue(l);

    lua::pushstring(l, match ml.gw2_ml.context.mount_index {
        0 => "none",
        1 => "jackal",
        2 => "griffon",
        3 => "springer",
        4 => "skimmer",
        5 => "raptor",
        6 => "roller-beetle",
        7 => "warclaw",
        8 => "skyscale",
        9 => "skiff",
        10 => "seige-turtle",
        _ => "unknown",
    });

    return 1;
}
