/*** RST
mumble-link
===========

.. |identity warning| replace:: Retrieving this value requires parsing the JSON
    text of the identity value from MumbleLink. This is a potentially resource
    intensive opperation. EG-Overlay will cache this parsed JSON and only
    reparse when :lua:data:`mumble-link.tick` has changed.

.. |position meters| replace:: Position coordinates returned by MumbleLink are
    in meters but Guild Wars 2 uses inches internally. You may need to convert
    these values to use them, although some other applications, like marker packs,
    use meters as well.

.. |coordinate order| replace:: Position coordinates are presented from a
    rendering perspective, which means the x coordinate is from left to right
    (east/west), the y is up and down (altitude) and z is depth (north/south). 

.. lua:module:: mumble-link

.. code:: lua
    
    local ml = require 'mumble-link'

The :lua:mod:`mumble-link` module allows access to the MumbleLink shared memory
data from Guild Wars 2. The shared memory is read on each attribute access.

*/

#include <windows.h>
#include "mumble-link.h"
#include "utils.h"
#include "logging/logger.h"
#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>
#include <jansson.h>
#include "lua-manager.h"

typedef struct gw2_ml_t {
    uint32_t version;
    uint32_t tick;
    
    struct {
        float x;
        float y;
        float z;
    } avatar_position;

    struct {
        float x;
        float y;
        float z;
    } avatar_front;

    struct {
        float x;
        float y;
        float z;
    } avatar_top;

    wchar_t name[256];
    
    struct {
        float x;
        float y;
        float z;
    } camera_position;

    struct {
        float x;
        float y;
        float z;
    } camera_front;

    struct {
        float x;
        float y;
        float z;
    } camera_top;

    wchar_t identity[256];
    uint32_t context_len;

    struct {
        unsigned char server_address[28];
        uint32_t map_id;
        uint32_t map_type;
        uint32_t shard_id;
        uint32_t instance;
        uint32_t build_id;
        uint32_t ui_state;
        uint16_t compass_width;
        uint16_t compass_height;
        float compass_rotation;
        float player_x;
        float player_y;
        float map_center_x;
        float map_center_y;
        float map_scale;
        uint32_t process_id;
        uint8_t mouse_index;
    } context;
    wchar_t description[2048];
} gw2_ml_t;

typedef struct ml_t {
    HANDLE map_file;
    gw2_ml_t *gw2_ml;
    logger_t *log;

    uint32_t identity_tick;
    json_t *identity_cache;
} ml_t;

static ml_t *ml = NULL;

static void mumble_link_check_identity_cache();
int mumble_link_open_module(lua_State *L);

void mumble_link_init() {
    ml = egoverlay_calloc(1, sizeof(ml_t));
    ml->log = logger_get("mumble-link");

    ml->map_file = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(gw2_ml_t),
        "MumbleLink"
    );

    if (ml->map_file==NULL) {
        logger_error(ml->log, "Couldn't create MumbleLink shared file.");
        return;
    }

    ml->gw2_ml = (gw2_ml_t*) MapViewOfFile(ml->map_file, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(gw2_ml_t));

    if (ml->gw2_ml==NULL) {
        logger_error(ml->log, "Couldn't map MumbleLink shared file.");
        return;
    }

    logger_debug(ml->log,"MumbleLink memory mapped.");

    lua_manager_add_module_opener("mumble-link", &mumble_link_open_module);
}

uint32_t mumble_link_tick()    { return ml->gw2_ml->tick;    }
uint32_t mumble_link_version() { return ml->gw2_ml->version; }

size_t mumble_link_name(char *name, size_t max_size) {
    return WideCharToMultiByte(CP_UTF8, 0, ml->gw2_ml->name, -1, name, (int)max_size, NULL, NULL);
}

size_t mumble_link_identity(char *identity, size_t max_size) {
    return WideCharToMultiByte(CP_UTF8, 0, ml->gw2_ml->identity, -1, identity, (int)max_size, NULL, NULL);
}

json_t *parse_identity() {
    char ident_mb[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, ml->gw2_ml->identity, -1, ident_mb, 512, NULL, NULL);
    json_error_t error = {0};
    json_t *ident_json = json_loads(ident_mb, 0, &error);

    if (!ident_json) {
        //logger_error(ml->log, "failed to parse identity json: %s", error.text);
        return NULL;
    }

    return ident_json;
}

size_t mumble_link_character_name(char *name, size_t max_size) {
    mumble_link_check_identity_cache();
    if (ml->identity_cache==NULL || !json_is_object(ml->identity_cache)) return 0;   

    json_t *name_json = json_object_get(ml->identity_cache, "name");
    if (name_json==NULL) {
        return 0;
    }

    const char *name_str = json_string_value(name_json);
    size_t name_len = strlen(name_str);

    if (name_len>=max_size) {
        logger_error(ml->log, "Not enough room to store character name.");
        return 0;
    }

    memcpy(name, name_str, name_len);

    return name_len;
}

mumble_link_profession_t mumble_link_character_profression() {
    mumble_link_check_identity_cache();
    if (ml->identity_cache==NULL || !json_is_object(ml->identity_cache)) return 0;   

    json_t *prof_json = json_object_get(ml->identity_cache, "profession");
    if (prof_json==NULL) {
        return MUMBLE_LINK_PROFESSION_ERROR;
    }

    return json_integer_value(prof_json);
}

void mumble_link_avatar_position(float *x, float *y, float *z) {
    *x = ml->gw2_ml->avatar_position.x;
    *y = ml->gw2_ml->avatar_position.y;
    *z = ml->gw2_ml->avatar_position.z;
}

void mumble_link_camera_position(float *x, float *y, float *z) {
    *x = ml->gw2_ml->camera_position.x;
    *y = ml->gw2_ml->camera_position.y;
    *z = ml->gw2_ml->camera_position.z;
}

void mumble_link_camera_front(float *x, float *y, float *z) {
    *x = ml->gw2_ml->camera_front.x;
    *y = ml->gw2_ml->camera_front.y;
    *z = ml->gw2_ml->camera_front.z;
}

float mumble_link_fov()  {
    mumble_link_check_identity_cache();
    if (ml->identity_cache==NULL || !json_is_object(ml->identity_cache)) return 0.f;

    json_t *fov = json_object_get(ml->identity_cache, "fov");
    return json_number_value(fov);
}

void mumble_link_cleanup() {
    if (ml->identity_cache) json_decref(ml->identity_cache);
    if (ml->gw2_ml) UnmapViewOfFile(ml->gw2_ml);
    if (ml->map_file) CloseHandle(ml->map_file);
    egoverlay_free(ml);
}

static void mumble_link_check_identity_cache() {
    if (ml->identity_cache==NULL || ml->identity_tick!=ml->gw2_ml->tick) {
        if (ml->identity_cache) json_decref(ml->identity_cache);

        ml->identity_cache = parse_identity();
        ml->identity_tick = ml->gw2_ml->tick;
    }
}

static int mumble_link_lua_index(lua_State *L);
static int mumble_link_lua_new_index(lua_State *L);

static luaL_Reg ml_funcs[] = {
    "__index",    &mumble_link_lua_index,
    "__newindex", &mumble_link_lua_new_index,
    NULL,          NULL
};

int mumble_link_open_module(lua_State *L) {
    lua_newtable(L); // mumble_link

    lua_newtable(L); // meta table
    luaL_setfuncs(L, ml_funcs, 0);

    lua_setmetatable(L, -2);

    return 1;
}

int mumble_link_lua_tick(lua_State *L);
int mumble_link_lua_version(lua_State *L);
int mumble_link_lua_character_name(lua_State *L);
int mumble_link_lua_character_profession(lua_State *L);
int mumble_link_lua_character_specialization(lua_State *L);
int mumble_link_lua_character_race(lua_State *L);
int mumble_link_lua_avatar_position(lua_State *L);
int mumble_link_lua_avatar_front(lua_State *L);
int mumble_link_lua_avatar_top(lua_State *L);
int mumble_link_lua_camera_position(lua_State *L);
int mumble_link_lua_camera_front(lua_State *L);
int mumble_link_lua_camera_top(lua_State *L);
int mumble_link_lua_map_type(lua_State *L);
int mumble_link_lua_map_id(lua_State *L);
int mumble_link_lua_ui_state(lua_State *L);

int mumble_link_lua_index(lua_State *L) {
    const char *key = luaL_checkstring(L, 2);
    //size_t key_len = strlen(key);

    if      (strcmp(key, "tick"                    )==0) return mumble_link_lua_tick(L);    
    else if (strcmp(key, "version"                 )==0) return mumble_link_lua_version(L);
    else if (strcmp(key, "character_name"          )==0) return mumble_link_lua_character_name(L);
    else if (strcmp(key, "character_profession"    )==0) return mumble_link_lua_character_profession(L);
    else if (strcmp(key, "character_specialization")==0) return mumble_link_lua_character_specialization(L);
    else if (strcmp(key, "character_race"          )==0) return mumble_link_lua_character_race(L);
    else if (strcmp(key, "avatar_position"         )==0) return mumble_link_lua_avatar_position(L);
    else if (strcmp(key, "avatar_front"            )==0) return mumble_link_lua_avatar_front(L);
    else if (strcmp(key, "avatar_top"              )==0) return mumble_link_lua_avatar_top(L);
    else if (strcmp(key, "camera_position"         )==0) return mumble_link_lua_camera_position(L);
    else if (strcmp(key, "camera_front"            )==0) return mumble_link_lua_camera_front(L);
    else if (strcmp(key, "camera_top"              )==0) return mumble_link_lua_camera_top(L);
    else if (strcmp(key, "map_type"                )==0) return mumble_link_lua_map_type(L);
    else if (strcmp(key, "map_id"                  )==0) return mumble_link_lua_map_id(L);
    else if (strcmp(key, "ui_state"                )==0) return mumble_link_lua_ui_state(L);

    char *mod_name = lua_manager_get_lua_module_name(L);
    logger_warn(ml->log, "%s tried to read mumble_link.%s, does not exist.", mod_name, key);
    egoverlay_free(mod_name);

    return 0;
}

int mumble_link_lua_new_index(lua_State *L) {
    char *mod_name = lua_manager_get_lua_module_name(L);
    logger_warn(ml->log, "%s tried to assign to mumble_link!", mod_name);
    egoverlay_free(mod_name);

    return 0;
}

/*** RST
Attributes
----------

.. lua:data:: tick

    :type: integer

    The current game tick. This is incremented roughly every frame and can be used to determine if the MumbleLink information is being actively updated by the game.

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_tick(lua_State *L) {
    lua_pushinteger(L, mumble_link_tick());
    return 1;
}

/*** RST
.. lua:data:: version

    :type: integer
    
    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_version(lua_State *L) {
    lua_pushinteger(L, mumble_link_version());
    return 1;
}

/*** RST
.. lua:data:: character_name

    :type: string

    The name of the currently logged in character.

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_character_name(lua_State *L) {
    char name[128] = {0};
    mumble_link_character_name(name, 128);
    lua_pushstring(L, name);
    return 1;
}

/*** RST
.. lua:data:: character_profession

    :type: string

    The profession of the currently logged in character. One of:

    * Elementalist
    * Engineer
    * Guardian
    * Mesmer
    * Necromancer
    * Ranger
    * Thief
    * Warrior

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_character_profession(lua_State *L) {
    switch (mumble_link_character_profression()) {
        case MUMBLE_LINK_PROFESSION_ERROR:        lua_pushstring(L, "error");        break;
        case MUMBLE_LINK_PROFESSION_ELEMENTALIST: lua_pushstring(L, "Elementalist"); break;
        case MUMBLE_LINK_PROFESSION_ENGINEER:     lua_pushstring(L, "Engineer");     break;
        case MUMBLE_LINK_PROFESSION_GUARDIAN:     lua_pushstring(L, "Guardian");     break;
        case MUMBLE_LINK_PROFESSION_MESMER:       lua_pushstring(L, "Mesmer");       break;
        case MUMBLE_LINK_PROFESSION_NECROMANCER:  lua_pushstring(L, "Necromancer");  break;
        case MUMBLE_LINK_PROFESSION_RANGER:       lua_pushstring(L, "Ranger");       break;
        case MUMBLE_LINK_PROFESSION_THIEF:        lua_pushstring(L, "Thief");        break;
        case MUMBLE_LINK_PROFESSION_WARRIOR:      lua_pushstring(L, "Warrior");      break;
        default:                                  lua_pushstring(L, "error");        break;
    }
    return 1;
}

/*** RST
.. lua:data:: character_specialization
    
    :type: integer

    The specialization ID of the currently logged in character. See `the GW2 API <https://wiki.guildwars2.com/wiki/API:2/specializations>`_.

    .. note::
        Module authors can use the :lua:mod:`gw2.static` module instead of querying the GW2 API.

        See :overlay:dbtable:`gw2static.specializations` and :lua:func:`gw2.static.specialization`.

    .. warning::
        |identity warning|

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_character_specialization(lua_State *L) {
    mumble_link_check_identity_cache();
    if (ml->identity_cache==NULL || !json_is_object(ml->identity_cache)) return 0;

    json_t *spec_json = json_object_get(ml->identity_cache, "spec");
    if (spec_json==NULL) {
        return 0;
    }

    lua_pushinteger(L, json_integer_value(spec_json));
    return 1;
}

/*** RST
.. lua:data:: character_race

    :type: string

    The race of the currently logged in character. One of:

    * Asura
    * Charr
    * Human
    * Norn
    * Sylvari

    .. warning::
        |identity warning|
    
    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_character_race(lua_State *L) {
    mumble_link_check_identity_cache();
    if (ml->identity_cache==NULL || !json_is_object(ml->identity_cache)) return 0;

    json_t *race_json = json_object_get(ml->identity_cache, "race");
    if (race_json==NULL) {
        return 0;
    }

    int race = (int)json_integer_value(race_json);
    switch (race) {
    case 0:  lua_pushliteral(L, "Asura"  ); break;
    case 1:  lua_pushliteral(L, "Charr"  ); break;
    case 2:  lua_pushliteral(L, "Human"  ); break;
    case 3:  lua_pushliteral(L, "Norn"   ); break;
    case 4:  lua_pushliteral(L, "Sylvari"); break;
    default: lua_pushliteral(L, "unknown"); break;
    }

    return 1;
}

void push_ml_xyz(lua_State *L, float *vals) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, vals[0]);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, vals[1]);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, vals[2]);
    lua_setfield(L, -2, "z");
}

/*** RST
.. lua:data:: avatar_position

    :type: position

    The player's current position in map coordinates.

    .. note::
        |position meters|

        |coordinate order|

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_avatar_position(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->avatar_position);
    return 1;
}

/*** RST
.. lua:data:: avatar_front

    :type: position

    The direction the player's character is facing, as a vector.

    .. note::
        |coordinate order|
    
    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_avatar_front(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->avatar_front);
    return 1;
}

/*** RST
.. lua:data:: avatar_top

    :type: position

    .. warning::
        This is not populated by GW2 and will always return zeros.

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_avatar_top(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->avatar_top);
    return 1;
}

/*** RST
.. lua:data:: camera_position

    :type: position

    The current camera position in map coordinates.

    .. note::
        |position meters|

        |coordinate order|

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_camera_position(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->camera_position);
    return 1;
}

/*** RST
.. lua:data:: camera_front

    :type: position

    The direction the camera is facing, as a vector.

    .. note::
        |coordinate order|
    
    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_camera_front(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->camera_front);
    return 1;
}

/*** RST
.. lua:data:: camera_top

    :type: position

    .. warning::
        This is not populated by GW2 and will always return zeros.

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_camera_top(lua_State *L) {
    push_ml_xyz(L, (float*)&ml->gw2_ml->camera_top);
    return 1;
}

/*** RST
.. lua:data:: map_type

    :type: string

    The current map type. One of:

    * redirect
    * character-creation
    * competitive-pvp
    * gvg
    * instance
    * public
    * tournament
    * tutorial
    * user-tournament
    * eternal-battlegrounds
    * blue-borderlands
    * green-borderlands
    * red-borderlands
    * wvw-reward
    * obsidian-sanctum
    * edge-of-the-mists
    * public-mini
    * big-battle
    * armistice-bastion

    .. note::
        Even though some of the options above are `documented in the GW2 MumbleLink API <https://wiki.guildwars2.com/wiki/API:MumbleLink>`_ they will never be returned because the are now invalid.

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_map_type(lua_State *L) {
    switch (ml->gw2_ml->context.map_type) {
    case  0: lua_pushstring(L, "redirect");              break;
    case  1: lua_pushstring(L, "character-creation");    break;
    case  2: lua_pushstring(L, "competitive-pvp");       break;
    case  3: lua_pushstring(L, "gvg");                   break;
    case  4: lua_pushstring(L, "instance");              break;
    case  5: lua_pushstring(L, "public");                break;
    case  6: lua_pushstring(L, "tournament");            break;
    case  7: lua_pushstring(L, "tutorial");              break;
    case  8: lua_pushstring(L, "user-tournament");       break;
    case  9: lua_pushstring(L, "eternal-battlegrounds"); break;
    case 10: lua_pushstring(L, "blue-borderlands");      break;
    case 11: lua_pushstring(L, "green-borderlands");     break;
    case 12: lua_pushstring(L, "red-borderlands");       break;
    case 13: lua_pushstring(L, "wvw-reward");            break;
    case 14: lua_pushstring(L, "obsidian-sanctum");      break;
    case 15: lua_pushstring(L, "edge-of-the-mists");     break;
    case 16: lua_pushstring(L, "public-mini");           break;
    case 17: lua_pushstring(L, "big-battle");            break;
    case 18: lua_pushstring(L, "armistice-bastion");     break;
    default: lua_pushstring(L, "unknown");               break;
    }

    return 1;
}

/*** RST
.. lua:data:: map_id

    :type: integer

    The current MapID.

    .. versionhistory::
        :0.0.1: Added
*/

int mumble_link_lua_map_id(lua_State *L) {
    lua_pushinteger(L, ml->gw2_ml->context.map_id);
    return 1;
}

/*** RST
.. lua:data:: ui_state

    :type: integer

    This is a bit mask indicating various states of the game UI:

    ==== ==============================================================
    Bit  Description
    ==== ==============================================================
    1    The map is open/fullscreen
    2    The minimap is located at the top right of the screen
    3    The minimap has rotation enabled
    4    The game window has focus
    5    The game is currently in a competitive game mode? (PvP, WvW)
    6    A textbox has focus
    7    In combat
    ==== ==============================================================

    For example, with default settings and out of combat, when the game has focus this value will be 8 (bit 4). When the map is opened it will be 9 (bits 1 and 4).

    .. versionhistory::
        :0.0.1: Added
*/
int mumble_link_lua_ui_state(lua_State *L) {
    /*
    lua_createtable(L, 0, 7);
    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_MAP_OPEN);
    lua_setfield(L, -2, "map_open");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_COMPASS_TOP_RIGHT);
    lua_setfield(L, -2, "compass_top_right");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_COMPASS_ROTATE);
    lua_setfield(L, -2, "compass_rotate");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_GAME_FOCUS);
    lua_setfield(L, -2, "map_game_focus");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_COMP_MODE);
    lua_setfield(L, -2, "competitive_mode");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_TEXTBOX_FOCUS);
    lua_setfield(L, -2, "textbox_focus");

    lua_pushboolean(L, ml->gw2_ml->context.ui_state & MUMBLE_LINK_UI_STATE_IN_COMBAT);
    lua_setfield(L, -2, "in_combat");
    */
    lua_pushinteger(L, ml->gw2_ml->context.ui_state);

    return 1;
}

/*** RST
Data
----

.. lua:alias:: position = table

    A table holding x, y, and z coordinates.

    .. luatablefields::
        :x: number
        :y: number
        :z: number

    .. versionhistory::
        :0.0.1: Added
*/
